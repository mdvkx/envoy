
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
    // probe cache for `m_Request . key ()`.
    // if found:  create `LookupResult` and pass it to the %(callback)
    // not found
    m_Dispatcher . post ( [ callback = std::move ( callback ), stop = &m_Stop ] ( ) mutable
      -> void
    {
      if ( !(*stop) ) {
        std::move ( callback ) ( LookupResult {}, /* ???= */ false );
      }
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
    if ( auto  cache = m_Cache . lock () )
      assert ( 0 );
    else
      assert ( 0 );
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
  -> Value *
{
  assert ( 0 );
}
// --------------------------------------------------------------------------
[[nodiscard]]
auto  RingBufferHttpCache::insert ( const Key  & key, Value  value )
  -> bool
{
  assert ( 0 );
}
// --------------------------------------------------------------------------
[[nodiscard]]
auto  RingBufferHttpCache::insert ( const Key  & key, absl::AnyInvocable<Value ()>  lazy )
  -> bool
{
  assert ( 0 );
}



} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
