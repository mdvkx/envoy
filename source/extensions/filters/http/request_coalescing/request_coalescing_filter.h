#pragma once

#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter
#include <memory>  // shared ptr

//namespace  Envoy::Extensions::HttpFilters::Cache {

struct  RequestCoalescingFilterConfig
{
  explicit  RequestCoalescingFilterConfig ( const request_coalescing::Config &  )  { }
};
struct  RequestCoalescingFilter : public Http::PassThroughFilter
{
  using  Config = RequestCoalescingFilterConfig;  // associated type
  std::shared_ptr<Config>  m_Config;
  explicit  RequestCoalescingFilter ( std::shared_ptr<Config>  config );
  auto  decodeHeaders ( Http::ResponseHeaderMap & headers,
                        bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeHeaders ( Http::ResponseHeaderMap & headers,
                        bool  is_last ) -> Http::FilterHeadersStatus override;
};
