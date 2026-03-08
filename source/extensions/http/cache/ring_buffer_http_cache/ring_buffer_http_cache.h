#pragma once

#include "source/common/protobuf/protobuf.h"  // Protobuf::Message
#include "source/common/protobuf/utility.h"  // MessageUtil
#include "source/extensions/filters/http/cache/http_cache.h"  // HttpCache
#include <memory>  // shared_ptr, unique_ptr, enable_shared_from_this
#include <optional>  // optional
#include <string>  // string
#include <string_view>  // string_view

// notes:  i dont like aliases for things like unique_ptr<X>, i find them opaque, so i dont use them

namespace  Envoy::Extensions::HttpFilters::Cache {

using namespace  std::literals;  // ""sv

struct  Response
{
  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  ResponseMetadata  m_Metadata {};
  std::string  m_Body = "";
};

/*
struct  Entry
{
  std::shared_future<Response>  m_Response;
};
*/

struct  RingBufferHttpCache : public HttpCache, public std::enable_shared_from_this<RingBufferHttpCache>
{
  using  Self   = RingBufferHttpCache;

  using  Key    = LookupRequest;
  using  Value  = Response;

  static constexpr auto  CACHE_NAME = "envoy.extensions.http.cache.ring_buffer_http_cache"sv;

  mutable std::mutex  m_Mtx;
  std::unordered_map
  < Envoy::Extensions::HttpFilters::Cache::Key  // generated via protobuf, see key.pb.h; fqn because Key is also a member type
   , Value
   , MessageUtil
   , MessageUtil
   >  m_Cache;  // TODO: ring buffer

  auto  cacheInfo ( ) const -> CacheInfo override;
  auto  makeLookupContext ( LookupRequest && request,
                            Http::StreamFilterCallbacks & callbacks ) -> std::unique_ptr<LookupContext> override;
  auto  makeInsertContext ( std::unique_ptr<LookupContext> && lookup,
                            Http::StreamFilterCallbacks & callbacks ) -> std::unique_ptr<InsertContext> override;
  auto  updateHeaders ( const LookupContext & lookup,
                        const Http::ResponseHeaderMap & headers,
                        const ResponseMetadata & metadata,
                        UpdateHeadersCallback callback ) -> void override;

  auto  lookup ( const Key & key ) const -> std::optional<Value>;
  auto  contains ( const Key & key ) const -> bool;
  auto  insert ( const Key & key,
                 Value && value ) -> void;
};

struct  RingBufferHttpCacheFactory : public HttpCacheFactory
{
  // UntypedFactory
  auto  name ( ) const -> std::string override;
  // TypedFactory
  auto  createEmptyConfigProto ( ) -> std::unique_ptr<Protobuf::Message> override;
  // HttpCacheFactory
  auto  getCache ( const envoy::extensions::filters::http::cache::v3::CacheConfig & , Server::Configuration::FactoryContext &  ) -> std::shared_ptr<HttpCache> override;
};

}
