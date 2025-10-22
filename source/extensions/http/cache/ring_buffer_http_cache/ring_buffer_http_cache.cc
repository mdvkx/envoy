
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
  LookupRequest  m_Request;
  //  simple http cache uses std::shared_ptr<bool>, but i dunno why, it doesn't make it thread safe
  bool  m_Stop = false;


  explicit  RingBufferLookupContext (
    Event::Dispatcher   & dispatcher,
    LookupRequest      && request
  )
    : m_Dispatcher  { dispatcher },
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


  auto  insertHeaders (
    const Http::ResponseHeaderMap  & response_headers,
    const ResponseMetadata         & metadata,
    InsertCallback                   insert_complete,
    bool                             end_stream
  )
    -> void
    override
  {

    assert ( 0 );
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
    assert ( 0 );
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
  }
  // ------------------------------------------------------------------------
  auto onDestroy ( )
    -> void
    override
  {
    assert ( 0 );
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
  return  std::make_unique<RingBufferLookupContext> ( callbacks . dispatcher (), std::move ( request ) );
}
// --------------------------------------------------------------------------
[[nodiscard]]
auto  RingBufferHttpCache::makeInsertContext (
  LookupContextPtr              && lookup_context,
  Http::StreamFilterCallbacks    & callbacks
)
  -> InsertContextPtr
{
  return  std::make_unique<RingBufferInsertContext> ();
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
auto  RingBufferHttpCache::lookup ( const LookupRequest  & request ) const
  -> bool
{
  assert ( 0 );
}



} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
