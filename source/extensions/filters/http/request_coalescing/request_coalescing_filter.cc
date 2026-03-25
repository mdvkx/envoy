//#include "source/extensions/filters/http/request_coalescing/request_coalescing_filter.h"
#include "./request_coalescing_filter.h"
#include "envoy/http/codes.h"  // Http::Code
#include "envoy/http/header_map.h"  // HeaderMap
#include "envoy/registry/registry.h"  // Registry::RegisterFactory
#include "source/common/common/logger.h"  // ENVOY_LOG_MISC
#include "source/common/http/header_map_impl.h"  // createHeaderMap
#include <memory>  // shared ptr
#include <optional>  // optional
namespace  Envoy::Extensions::HttpFilters::RequestCoalescing
{

namespace
{
  auto  g_SelfRegister = Registry::RegisterFactory<RequestCoalescingFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory> {};
}

const auto  g_CacheableStatusCodes = std::unordered_set<std::string_view>
{  // unordered_set doesn't have a constexpr constructor, jeeeeeesus.
  "200", "203", "204", "206",
  "300", "301", "308",
  "404", "405", "410", "414", "451",
  "501",
};
// --------------------------------------------------------------------------
static auto  is_cacheable ( const Http::ResponseHeaderMap & headers ) -> bool
{
  if ( !g_CacheableStatusCodes . contains ( headers . getStatusValue () ) )
    return  false;
  //if ( auto  * p = headers . CacheControl (); p == nullptr ||   )
  return  true;
}
// --------------------------------------------------------------------------
static auto  is_cacheable ( const Response & response ) -> bool
{
  return  response . m_Headers != nullptr && is_cacheable ( *response . m_Headers );
}
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
auto  Cache::lookup ( const std::string & key ) const -> std::optional<const Response *>
{
  auto  l = std::unique_lock { m_Mtx };
  if ( auto  i = m_Cache . find ( key ); i != m_Cache . end () )
    return  std::addressof ( i -> second );
  else
    return  std::nullopt;
}
// --------------------------------------------------------------------------
auto  Cache::insert ( const std::string & key,
                      Response && value ) -> void
{
  auto  l = std::unique_lock { m_Mtx };
  m_Cache [ key ] = std::move ( value );
}
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
      RequestCoalescingFilter::RequestCoalescingFilter ( std::shared_ptr<Config>  config,
                                                         std::shared_ptr<Cache>  cache )
  : m_Config  { config }
  , m_Cache  { cache }
{
}
// --------------------------------------------------------------------------
auto  RequestCoalescingFilter::commit ( ) -> void
{
  if ( !is_cacheable ( m_Response ) )
    return;
  m_Cache -> insert ( m_Key, std::move ( m_Response ) );
  //assert ( 0 );
}
// --------------------------------------------------------------------------
auto  RequestCoalescingFilter::decodeHeaders ( Http::RequestHeaderMap & headers,
                                               bool  is_last ) -> Http::FilterHeadersStatus
{
  using namespace  std::literals;
  const auto  scheme = headers . getSchemeValue ();
  const auto  host = headers . getHostValue ();
  const auto  path = headers . getPathValue ();
  const auto  key = absl::StrCat ( scheme, "://", host, path );
  m_Key = key;
  const auto  response = m_Cache -> lookup ( key );
  if ( !response  // cache miss
       ||  std::chrono::duration_cast<std::chrono::seconds> ( (m_Config -> m_Time . systemTime () - (*response) -> m_Stamp) ) > 60s )  // expired
    return  Http::FilterHeadersStatus::Continue;
  decoder_callbacks_ -> sendLocalReply ( Http::Code::OK, response . value () -> m_Body, [ response ] ( Http::ResponseHeaderMap & headers ) -> void
  {
    // TODO: what about original headers?
    // TODO: response may dangle! it's a reference into a shared cache
    headers = *response . value () -> m_Headers;
  }, absl::nullopt, "details"sv );
  return  Http::FilterHeadersStatus::StopAllIterationAndWatermark;
  assert ( 0 );
}
// --------------------------------------------------------------------------
auto  RequestCoalescingFilter::encodeHeaders ( Http::ResponseHeaderMap & headers,
                                               bool  is_last ) -> Http::FilterHeadersStatus
{
  m_Response . m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
  m_Response . m_Stamp = m_Config -> m_Time . systemTime ();
  if ( is_last )
    this -> commit ();
  return  Http::FilterHeadersStatus::Continue;
}
// --------------------------------------------------------------------------
auto  RequestCoalescingFilter::encodeData ( Buffer::Instance & data,
                                            bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG_MISC ( debug, "data: \"{}\", is_last: {}", data . toString (), is_last ? "true" : "false" );
  m_Response . m_Body += data . toString ();
  if ( is_last )
    this -> commit ();
  return  Http::FilterDataStatus::Continue;
}
// --------------------------------------------------------------------------
auto  RequestCoalescingFilter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus
{
  m_Response . m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
  this -> commit ();
  return  Http::FilterTrailersStatus::Continue;
}
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
auto  RequestCoalescingFilterFactory::createFilterFactoryFromProtoTyped ( const envoy::extensions::filters::http::request_coalescing::Config & config,
                                                                          const std::string & ,
                                                                          Server::Configuration::FactoryContext & context ) -> Http::FilterFactoryCb
{
  return
  [
    config  = std::make_shared<RequestCoalescingFilterConfig> ( config, context . serverFactoryContext () ),
    cache   = std::make_shared<Cache> ()
  ] ( Http::FilterChainFactoryCallbacks & callbacks ) -> void
  {
    callbacks . addStreamFilter ( std::make_shared<RequestCoalescingFilter> ( config, cache ) );
  };
}

}
