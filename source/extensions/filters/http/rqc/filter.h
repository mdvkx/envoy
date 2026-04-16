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

struct  Ticket
{
};

struct  Cache
{
  std::mutex  m_Mtx;
  std::unordered_map<std::string, Ticket>  m_Requests;
  auto  insert ( const std::string & k,
                 const std::function<Ticket ()> & v ) -> bool
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Requests . find ( k );
    if ( i == m_Requests . end () )
      return  false;
    m_Requests . emplace_hint ( i, k, v () );
    return  true;
  }
  auto  remove ( const std::string & k ) -> std::optional<Ticket>
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Requests . find ( k );
    if ( i == m_Requests . end () )
      return  std::nullopt;
    auto  x = std::move ( i -> second );
    m_Requests . erase ( i );
    return  x;
  }
};

struct  Filter : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::filter>, public std::enable_shared_from_this<Filter>
{
  using  Self = Filter;

  std::shared_ptr<Cache>  m_Cache;
  std::string  m_Key;
  bool  m_First = false;

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
