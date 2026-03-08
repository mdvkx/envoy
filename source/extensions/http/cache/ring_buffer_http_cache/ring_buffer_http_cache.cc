//#include "source/extensions/http/cache/ring_buffer_http_cache/ring_buffer_http_cache.h"
#include "./ring_buffer_http_cache.h"
#include "envoy/registry/registry.h"  // Registry::RegisterFactory
#include "source/common/buffer/buffer_impl.h"  // Buffer::Instance
#include "source/common/http/header_map_impl.h"  //
#include "source/common/protobuf/protobuf.h"  // ProtobufWkt::Empty
#include "source/extensions/filters/http/cache/http_cache.h"  // InsertContext, LookupContext
#include <cassert>  // assert
#include <memory>  // shared_ptr, unique_ptr, weak_ptr
#include <utility>  // move
// --------------------------------------------------------------------------
struct  RingBufferHttpCacheLookupContext : public LookupContext
{
  Event::Dispatcher & m_Dispatcher;
  std::weak_ptr<RingBufferHttpCache>  m_Cache;
  LookupRequest  m_Request;
        RingBufferHttpCacheLookupContext ( Event::Dispatcher & dispatcher,
                                           std::shared_ptr<RingBufferHttpCache>  cache,
                                           LookupRequest && request )
        : m_Dispatcher { dispatcher }
        , m_Cache { cache }
        , m_Request { std::move ( request ) }
  {
  }
  auto  getHeaders      ( LookupHeadersCallback && callback ) -> void override
  {
    assert ( 0 );
  }
  auto  getBody         ( const AdjustedByteRange & range,
                          LookupBodyCallback && callback ) -> void override
  {
    assert ( 0 );
  }
  auto  getTrailers     ( LookupTrailersCallback && callback ) -> void override
  {
    assert ( 0 );
  }
  // "any async activities are cleaned up before returning from `onDestroy()`. (...) `onDestroy()`
  // should cancel any outstanding async operations and, if necessary, it should block on that
  // cancellation to avoid data races."
  auto  onDestroy       ( ) -> void override
  {
    assert ( 0 );
  }
};
// --------------------------------------------------------------------------
struct  RingBufferHttpCacheInsertContext : public InsertContext
{
  Event::Dispatcher & m_Dispatcher;
  std::weak_ptr<RingBufferHttpCache>  m_Cache;
  std::unique_ptr<RingBufferHttpCacheLookupContext>  m_Lookup;
        RingBufferHttpCacheInsertContext ( Event::Dispatcher & dispatcher,
                                           std::shared_ptr<RingBufferHttpCache>  cache,
                                           std::unique_ptr<RingBufferHttpCacheLookupContext> && lookup )
        : m_Dispatcher { dispatcher }
        , m_Cache { cache }
        , m_Lookup { std::move ( lookup ) }
  {
  }
  auto  insertHeaders   ( const Http::ResponseHeaderMap & headers,
                          const ResponseMetadata & metadata,
                          InsertCallback  callback,
                          bool  eof ) -> void override
  {
    assert ( 0 );
  }
  auto  insertBody      ( const Buffer::Instance & fragment,
                          InsertCallback  callback,
                          bool  eof ) -> void override
  {
    assert ( 0 );
  }
  auto  insertTrailers  ( const Http::ResponseTrailerMap & trailers,
                          InsertCallback  callback ) -> void override
  {
    assert ( 0 );
  }
  // "any async activities are cleaned up before returning from `onDestroy()`. (...) `onDestroy()`
  // should cancel any outstanding async operations and, if necessary, it should block on that
  // cancellation to avoid data races."
  auto  onDestroy       ( ) -> void override
  {
    assert ( 0 );
  }
};
// --------------------------------------------------------------------------
namespace
{
  auto  g_SelfRegister = Registry::RegisterFactory<RingBufferHttpCacheFactory, HttpCacheFactory> {};
}
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::cacheInfo ( ) const -> CacheInfo
{
  assert ( 0 );
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::makeLookupContext ( LookupRequest && request,
                                               Http::StreamFilterCallbacks & callbacks ) -> std::unique_ptr<LookupContext>
{
  return  std::make_unique<RingBufferHttpCacheLookupContext> ( callbacks . dispatcher (), this -> shared_from_this (), std::move ( request ) );
  assert ( 0 );
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::makeInsertContext ( std::unique_ptr<LookupContext> && lookup,
                                               Http::StreamFilterCallbacks & callbacks ) -> std::unique_ptr<InsertContext>
{
  return  std::make_unique<RingBufferHttpCacheInsertContext> ( callbacks . dispatcher (), this -> shared_from_this (), std::move ( lookup ) );
  assert ( 0 );
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::updateHeaders ( const LookupContext & lookup,
                                           const Http::ResponseHeaderMap & headers,
                                           const ResponseMetadata & metadata,
                                           UpdateHeadersCallback callback ) -> void
{
  assert ( 0 );
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::lookup ( const Key & key ) const -> std::optional<Value>
{
  assert ( 0 );
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::contains ( const Key & key ) const -> bool
{
  assert ( 0 );
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::insert ( const Key & key,
                                    Value && value ) -> void
{
  assert ( 0 );
}
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
auto  RingBufferHttpCacheFactory::name ( ) const -> std::string
{
  return  std::string { RingBufferHttpCache::CACHE_NAME };
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCacheFactory::createEmptyConfigProto ( ) -> std::unique_ptr<ProtobufTypes::Message>
{
  return  std::make_unique<ProtobufWkt::Empty> ();  // TODO: custom protobuf config type
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCacheFactory::getCache ( const envoy::extensions::filters::http::cache::v3::CacheConfig & , Server::Configuration::FactoryContext &  ) -> std::shared_ptr<HttpCache>
{
  return  std::make_shared<RingBufferHttpCache> ();
}
