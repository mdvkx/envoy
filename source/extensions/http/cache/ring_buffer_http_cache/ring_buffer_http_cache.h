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


struct  RingBufferHttpCache
  : public HttpCache,
    public std::enable_shared_from_this<RingBufferHttpCache>  // TODO:  enable shared from this?
{
  using  Self = RingBufferHttpCache;

  static constexpr std::string_view  CACHE_INFO_NAME = "envoy.extensions.http.cache.ring_buffer";

  using  Key = int;
  using  Value = int;

  // TODO:
  // ring buffer

  // from  HttpCache
  [[nodiscard]]
  auto  makeLookupContext (
    LookupRequest                 && request,
    Http::StreamFilterCallbacks    & callbacks
  )
    -> LookupContextPtr
    override;

  [[nodiscard]]
  auto  makeInsertContext (
    LookupContextPtr              && lookup_context,
    Http::StreamFilterCallbacks    & callbacks
  )
    -> InsertContextPtr
    override;

  auto  updateHeaders (
    const LookupContext            & lookup_context,
    const Http::ResponseHeaderMap  & response_headers,
    const ResponseMetadata         & metadata,
    UpdateHeadersCallback            on_complete
  )
    -> void
    override;

  [[nodiscard]]
  auto  cacheInfo ( ) const
    -> CacheInfo
    override;

  // ----------------------------------------------------

  [[nodiscard]]
  auto  lookup ( const Key  & key ) const
    -> Value *;

  [[nodiscard]]
  auto  insert ( const Key  & key, Value  value )
    -> bool;

  [[nodiscard]]
  auto  insert ( const Key  & key, const std::function<Value ()>  & lazy )
    -> bool;

};

struct  RingBufferHttpCacheFactory : public HttpCacheFactory
{
  using  Self = RingBufferHttpCacheFactory;

  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  name ( ) const
    -> std::string
    override
  {
    return  std::string { RingBufferHttpCache::CACHE_INFO_NAME };
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  createEmptyConfigProto ( )
    -> ProtobufTypes::MessagePtr
    override
  {
    return  std::make_unique<ProtobufWkt::Empty>  ();
    //return  std::make_unique<envoy::extensions::http::cache::simple_http_cache::v3::RingBufferHttpCacheConfig>  ();
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  getCache (
    [[maybe_unused]]
    const envoy::extensions::filters::http::cache::v3::CacheConfig   & config,  // the fuck?
    Server::Configuration::FactoryContext                            & context
  )
    -> std::shared_ptr<HttpCache>
    override
  {
    return  std::make_shared<RingBufferHttpCache> ();
  }
};

} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
