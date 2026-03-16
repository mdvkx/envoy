//#include "source/extensions/filters/http/request_coalescing/request_coalescing.h"
#include "./request_coalescing.h"
#include <memory>  // shared ptr

// --------------------------------------------------------------------------
      RequestCoalescingFilter::RequestCoalescingFilter ( std::shared_ptr<Config>  config )
      : m_Config  { config }
{
}
// --------------------------------------------------------------------------
auto  RequestCoalescingFilter::decodeHeaders ( Http::RequestHeaderMap & headers,
                                               bool  is_last ) -> Http::FilterHeadersStatus
{
  return  Http::FilterHeadersStatus::Continue;
}
// --------------------------------------------------------------------------
auto  RequestCoalescingFilter::encodeHeaders ( Http::ResponseHeaderMap & headers,
                                               bool  is_last ) -> Http::FilterHeadersStatus
{
  return  Http::FilterHeadersStatus::Continue;
}
