#pragma once

#include "envoy/http/header_map.h"  // RequestHeaderMap

#include "source/common/common/logger.h"  // Loggable, Id
#include "source/extensions/filters/http/cache_rqc/config.pb.h"
#include "source/extensions/filters/http/cache_rqc/config.pb.validate.h"
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase<>
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter

#include <memory>
#include <string>

// tl;dr:
namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

struct  Filter;
struct  FilterFactory;

}

// --------------------------------------------------------------------------------------------------------------------

namespace  Envoy::Extensions::HttpFilters::CacheRqC
{


struct  Response
{
  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers;
  std::string  m_Body;
};


struct  Cache  // not thread-safe (yet)
{
  std::unordered_map<std::string, Response>  m_Responses;
};


struct  Filter
  : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::cache_filter>, public std::enable_shared_from_this<Filter>
{
    Filter ( std::shared_ptr<Cache>  cache )
    : m_Cache { cache }
  {
    ENVOY_LOG ( debug, "Filter ()" );
  }

    ~Filter ( ) override
  {
    ENVOY_LOG ( debug, "~Filter ()" );
  }

  auto  decodeHeaders ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    ENVOY_LOG ( debug, "decodeHeaders (): {}, {}", headers, is_last );
    m_Key = this -> derive_key ( headers );
    auto  response = this -> lookup ( m_Key );
    ENVOY_LOG ( debug, "decodeHeaders (): response? = {}", response . has_value () );
    return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeHeaders ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    ENVOY_LOG ( debug, "encodeHeaders (): {}, {}", headers, is_last );
    m_Response . m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
    if ( is_last )
      this -> commit ();
    return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override
  {
    ENVOY_LOG ( debug, "encodeTrailers (): {}", trailers );
    m_Response . m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
    this -> commit ();
    return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeData ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus override
  {
    ENVOY_LOG ( debug, "encodeData (): {}, {}", data, is_last );
    m_Response . m_Body += data . toString ();
    if ( is_last )
      this -> commit ();
    return  Http::FilterHeadersStatus::Continue;
  }

  std::shared_ptr<Cache>  m_Cache;
  std::string  m_Key;
  Response  m_Response;

  auto  commit ( ) -> void
  {
    this -> insert ( m_Key, std::move ( m_Response ) );
  }

  static auto  derive_key ( const Http::RequestHeaderMap & headers ) -> std::string
  {
    using namespace  std::literals;
    return  absl::StrCat ( headers . getSchemeValue (), "://"s, headers . getHostValue (), headers . getPathValue () );
  }

  auto  insert ( const std::string & key, Response && response ) -> void
  {
    m_Cache -> m_Responses . insert_or_assign ( key, std::move ( response ) );
  }

  auto  lookup ( const std::string & key ) const -> std::optional<Response>
  {
    auto  i = m_Cache -> m_Responses . find ( key );
    if ( i == m_Cache -> m_Responses . end () )
      return  std::nullopt;
    else
      return  i -> second;
  }

};

// this is actually more of a filter factory factory. anyways
struct  FilterFactory
  : public Common::FactoryBase<envoy::extensions::filters::http::cache_rqc::Config>
{
  using  Self = FilterFactory;
  using  Base = Self::FactoryBase;
  using  Config = envoy::extensions::filters::http::cache_rqc::Config;

  auto  createFilterFactoryFromProtoTyped ( const Config & ,
                                            const std::string & ,
                                            Server::Configuration::FactoryContext &  ) -> Envoy::Http::FilterFactoryCb override
  {
    auto  cache = std::make_shared<Cache> ();
    return  [ cache ] ( Http::FilterChainFactoryCallbacks & callbacks ) -> void
    {
      callbacks . addStreamFilter ( std::make_shared<Filter> ( cache ) );
    };
  }

    FilterFactory ( )
    : Base { "envoy.filters.http.cache_rqc" }
  {
  }

};

}
