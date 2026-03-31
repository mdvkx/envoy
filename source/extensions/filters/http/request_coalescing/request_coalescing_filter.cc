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
{
  "200", "203", "204", "206",
  "300", "301", "308",
  "404", "405", "410", "414", "451",
  "501",
};

static auto  is_cacheable ( const Http::ResponseHeaderMap & headers ) -> bool
{
  return  g_CacheableStatusCodes . contains ( headers . getStatusValue () );
}

static auto  is_cacheable ( const Response & response ) -> bool
{
  return  (
    response . m_Headers != nullptr
    && is_cacheable ( *response . m_Headers )
  );
}

// --------------------------------------------------------------------------

      RequestCoalescingFilter::RequestCoalescingFilter ( std::shared_ptr<Config>  config,
                                                         std::shared_ptr<Cache>  cache )
  : m_Config  { config },
    m_Cache  { cache }
{
}

auto  RequestCoalescingFilter::commit ( ) -> void
{
  if ( !is_cacheable ( m_Response ) )
    return;
  auto  l = std::unique_lock { m_Cache -> m_Mtx };
  m_Cache -> m_Responses . insert_or_assign ( m_Key, std::move ( m_Response ) );  // insert_or_assign because we might have to replace an expired response
  m_Cache -> m_Pending . erase ( m_Key );
}

auto  RequestCoalescingFilter::decodeHeaders ( Http::RequestHeaderMap & headers,
                                               bool  /* is_last - don't care because i don't need body/trailers anyway */ ) -> Http::FilterHeadersStatus
{
  using namespace  std::literals;
  const auto  scheme = headers . getSchemeValue ();
  const auto  host   = headers . getHostValue ();
  const auto  path   = headers . getPathValue ();
  m_Key = absl::StrCat ( scheme, "://", host, path );
  auto  l = std::unique_lock { m_Cache -> m_Mtx };
  if (
    auto  i = m_Cache -> m_Responses . find ( m_Key );
    i == m_Cache -> m_Responses . end ()
    || std::chrono::duration_cast<std::chrono::seconds> ( m_Config -> m_Time . systemTime () - i -> second . m_Stamp ) > 60s
  )
  {  // cache miss or response expired
    // is there a pending request to the same resource already sent by a different filter?
    if ( auto  i = m_Cache -> m_Pending . find ( m_Key );  i != m_Cache -> m_Pending . end () )
    {  // yes - subscribe to it
      i -> second . subscribe ( this -> decoder_callbacks_ );
      return  Http::FilterHeadersStatus::StopIteration;
      assert ( 0 );
    }
    else
    {  // no - create it
      m_Pending = std::addressof ( m_Cache -> m_Pending . emplace_hint ( i, m_Key, Pending {} ) -> second );
      return  Http::FilterHeadersStatus::Continue;
    }
  }
  else
  {  // cache hit
    const auto  & response = i -> second;
    decoder_callbacks_ -> sendLocalReply (
      Http::Code::OK,
      response . m_Body,
      [ ] ( Http::ResponseHeaderMap & ) -> void { },
      absl::nullopt,
      ""sv
    );
    return  Http::FilterHeadersStatus::StopAllIterationAndWatermark;
  }
}

auto  RequestCoalescingFilter::encodeHeaders ( Http::ResponseHeaderMap & headers,
                                               bool  is_last ) -> Http::FilterHeadersStatus
{
  m_Response . m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
  m_Response . m_Stamp = m_Config -> m_Time . systemTime ();
  if ( m_Pending )
    m_Pending -> publishHeaders ( headers, is_last );
  if ( is_last )
    this -> commit ();
  return  Http::FilterHeadersStatus::Continue;
}

auto  RequestCoalescingFilter::encodeData ( Buffer::Instance & data,
                                            bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG_MISC ( debug, "data: \"{}\", is_last: {}", data . toString (), is_last ? "true" : "false" );
  m_Response . m_Body += data . toString ();
  if ( m_Pending )
    m_Pending -> publishBody ( data, is_last );
  if ( is_last )
    this -> commit ();
  return  Http::FilterDataStatus::Continue;
}

auto  RequestCoalescingFilter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus
{
  m_Response . m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
  if ( m_Pending )
    m_Pending -> publishTrailers ( trailers );
  this -> commit ();
  return  Http::FilterTrailersStatus::Continue;
}

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
