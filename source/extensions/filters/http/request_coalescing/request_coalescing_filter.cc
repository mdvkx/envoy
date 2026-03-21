//#include "source/extensions/filters/http/request_coalescing/request_coalescing_filter.h"
#include "./request_coalescing_filter.h"
#include "envoy/registry/registry.h"  //
#include "source/common/common/logger.h"  // ENVOY_LOG_xxx
#include <memory>  // shared ptr
namespace  Envoy::Extensions::HttpFilters::RequestCoalescing
{

namespace
{
  auto  g_SelfRegister = Registry::RegisterFactory<RequestCoalescingFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory> {};
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
      RequestCoalescingFilter::RequestCoalescingFilter ( std::shared_ptr<Config>  config )
  : m_Config  { config }
{
}
// --------------------------------------------------------------------------
auto  RequestCoalescingFilter::decodeHeaders ( Http::RequestHeaderMap & headers,
                                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG_MISC ( debug, "RequestCoalescingFilter::decodeHeaders (), headers = {}", headers );
  // if cacheable
  return  Http::FilterHeadersStatus::Continue;
}
// --------------------------------------------------------------------------
auto  RequestCoalescingFilter::encodeHeaders ( Http::ResponseHeaderMap & headers,
                                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG_MISC ( debug, "RequestCoalescingFilter::encodeHeaders (), headers = {}", headers );
  return  Http::FilterHeadersStatus::Continue;
}
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
auto  RequestCoalescingFilterFactory::createFilterFactoryFromProtoTyped ( const envoy::extensions::filters::http::request_coalescing::Config & config,
                                                                          const std::string & ,
                                                                          Server::Configuration::FactoryContext & context ) -> Http::FilterFactoryCb
{
  ENVOY_LOG_MISC ( debug, "RequestCoalescingFilterFactory::createFilterFactoryFromProtoTyped ()" );
  return
  [
    config = std::make_shared<RequestCoalescingFilterConfig> ( config, context . serverFactoryContext () )
  ] ( Http::FilterChainFactoryCallbacks & callbacks ) -> void
  {
    ENVOY_LOG_MISC ( debug, "RequestCoalescingFilterFactory::createFilterFactoryFromProtoTyped ()::<anonymous lambda>" );
    callbacks . addStreamFilter ( std::make_shared<RequestCoalescingFilter> ( config ) );
  };
}

}
