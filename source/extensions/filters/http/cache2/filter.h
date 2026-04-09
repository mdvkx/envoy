#pragma once

#include "./cache.h"
#include "./response.h"

#include "envoy/buffer/buffer.h"  // Buffer::Instance
#include "envoy/common/time.h"  // SystemTime
#include "envoy/http/header_map.h"  // RequestHeaderMap

#include "source/common/common/logger.h"  // Loggable, Id
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase<>
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter

#include "source/extensions/filters/http/cache2/config.pb.h"
#include "source/extensions/filters/http/cache2/config.pb.validate.h"

#include <memory>
#include <optional>
#include <string>

namespace  Envoy::Extensions::HttpFilters::Cache2 {


enum struct  State
{
  Unknown,

  N_A,  // request/response not cacheable, or otherwise n/a

  Hit,
  Miss,

  Destroyed,
};

struct  Filter : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::cache_filter>, public std::enable_shared_from_this<Filter>
{
  State  m_State = State::Unknown;

  std::shared_ptr<Cache>  m_Cache;

  std::string  m_Key;

  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::string  m_Data = "";
  Envoy::SystemTime  m_Stamp;

  Filter ( std::shared_ptr<Cache>  cache )
    : m_Cache { cache }
  {
  }

  auto  onDestroy ( ) -> void override;
  auto  onStreamComplete ( ) -> void override;
  auto  decodeHeaders  ( Http::RequestHeaderMap & headers,
                         bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeHeaders  ( Http::ResponseHeaderMap & headers,
                         bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override;
  auto  encodeData     ( Buffer::Instance & data,
                         bool  is_last ) -> Http::FilterDataStatus override;

  auto  post ( std::invocable<> auto && f ) -> void;

  auto  commit ( ) -> void;

};

auto  Filter::post ( std::invocable<> auto && f ) -> void
{
  this -> decoder_callbacks_ -> dispatcher () . post ( [ wp = this -> weak_from_this (), f = std::move ( f ) ] ( ) mutable -> void
  {  // aka "cancel wrapper"
    auto  p = wp . lock ();
    if (
      p != nullptr
      && p -> m_State != State::Done
    )
    {
      (std::move ( f )) ();
    }

  } );
}

struct  Factory : public Common::FactoryBase<envoy::extensions::filters::http::cache2::Config>
{
  using  Base = FactoryBase;

  using  Config = envoy::extensions::filters::http::cache2::Config;

  Factory ( )
    : Base { "envoy.filters.http.cache2" }
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
