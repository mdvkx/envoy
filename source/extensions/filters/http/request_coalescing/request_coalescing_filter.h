#pragma once

#include "source/extensions/filters/http/cache/key.pb.h"
#include "source/extensions/filters/http/request_coalescing/request_coalescing_filter.pb.h"
#include "source/extensions/filters/http/request_coalescing/request_coalescing_filter.pb.validate.h"

#include "envoy/common/time.h"
#include "source/common/protobuf/utility.h"  // MessageUtil
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter
#include <memory>  // shared, unique ptr
#include <string>  // string
#include <unordered_map>  // unordered map

namespace  Envoy::Extensions::HttpFilters::RequestCoalescing
{

struct  RequestCoalescingFilterConfig
{
  explicit  RequestCoalescingFilterConfig ( const envoy::extensions::filters::http::request_coalescing::Config & , Server::Configuration::CommonFactoryContext &  )
  {
  }
};

struct  Response
{
  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::string  m_Body = "";

  Envoy::SystemTime  m_Stamp {};  // "response metadata"
};

using  Cache = std::unordered_map
  < Envoy::Extensions::HttpFilters::Cache::Key  //
   , Response
   , MessageUtil
   , MessageUtil
   > ;

struct  RequestCoalescingFilter : public Http::PassThroughFilter, public std::enable_shared_from_this<RequestCoalescingFilter>
{
  using  Config = RequestCoalescingFilterConfig;  // associated type
                                                  //
  std::shared_ptr<Config>  m_Config;
  std::shared_ptr<Cache>  m_Cache;

  explicit  RequestCoalescingFilter ( std::shared_ptr<Config>  config,
                                      std::shared_ptr<Cache>  cache );
    ~RequestCoalescingFilter ( ) override = default;
  // Http::StreamFilterBase
  auto  onDestroy ( ) -> void override
  {
  }
  auto  onStreamComplete ( ) -> void override
  {
  }
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
