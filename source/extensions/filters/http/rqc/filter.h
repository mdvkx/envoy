#pragma once

#include "envoy/buffer/buffer.h"  // Buffer::Instance
#include "envoy/http/header_map.h"  // RequestHeaderMap
#include "source/common/common/logger.h"  // Loggable, Id
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase<>
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter

#include "source/extensions/filters/http/rqc/config.pb.h"
#include "source/extensions/filters/http/rqc/config.pb.validate.h"


namespace  Envoy::Extensions::HttpFilters::Rqc {

enum struct  State
{
  Unknown,

  // i'm responsible for sending the request upstream and streaming the response for all subscribers
  Publisher,  // 1st
  // i'm waiting for the response to be published
  Subscriber, // 2nd, 3rd, 4th, ...
  Done,
};

struct  Filter : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::cache_filter>, public std::enable_shared_from_this<Filter>
{
  State  m_State = State::Unknown;
  auto  onDestroy ( ) -> void override;
  auto  onStreamComplete ( ) -> void override;
  auto  decodeHeaders  ( Http::RequestHeaderMap & headers,
                         bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeHeaders  ( Http::ResponseHeaderMap & headers,
                         bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override;
  auto  encodeData     ( Buffer::Instance & data,
                         bool  is_last ) -> Http::FilterDataStatus override;

  auto  touch  ( Http::RequestHeaderMap & headers ) -> void;
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
    return  [ ] ( Http::FilterChainFactoryCallbacks & callbacks )
    {
      callbacks . addStreamFilter ( std::make_shared<Filter> () );
    };
  }
};

}
