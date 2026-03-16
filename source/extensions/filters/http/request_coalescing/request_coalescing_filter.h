#pragma once

#include "source/extensions/filters/http/request_coalescing/request_coalescing_filter.pb.h"
#include "source/extensions/filters/http/request_coalescing/request_coalescing_filter.pb.validate.h"

#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter
#include <memory>  // shared ptr

namespace  Envoy::Extensions::HttpFilters::Cache {
struct  RequestCoalescingFilterConfig
{
  explicit  RequestCoalescingFilterConfig ( const envoy::extensions::filters::http::request_coalescing::Config & , Server::Configuration::CommonFactoryContext &  )  { }
};
struct  RequestCoalescingFilter : public Http::PassThroughFilter, public std::enable_shared_from_this<RequestCoalescingFilter>
{
  using  Config = RequestCoalescingFilterConfig;  // associated type
  std::shared_ptr<Config>  m_Config;
  explicit  RequestCoalescingFilter ( std::shared_ptr<Config>  config );
  // Http::StreamFilterBase
  auto  onDestroy ( ) -> void override { }
  auto  onStreamComplete ( ) -> void override { }
  // Http::StreamDecoderFilter
  auto  decodeHeaders ( Http::RequestHeaderMap & headers,
                        bool  is_last ) -> Http::FilterHeadersStatus override;
  // Http::StreamEncoderFilter
  auto  encodeHeaders ( Http::ResponseHeaderMap & headers,
                        bool  is_last ) -> Http::FilterHeadersStatus override;

};

struct  RequestCoalescingFilterFactory : public Common::FactoryBase<envoy::extensions::filters::http::request_coalescing::Config>
{
  RequestCoalescingFilterFactory ( )
    : FactoryBase { "envoy.filters.http.request_coalescing" }
  {
  }

  auto createFilterFactoryFromProtoTyped ( const envoy::extensions::filters::http::request_coalescing::Config & config,
                                           const std::string & stats_prefix,
                                           Server::Configuration::FactoryContext & context ) -> Http::FilterFactoryCb override;
};


}
