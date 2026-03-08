//#include "source/extensions/http/cache/ring_buffer_http_cache/ring_buffer_http_cache.h"
#include "./ring_buffer_http_cache.h"
#include "envoy/registry/registry.h"  // Registry::RegisterFactory
#include "source/common/buffer/buffer_impl.h"  // Buffer::Instance
#include "source/common/http/header_map_impl.h"  //
#include "source/common/protobuf/protobuf.h"  // ProtobufWkt::Empty
#include "source/extensions/filters/http/cache/http_cache.h"  // InsertContext, LookupContext
#include <cassert>  // assert
#include <memory>  // shared_ptr, unique_ptr, weak_ptr
#include <optional>  // optional
#include <stdexcept>  // invalid_argument, runtime_error
#include <utility>  // move
namespace  Envoy::Extensions::HttpFilters::Cache {
// --------------------------------------------------------------------------
struct  RingBufferHttpCacheLookupContext : public LookupContext
{
    RingBufferHttpCacheLookupContext ( Event::Dispatcher & dispatcher,
                                       std::shared_ptr<RingBufferHttpCache>  cache,
                                       LookupRequest && request )
        : m_Dispatcher { dispatcher }
        , m_Cache { cache }
        , m_Request { std::move ( request ) }
  {
  }

  Event::Dispatcher & m_Dispatcher;
  std::weak_ptr<RingBufferHttpCache>  m_Cache;
  LookupRequest  m_Request;

  std::optional<Response>  m_Response;

  auto  getHeaders      ( LookupHeadersCallback && callback ) -> void override
  {
    assert ( !m_Response . has_value () );  // "it is a programming error to call this method twice", doesn't cover it 100%, but it's something
    auto  cache = m_Cache . lock ();
    if ( !cache )
      throw  std::runtime_error { "lookup context outlived the cache that created it" };
    m_Response = cache -> lookup ( m_Request );
    auto  result = m_Response . has_value () ? m_Request . makeLookupResult ( std::move ( m_Response -> m_Headers ), std::move ( m_Response -> m_Metadata ), m_Response -> m_Body . length () ) : LookupResult {};
    m_Dispatcher . post ( [ callback = std::move ( callback ), result = std::move ( result ), is_last = !m_Response . has_value () || ( m_Response -> m_Body . empty () && m_Response -> m_Trailers == nullptr ) ] ( ) mutable -> void
    {
      (std::move ( callback )) ( std::move ( result ), is_last );
    } );
  }
  auto  getBody         ( const AdjustedByteRange & range,
                          LookupBodyCallback && callback ) -> void override
  {
    assert ( m_Response . has_value () );
    assert ( range . end () <= m_Response -> m_Body . length () );
    auto  result = std::make_unique<Buffer::OwnedImpl> ( std::string_view { m_Response -> m_Body } . substr ( range . begin (), range . length () ) );
    m_Dispatcher . post ( [ callback = std::move ( callback ), result = std::move ( result ), is_last = range . end () == m_Response -> m_Body . length () && m_Response -> m_Trailers == nullptr  ] ( ) mutable -> void
    {
      (std::move ( callback )) ( std::move ( result ), is_last );
    } );
  }
  auto  getTrailers     ( LookupTrailersCallback && callback ) -> void override
  {
    assert ( m_Response . has_value () );
    assert ( m_Response -> m_Trailers != nullptr );
    auto  result = std::move ( m_Response -> m_Trailers );
    m_Dispatcher . post ( [ callback = std::move ( callback ), result = std::move ( result ) ] ( ) mutable -> void
    {
      (std::move ( callback )) ( std::move ( result ) );
    } );
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

    RingBufferHttpCacheInsertContext ( Event::Dispatcher & dispatcher,
                                       std::shared_ptr<RingBufferHttpCache>  cache,
                                       std::unique_ptr<RingBufferHttpCacheLookupContext> && lookup )
        : m_Dispatcher { dispatcher }
        , m_Cache { cache }
        , m_Lookup { std::move ( lookup ) }
  {
  }
  auto  post ( ) -> void
  {
    assert ( 0 );
  }
  auto  commit ( ) -> bool
  {
    // TODO:  construct a Response from individual pieces, insert into the cache
    auto  response = Response { std::move ( m_Headers ), std::move ( m_Trailers ), std::move ( m_Metadata ), std::move ( m_Body ) };
    auto  cache = m_Cache . lock ();
    if ( !cache )
      throw  std::runtime_error { "insert context outlived the cache that created it" };
    cache -> insert ( *m_Lookup, std::move ( response ) );
    assert ( 0 );
  }

  Event::Dispatcher & m_Dispatcher;
  std::weak_ptr<RingBufferHttpCache>  m_Cache;
  std::unique_ptr<RingBufferHttpCacheLookupContext>  m_Lookup;

  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  ResponseMetadata  m_Metadata {};
  std::string  m_Body = "";

  auto  insertHeaders   ( const Http::ResponseHeaderMap & headers,
                          const ResponseMetadata & metadata,
                          InsertCallback  callback,
                          bool  is_last ) -> void override
  {
    m_Headers  = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
    m_Metadata = metadata;
    if ( is_last )
      this -> commit ();
    m_Dispatcher . post ( [ callback = std::move ( callback ) ] ( ) mutable -> void
    {
      (std::move ( callback )) ( true );
    } );
  }
  auto  insertBody      ( const Buffer::Instance & fragment,
                          InsertCallback  callback,
                          bool  is_last ) -> void override
  {
    m_Body += fragment . toString ();  // TODO: inefficient? maybe use envoy's Buffer::* api instead
    if ( is_last )
      this -> commit ();
    m_Dispatcher . post ( [ callback = std::move ( callback ) ] ( ) mutable -> void
    {
      (std::move ( callback )) ( true );
    } );
  }
  auto  insertTrailers  ( const Http::ResponseTrailerMap & trailers,
                          InsertCallback  callback ) -> void override
  {
    m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
    if ( is_last )
      this -> commit ();
    m_Dispatcher . post ( [ callback = std::move ( callback ) ] ( ) mutable -> void
    {
      (std::move ( callback )) ( true );
    } );
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
  return  CacheInfo { . name_ = CACHE_NAME };
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
  // a kind of dynamic_pointer_cast for unique ptr
  auto  * p = dynamic_cast<RingBufferHttpCacheLookupContext *> ( lookup . get () );
  if ( p == nullptr )
    throw  std::invalid_argument { "mismatch in lookup context type" };
  lookup . release ();
  return  std::make_unique<RingBufferHttpCacheInsertContext> ( callbacks . dispatcher (), this -> shared_from_this (), std::unique_ptr<RingBufferHttpCacheLookupContext> { p } );
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
  return  std::nullopt;
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
auto  RingBufferHttpCacheFactory::createEmptyConfigProto ( ) -> std::unique_ptr<Protobuf::Message>
{
  return  std::make_unique<ProtobufWkt::Empty> ();  // TODO: custom protobuf config type
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCacheFactory::getCache ( const envoy::extensions::filters::http::cache::v3::CacheConfig & , Server::Configuration::FactoryContext &  ) -> std::shared_ptr<HttpCache>
{
  return  std::make_shared<RingBufferHttpCache> ();
}
}
