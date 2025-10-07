#pragma once

#include <cassert>

#include "source/common/protobuf/protobuf.h"  // ProtobufWkt
#include "source/extensions/filters/http/cache/http_cache.h"  // HttpCache

// --------------------------------------------------------------------------

namespace Envoy
{
namespace Extensions
{
namespace HttpFilters
{
namespace Cache
{


struct  RingBufferLookupContext : public LookupContext
{

  // ------------------------------------------------------------------------
  auto  getHeaders ( LookupHeadersCallback  && callback )
    -> void
    override
  {
    // probe cache for `m_Request . key ()`.
    // if found:  create `LookupResult` and pass it to the %(callback)
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto  getTrailers ( LookupTrailersCallback  && callback )
    -> void
    override
  {
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto  getBody (
    const AdjustedByteRange  & range,
    LookupBodyCallback      && callback
  )
    -> void
    override
  {
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto  onDestroy ( )
    -> void
    override
  {
    PANIC ( "not yet implemented!" );
  }
};

struct  RingBufferInsertContext : public InsertContext
{

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
    PANIC ( "not yet implemented!" );
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
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto insertTrailers (
    const Http::ResponseTrailerMap  & trailers,
    InsertCallback                    insert_complete
  )
    -> void
    override
  {
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto onDestroy ( )
    -> void
    override
  {
    PANIC ( "not yet implemented!" );
  }
};


struct  RingBufferHttpCache : public HttpCache
{
  using  Self = RingBufferHttpCache;

  static constexpr std::string_view  CACHE_INFO_NAME = "envoy.extensions.http.cache.ring_buffer";

  // TODO:
  // ring buffer

  // ------------------------------------------------------------------------

  auto  makeLookupContext (
    LookupRequest                 && request,
    Http::StreamFilterCallbacks    & callbacks
  )
    -> LookupContextPtr
    override
  {
    return  std::make_unique<RingBufferLookupContext> ();
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto  makeInsertContext (
    LookupContextPtr              && lookup_context,
    Http::StreamFilterCallbacks    & callbacks
  )
    -> InsertContextPtr
    override
  {
    return  std::make_unique<RingBufferInsertContext> ();
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto  updateHeaders (
    const LookupContext            & lookup_context,
    const Http::ResponseHeaderMap  & response_headers,
    const ResponseMetadata         & metadata,
    UpdateHeadersCallback            on_complete
  )
    -> void
    override
  {
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto  cacheInfo ( ) const
    -> CacheInfo
    override
  {
    using namespace std::string_view_literals;
    return  CacheInfo { . name_ = Self::CACHE_INFO_NAME };
    PANIC ( "not yet implemented!" );
  }
};

struct  RingBufferHttpCacheFactory : public HttpCacheFactory
{
  using  Self = RingBufferHttpCacheFactory;

  // ------------------------------------------------------------------------
  auto  name ( ) const
    -> std::string
    override
  {
    return  std::string { RingBufferHttpCache::CACHE_INFO_NAME };
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto  createEmptyConfigProto ( )
    -> ProtobufTypes::MessagePtr
    override
  {
    return  std::make_unique<ProtobufWkt::Empty>  ();
    //return  std::make_unique<envoy::extensions::http::cache::simple_http_cache::v3::RingBufferHttpCacheConfig>  ();
    PANIC ( "not yet implemented!" );
  }
  // ------------------------------------------------------------------------
  auto  getCache (
    [[maybe_unused]]
    const envoy::extensions::filters::http::cache::v3::CacheConfig   & config,  // the fuck?
    Server::Configuration::FactoryContext                            & context
  )
    -> std::shared_ptr<HttpCache>
    override
  {
    return  std::make_shared<RingBufferHttpCache> ();
    PANIC ( "not yet implemented!" );
  }
};


} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
