#pragma once

//#include "./cache.h"

#include "envoy/buffer/buffer.h"  // Buffer::Instance
#include "envoy/http/header_map.h"  // RequestHeaderMap
#include "source/common/common/logger.h"  // Loggable, Id
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase<>
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter

#include "source/extensions/filters/http/rqc/config.pb.h"
#include "source/extensions/filters/http/rqc/config.pb.validate.h"

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace  Envoy::Extensions::HttpFilters::Rqc
{

enum struct  State : std::uint32_t
{
  Initial,

  // i'm responsible for sending the request upstream and streaming the response for all subscribers
  Publisher,  // 1st
  // i'm waiting for the response to be published
  Subscriber, // 2nd, 3rd, 4th, ...
  /*
  Destroyed,
  */
};

struct  Cache
{
};

struct  Filter : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::filter>, public std::enable_shared_from_this<Filter>
{
  std::shared_ptr<Cache>  m_Cache;

  State  m_State = State::Initial;

  Filter ( std::shared_ptr<Cache>  cache )
    : m_Cache { cache }
  {
  }

  auto  onDestroy ( ) -> void override
  {
  }

  auto  decodeHeaders ( Http::RequestHeaderMap & headers,
                        bool  is_last ) -> Http::FilterHeadersStatus override
  {
    return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeHeaders ( Http::ResponseHeaderMap & headers,
                        bool  is_last ) -> Http::FilterHeadersStatus override
  {
    return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override
  {
    return  Http::FilterTrailersStatus::Continue;
  }

  auto  encodeData    ( Buffer::Instance & body,
                        bool  is_last ) -> Http::FilterDataStatus override
  {
    return  Http::FilterDataStatus::Continue;
  }

};

struct  Factory : public Common::FactoryBase<envoy::extensions::filters::http::rqc::Config>
{
  using  Base = FactoryBase;

  using  Config = envoy::extensions::filters::http::rqc::Config;

  Factory ( )
    : Base { "envoy.filters.http.rqc" }
  {
  }

  auto  createFilterFactoryFromProtoTyped ( const Config & ,
                                            const std::string & ,
                                            Server::Configuration::FactoryContext &  ) -> Envoy::Http::FilterFactoryCb override
  {
    auto  cache = std::make_shared<Cache> ();
    return  [ = ] ( Http::FilterChainFactoryCallbacks & callbacks )
    {
      callbacks . addStreamFilter ( std::make_shared<Filter> ( cache ) );
    };
  }
};

}
