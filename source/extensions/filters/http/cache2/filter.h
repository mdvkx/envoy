#pragma once

#include "envoy/buffer/buffer.h"  // Buffer::Instance
#include "envoy/common/time.h"  // SystemTime
#include "envoy/http/header_map.h"  // RequestHeaderMap

#include "source/common/common/logger.h"  // Loggable, Id
#include "source/extensions/filters/http/common/concurrent_hash_map.h"  // ConcurrentHashMap
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
  Initial,
  NotCacheable,
  Miss,
  Hit,
};

struct  Response
{
  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::string  m_Body = "";
  Envoy::SystemTime  m_Stamp;
};

using  Cache = ConcurrentHashMap<std::string, std::shared_ptr<const Response> >;

struct  Filter : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::filter>, public std::enable_shared_from_this<Filter>
{
  using  Self = Filter;
  State  m_State = State::Initial;

  std::shared_ptr<Cache>  m_Cache;

  std::string  m_Key;

  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::string  m_Body = "";
  Envoy::SystemTime  m_Stamp;

        Filter ( std::shared_ptr<Cache>  cache )
    : m_Cache { cache }
  {
  }

  // Http::StreamFilterBase
  auto  onDestroy ( ) -> void override;

  // Http::StreamDecoderFilter
  auto  decodeHeaders  ( Http::RequestHeaderMap & headers,
                         bool  is_last ) -> Http::FilterHeadersStatus override;
  // Http::StreamEncoderFilter
  auto  encodeHeaders  ( Http::ResponseHeaderMap & headers,
                         bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeData     ( Buffer::Instance & data,
                         bool  is_last ) -> Http::FilterDataStatus override;
  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override;

  [[nodiscard]]
  static auto  derive_key ( const Http::RequestHeaderMap & headers ) -> std::string;

  auto  commit         ( ) -> void;
  auto  post           ( std::invocable<> auto && f ) -> void;

};

auto  Filter::post ( std::invocable<> auto && f ) -> void
{
  this -> decoder_callbacks_ -> dispatcher () . post ( [ wp = this -> weak_from_this (), f = std::move ( f ) ] ( ) mutable -> void
  {  // aka "cancel wrapper"
    if ( auto  p = wp . lock () )
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
