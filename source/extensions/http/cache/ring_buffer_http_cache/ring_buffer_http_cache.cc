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
template <typename  F_, typename  Signature_>
struct  is_signature_invocable : std::false_type { };
template <typename  F_, typename  R_, typename ...  Args_>
struct  is_signature_invocable <F_, R_ (Args_ ...)> : std::bool_constant<std::is_invocable_r_v<R_, F_, Args_ ...> > { };
template <typename  F_, typename  Signature_>
constexpr auto  is_signature_invocable_v = is_signature_invocable<F_, Signature_>::value;
template <typename  F_, typename  Signature_>
concept  Fn = std::is_function_v<Signature_> && is_signature_invocable_v<F_, Signature_>;
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
  template <Fn<void ()>  F_>
  auto  post ( F_ && f ) -> void
  {
    m_Dispatcher . post ( [ f = std::move ( f ), done = std::cref ( m_Done ) ] ( ) mutable -> void
    {
      if ( !done )  (std::move ( f )) ();
    } );
  }

  Event::Dispatcher & m_Dispatcher;
  std::weak_ptr<RingBufferHttpCache>  m_Cache;
  LookupRequest  m_Request;

  std::optional<Response>  m_Response;

  bool  m_Done = false;

  auto  getHeaders      ( LookupHeadersCallback && callback ) -> void override
  {
    assert ( !m_Response . has_value () );  // "it is a programming error to call this method twice", doesn't cover it 100%, but it's something
    auto  cache = m_Cache . lock ();
    if ( !cache )
      throw  std::runtime_error { "lookup context outlived the cache that created it" };
    m_Response = cache -> lookup ( m_Request );
    auto  result = m_Response . has_value () ? m_Request . makeLookupResult ( std::move ( m_Response -> m_Headers ), std::move ( m_Response -> m_Metadata ), m_Response -> m_Body . length () ) : LookupResult {};
    this -> post ( [ callback = std::move ( callback ), result = std::move ( result ), is_last = !m_Response . has_value () || ( m_Response -> m_Body . empty () && m_Response -> m_Trailers == nullptr ) ] ( ) mutable -> void
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
    this -> post ( [ callback = std::move ( callback ), result = std::move ( result ), is_last = range . end () == m_Response -> m_Body . length () && m_Response -> m_Trailers == nullptr ] ( ) mutable -> void
    {
      (std::move ( callback )) ( std::move ( result ), is_last );
    } );
  }
  auto  getTrailers     ( LookupTrailersCallback && callback ) -> void override
  {
    assert ( m_Response . has_value () );
    assert ( m_Response -> m_Trailers != nullptr );
    auto  result = std::move ( m_Response -> m_Trailers );
    this -> post ( [ callback = std::move ( callback ), result = std::move ( result ) ] ( ) mutable -> void
    {
      (std::move ( callback )) ( std::move ( result ) );
    } );
  }
  // "any async activities are cleaned up before returning from `onDestroy()`. (...) `onDestroy()`
  // should cancel any outstanding async operations and, if necessary, it should block on that
  // cancellation to avoid data races."
  auto  onDestroy       ( ) -> void override
  {
    m_Done = true;
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
  template <Fn<void ()>  F_>
  auto  post ( F_ && f ) -> void
  {
    m_Dispatcher . post ( [ f = std::move ( f ), done = std::cref ( m_Done ) ] ( ) mutable -> void
    {
      if ( !done )  (std::move ( f )) ();
    } );
  }
  auto  commit ( ) -> bool
  {
    auto  cache = m_Cache . lock ();
    if ( !cache )
      return  false; // throw  std::runtime_error { "insert context outlived the cache that created it" };
    // TODO:  construct a Response from individual pieces, insert into the cache
    auto  response = Response { std::move ( m_Headers ), std::move ( m_Trailers ), std::move ( m_Metadata ), std::move ( m_Body ) };
    cache -> insert ( m_Lookup -> m_Request, std::move ( response ) );
    return  true;
  }

  Event::Dispatcher & m_Dispatcher;
  std::weak_ptr<RingBufferHttpCache>  m_Cache;
  std::unique_ptr<RingBufferHttpCacheLookupContext>  m_Lookup;

  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  ResponseMetadata  m_Metadata {};
  std::string  m_Body = "";

  bool  m_Done = false;  // TODO:  a separate lifetime?

  auto  insertHeaders   ( const Http::ResponseHeaderMap & headers,
                          const ResponseMetadata & metadata,
                          InsertCallback  callback,
                          bool  is_last ) -> void override
  {
    m_Headers  = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
    m_Metadata = metadata;
    auto  success = !is_last || this -> commit ();
    this -> post ( [ callback = std::move ( callback ), success ] ( ) mutable -> void
    {
      (std::move ( callback )) ( success );
    } );
  }
  auto  insertBody      ( const Buffer::Instance & fragment,
                          InsertCallback  callback,
                          bool  is_last ) -> void override
  {
    m_Body += fragment . toString ();  // TODO: inefficient? maybe use envoy's Buffer::* api instead
    auto  success = !is_last || this -> commit ();
    this -> post ( [ callback = std::move ( callback ), success ] ( ) mutable -> void
    {
      (std::move ( callback )) ( success );
    } );
  }
  auto  insertTrailers  ( const Http::ResponseTrailerMap & trailers,
                          InsertCallback  callback ) -> void override
  {
    m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
    auto  success = this -> commit ();
    this -> post ( [ callback = std::move ( callback ), success ] ( ) mutable -> void
    {
      (std::move ( callback )) ( success );
    } );
  }
  // "any async activities are cleaned up before returning from `onDestroy()`. (...) `onDestroy()`
  // should cancel any outstanding async operations and, if necessary, it should block on that
  // cancellation to avoid data races."
  auto  onDestroy       ( ) -> void override
  {
    m_Done = true;
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
  return  CacheInfo { . name_ = Self::CACHE_NAME };
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::makeLookupContext ( LookupRequest && request,
                                               Http::StreamFilterCallbacks & callbacks ) -> std::unique_ptr<LookupContext>
{
  return  std::make_unique<RingBufferHttpCacheLookupContext> ( callbacks . dispatcher (), this -> shared_from_this (), std::move ( request ) );
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
  auto  l = std::unique_lock { m_Mtx };
  auto  i = m_Cache . find ( key . key () );  // TODO:  key key is a bit weird

  assert ( 0 );
  /*
  auto  l = std::unique_lock { m_Mtx };
  auto  i = m_Cache . find ( key . key () );
  if ( i == m_Cache . end () )
    return  std::nullopt;
  const auto & [ _, v ] = *i;
  return  Response { !v . m_Headers ? nullptr : Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *v . m_Headers ), !v . m_Trailers ? nullptr : Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( *v . m_Trailers ), v . m_Metadata, v . m_Body };
  */
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::contains ( const Key & key ) const -> bool
{
  assert ( 0 );
  /*
  auto  l = std::unique_lock { m_Mtx };
  return  m_Cache . find ( key . key () ) != m_Cache . end ();
  */
}
// --------------------------------------------------------------------------
auto  RingBufferHttpCache::insert ( const Key & key,
                                    Value && value ) -> void
{
  assert ( 0 );
  /*
  auto  l = std::unique_lock { m_Mtx };
  m_Cache . insert_or_assign ( key . key (), std::move ( value ) );
  */
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
