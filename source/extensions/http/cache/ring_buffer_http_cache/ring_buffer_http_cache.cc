
#include "source/extensions/http/cache/ring_buffer_http_cache/ring_buffer_http_cache.h"

// --------------------------------------------------------------------------

namespace Envoy
{
namespace Extensions
{
namespace HttpFilters
{
namespace Cache
{

namespace
{

auto  g_Register  = Registry::RegisterFactory<RingBufferHttpCacheFactory, HttpCacheFactory>  ();

} // namespace


///  represents a single lookup operation.
struct  RingBufferLookupContext : public LookupContext
{
  Event::Dispatcher  & m_Dispatcher;
  std::weak_ptr<RingBufferHttpCache>  m_Cache;
  LookupRequest  m_Request;
  //  simple http cache uses std::shared_ptr<bool>, but i dunno why, it doesn't make it thread safe
  bool  m_Stop = false;

  std::optional<Response>  m_Response;  // nice to have:  lazy initialize? eg. Lazy<Response>


        RingBufferLookupContext (
    Event::Dispatcher                      & dispatcher,
    std::shared_ptr<RingBufferHttpCache>     cache,
    LookupRequest                         && request
  )
    : m_Dispatcher  { dispatcher },
      m_Cache       { cache },
      m_Request     { std::move ( request ) }
  {
  }
  // ------------------------------------------------------------------------
  auto  getHeaders ( LookupHeadersCallback  && callback )
    -> void
    override
  {
    m_Response = [ & ] ( ) {
      auto  cache = m_Cache . lock ();
      assert ( cache != nullptr );
      return  cache -> lookup ( m_Request );
    } ();

    auto  result = [ & ] ( ) {
      if ( m_Response )
        return  m_Request . makeLookupResult ( std::move ( m_Response -> m_ResponseHeaders ), std::move ( m_Response -> m_ResponseMetadata ), m_Response -> m_ResponseBody . length () );
      else
        return  LookupResult {};
    } ();

    m_Dispatcher . post ( [
      callback  = std::move ( callback ),
      result    = std::move ( result ),
      stop      = &m_Stop,  // TODO:  not thread-safe
      eof       = m_Response -> m_ResponseBody . empty () && m_Response -> m_ResponseTrailers == nullptr
    ] ( ) mutable
      -> void
    {
      if ( !(*stop) )  // TODO:  not thread-safe
        std::move ( callback ) ( std::move ( result ), /* ???= */ false );
    } );
  }
  // ------------------------------------------------------------------------
  auto  getTrailers ( LookupTrailersCallback  && callback )
    -> void
    override
  {
    assert ( 0 );
  }
  // ------------------------------------------------------------------------
  auto  getBody (
    const AdjustedByteRange  & range,
    LookupBodyCallback      && callback
  )
    -> void
    override
  {
    assert ( 0 );
  }
  // ------------------------------------------------------------------------
  auto  onDestroy ( )
    -> void
    override
  {
    m_Stop = true;
  }
};

///  represents a single insert operation.
struct  RingBufferInsertContext : public InsertContext
{
  Event::Dispatcher          & m_Dispatcher;
  std::weak_ptr<RingBufferHttpCache>  m_Cache;
  std::unique_ptr<RingBufferLookupContext>  m_Lookup;  // yes? no? -??? at least  i don't dangle
  bool                         m_Stop = false;

  Http::ResponseHeaderMapPtr   m_ResponseHeaders  = nullptr;
  Http::ResponseTrailerMapPtr  m_ResponseTrailers = nullptr;
  ResponseMetadata             m_ResponseMetadata;
  std::string                  m_ResponseBody;

  // ------------------------------------------------------------------------
        RingBufferInsertContext (
    Event::Dispatcher                          & dispatcher,
    std::shared_ptr<RingBufferHttpCache>         cache,
    std::unique_ptr<RingBufferLookupContext>  && lookup
  )
    : m_Dispatcher       { dispatcher }
    , m_Cache            { cache }
    , m_Lookup           { std::move ( lookup ) }
  {
  }
  // ------------------------------------------------------------------------
  auto  commit ( )
    -> void
  {
    // response is complete, insert into cache
    auto  cache = m_Cache . lock ();
    assert ( cache != nullptr );
    cache -> insert ( m_Lookup -> m_Request, [ this ] ( ) mutable
      -> Response
    {
      return  Response { std::move ( m_ResponseHeaders ), std::move ( m_ResponseTrailers ), std::move ( m_ResponseMetadata ), std::move ( m_ResponseBody ) };
    } );
  }
  // ------------------------------------------------------------------------
  auto  insertHeaders (
    const Http::ResponseHeaderMap  & response_headers,
    const ResponseMetadata         & metadata,
    InsertCallback                   insert_complete,
    bool                             end_stream
  )
    -> void
    override
  {
    m_ResponseHeaders   = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( response_headers );
    m_ResponseMetadata  = metadata;
    if ( end_stream )
      this -> commit ();
    m_Dispatcher . post ( [ func = std::move ( insert_complete ) ] ( ) mutable
      -> void
    {
      std::move ( func ) ( /* success: */ true );
    } );
  }
  // ------------------------------------------------------------------------
  auto  insertBody (
    const Buffer::Instance  & fragment,
    InsertCallback            ready_for_next_fragment,
    bool                      end_stream
  )
    -> void
    override
  {
    m_ResponseBody += fragment . toString ();
    std::cout << "\"" << m_ResponseBody << "\"\n";
    std::cout . flush ();
    if ( end_stream )
      this -> commit ();
    m_Dispatcher . post ( [ func = std::move ( ready_for_next_fragment ) ] ( ) mutable
      -> void
    {
      std::move ( func ) ( /* success: */ true );
    } );
  }
  // ------------------------------------------------------------------------
  auto insertTrailers (
    const Http::ResponseTrailerMap  & trailers,
    InsertCallback                    insert_complete
  )
    -> void
    override
  {
    assert ( 0 );
    m_ResponseTrailers  = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
    this -> commit ();
    m_Dispatcher . post ( [ func = std::move ( insert_complete ) ] ( ) mutable
      -> void
    {
      std::move ( func ) ( /* success: */ true );
    } );
  }
  // ------------------------------------------------------------------------
  auto onDestroy ( )
    -> void
    override
  {
    m_Stop = true;
  }
};

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
[[nodiscard]]
auto  RingBufferHttpCache::makeLookupContext (
  LookupRequest                 && request,
  Http::StreamFilterCallbacks    & callbacks
)
  -> LookupContextPtr
{
  return  std::make_unique<RingBufferLookupContext> ( callbacks . dispatcher (), this -> shared_from_this (), std::move ( request ) );
}
// --------------------------------------------------------------------------
[[nodiscard]]
auto  RingBufferHttpCache::makeInsertContext (
  LookupContextPtr              && lookup_context,
  Http::StreamFilterCallbacks    & callbacks
)
  -> InsertContextPtr
{
  // ughhhhh
  auto  lookup = [ & ] ( ) {
    const auto  tmp = dynamic_cast<RingBufferLookupContext *> ( lookup_context . release () );
    assert ( tmp );
    auto  lookup = std::unique_ptr<RingBufferLookupContext> ( std::move ( tmp ) );
    return  lookup;
  } ();

  return  std::make_unique<RingBufferInsertContext> ( callbacks . dispatcher (), this -> shared_from_this (), std::move ( lookup ) );
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::updateHeaders (
  const LookupContext            & lookup_context,
  const Http::ResponseHeaderMap  & response_headers,
  const ResponseMetadata         & metadata,
  UpdateHeadersCallback            on_complete
)
  -> void
{
  assert ( 0 );
}
// --------------------------------------------------------------------------
[[nodiscard]]
auto  RingBufferHttpCache::cacheInfo ( ) const
  -> CacheInfo
{
  return  CacheInfo { . name_ = Self::CACHE_INFO_NAME };
}
// --------------------------------------------------------------------------
[[nodiscard]]
auto  RingBufferHttpCache::lookup ( const Key  & key ) const
  -> std::optional<Value>
{
  const auto  & headers = key . requestHeaders ();
  const auto  q = absl::StrCat ( headers . getHostValue (), headers . getPathValue () );
  const auto  entry = m_Buffer . lookup ( [ & ] ( const auto  & p )
    -> bool
  {
    return  q == p . first;
  } );
  if ( entry == nullptr )
    return  std::nullopt;
  // copy
  const auto  & [ _, value ] = *entry;
  return  Response {
    Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *value . m_ResponseHeaders ),
    value . m_ResponseTrailers ? Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( *value . m_ResponseTrailers ) : nullptr,
    value . m_ResponseMetadata,
    value . m_ResponseBody
  };
}
// --------------------------------------------------------------------------
[[nodiscard]]
auto  RingBufferHttpCache::insert ( const Key  & key, Value  value )
  -> bool
{
  return  this -> insert ( key, [ value = std::move ( value ) ] ( ) mutable
    -> Value
  {
    return  std::move ( value );  // rvo doesn't pick it up. weird, but ok
    assert ( 0 );
  } );
}
// --------------------------------------------------------------------------
[[nodiscard]]
auto  RingBufferHttpCache::insert ( const Key  & key, absl::AnyInvocable<Value ()>  lazy )
  -> bool
{
  const auto  & headers = key . requestHeaders ();
  const auto  q = absl::StrCat ( headers . getHostValue (), headers . getPathValue () );
  std::cout << q << "\n";
  std::cout . flush ();
  m_Buffer . push ( q, lazy () );
  return  true;
}



} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
