#pragma once

#include "source/extensions/filters/http/cache/key.pb.h"
#include "source/extensions/filters/http/request_coalescing/request_coalescing_filter.pb.h"
#include "source/extensions/filters/http/request_coalescing/request_coalescing_filter.pb.validate.h"

#include "envoy/common/time.h"
#include "envoy/http/header_map.h"  // HeaderMap
#include "source/common/http/header_map_impl.h"  // createHeaderMap
#include "source/common/protobuf/utility.h"  // MessageUtil
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter
#include <future>  // promise, shared_future
#include <memory>  // shared, unique ptr
#include <mutex>  // mutex
#include <string>  // string
#include <unordered_map>  // unordered map
#include <variant>  // variant, monostate

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

struct  Pending
{
  std::vector<Http::StreamDecoderFilterCallbacks *>  m_Subscribers;
  auto  publishHeaders ( const Http::ResponseHeaderMap & headers, bool  is_last ) -> void
  {
    for ( auto & p : m_Subscribers )
      p -> encodeHeaders ( Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers ), is_last, "dunno" );
  }
  auto  publishTrailers ( const Http::ResponseTrailerMap & trailers ) -> void
  {
    for ( auto & p : m_Subscribers )
      p -> encodeTrailers ( Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers ) );
  }
  auto  publishBody ( const Buffer::Instance & data, bool  is_last ) -> void
  {
    for ( auto & p : m_Subscribers )
    {
      auto  copy = Buffer::OwnedImpl { data };
      p -> encodeData ( copy, is_last );
    }
  }
  auto  subscribe ( Http::StreamDecoderFilterCallbacks * p ) -> void
  {
    m_Subscribers . emplace_back ( p );
  }
};

struct  Cache
{
  mutable std::mutex  m_Mtx;
  std::unordered_map<std::string, Response>  m_Responses;
  std::unordered_map<std::string, Pending>  m_Pending;
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
  Pending * m_Pending = nullptr;

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
    // i don't override decodeData() and decodeTrailers() because the headers are all that's needed to lookup a cache entry.
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
