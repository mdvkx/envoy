#pragma once

#include "envoy/buffer/buffer.h"  // Buffer::Instance
#include "envoy/http/header_map.h"  // RequestHeaderMap
#include "source/common/common/logger.h"  // Loggable, Id
#include "source/extensions/filters/http/common/concurrent_hash_map.h"  // ConcurrentHashMap
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

struct  MsgHeaders
{
  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers;
  bool  m_Last;
};
struct  MsgTrailers
{
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers;
};
struct  MsgBody
{
  std::unique_ptr<Buffer::Instance>  m_Body;
  bool  m_Last;
};

using  Msg = std::variant<MsgHeaders, MsgTrailers, MsgBody>;

struct  Filter;

struct  Ticket
{
  std::vector<std::shared_ptr<Filter> >  m_Waiting;
};

using  Cache = ConcurrentHashMap<std::string, Ticket>;

enum struct  State : std::uint32_t
{
  Initial,
  // responsible for sending the request upstream and streaming the response for all subscribers
  Publisher,  // 1st
  // waiting for the response to be published
  Subscriber, // 2nd, 3rd, 4th, ...
};

struct  Filter : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::filter>, public std::enable_shared_from_this<Filter>
{
  using  Self = Filter;

  std::shared_ptr<Cache>  m_Cache;
  std::string  m_Key;
  std::vector<std::shared_ptr<Filter> >  m_Waiting;

  State  m_State = State::Initial;

  Filter ( std::shared_ptr<Cache>  cache )
    : m_Cache { cache }
  {
  }

  // Http::StreamFilterBase
  auto  onDestroy ( ) -> void override;

  // Http::StreamDecoderFilter
  auto  decodeHeaders ( Http::RequestHeaderMap & headers,
                        bool  is_last ) -> Http::FilterHeadersStatus override;

  // Http::StreamEncoderFilter
  auto  encodeHeaders ( Http::ResponseHeaderMap & headers,
                        bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override;
  auto  encodeData    ( Buffer::Instance & body,
                        bool  is_last ) -> Http::FilterDataStatus override;

  static auto  derive_key ( const Http::RequestHeaderMap & headers ) -> std::string;

  auto  receive_msg ( std::shared_ptr<const Msg>  msg ) -> void;

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
