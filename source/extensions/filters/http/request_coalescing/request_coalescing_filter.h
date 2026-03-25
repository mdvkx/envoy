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
  TimeSource  & m_Time;
  explicit  RequestCoalescingFilterConfig ( const envoy::extensions::filters::http::request_coalescing::Config & , Server::Configuration::CommonFactoryContext & context )
    : m_Time  { context . timeSource () }
  {
  }
};

struct  Response
{
  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::string  m_Body = "";

  Envoy::SystemTime  m_Stamp {};  // "response metadata", @note: this should really be MonotonicTime
};

/*
using  Cache = std::unordered_map
  < std::string  //Envoy::Extensions::HttpFilters::Cache::Key  //
   , Response
   //, MessageUtil
   //, MessageUtil
   > ;
   */

struct  Cache
{
  mutable std::mutex  m_Mtx;
  std::unordered_map<std::string, Response>  m_Cache;

  auto  lookup ( const std::string & key ) const -> std::optional<const Response *>;
  auto  insert ( const std::string & key,
                 Response && value ) -> void;
};

struct  RequestCoalescingFilter : public Http::PassThroughFilter, public std::enable_shared_from_this<RequestCoalescingFilter>
{
  using  Self = RequestCoalescingFilter;

  using  Config = RequestCoalescingFilterConfig;  // associated type

    RequestCoalescingFilter ( std::shared_ptr<Config>  config,
                              std::shared_ptr<Cache>  cache );
    ~RequestCoalescingFilter ( ) override = default;


  auto  commit ( ) -> void;

  std::shared_ptr<Config>  m_Config;
  std::shared_ptr<Cache>  m_Cache;
  std::string  m_Key;
  Response  m_Response;

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
  // i don't override decodeData() and decodeTrailers() because only the headers are needed to lookup a cache entry.
  // Http::StreamEncoderFilter
  auto  encodeHeaders ( Http::ResponseHeaderMap & headers,
                        bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeData ( Buffer::Instance & data,
                     bool  is_last ) -> Http::FilterDataStatus override;
  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override;
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
