#pragma once

#include "envoy/buffer/buffer.h"  // Buffer::Instance
#include "envoy/http/header_map.h"  // RequestHeaderMap
#include "source/common/common/logger.h"  // Loggable, Id
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase<>
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter

#include "source/extensions/filters/http/rqc/config.pb.h"
#include "source/extensions/filters/http/rqc/config.pb.validate.h"


namespace  Envoy::Extensions::HttpFilters::Rqc {


struct  Filter;

struct  Pending
{
  std::vector<std::shared_ptr<Filter> >  m_Waiting;
  auto  publish_headers ( const Http::ResponseHeaderMap & headers,
                          bool  is_last ) -> void;
  auto  publish_trailers ( const Http::ResponseTrailerMap & trailers ) -> void;
  auto  publish_data ( const std::string & data,
                       bool is_last ) -> void;
  auto  subscribe ( std::shared_ptr<Filter>  x ) -> void;
};

struct  Collapser
{
  mutable std::mutex  m_Mtx;
  std::unordered_map<std::string, Pending>  m_Pending;
  auto  insert_or ( const std::string & k,
                    const std::function<Pending ()> & lazy,
                    const std::function<void (Pending &)> & modify ) -> bool  // cannot return a reference, unsafe
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Pending . find ( k );
    if ( i == m_Pending . end () )
    {
      m_Pending . emplace_hint ( i, k, lazy () );
      return  true;
    }
    else
    {
      modify ( i -> second );
      return  false;
    }
  }
  auto  remove ( const std::string & k ) -> std::optional<Pending>
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Pending . find ( k );
    if ( i == m_Pending . end () )
      return  std::nullopt;
    auto  x = std::move ( i -> second );
    m_Pending . erase ( i );
    l . unlock ();  // a bit proposterous
    return  x;
  }
};

enum struct  State
{
  Unknown,

  // i'm responsible for sending the request upstream and streaming the response for all subscribers
  Publisher,  // 1st
  // i'm waiting for the response to be published
  Subscriber, // 2nd, 3rd, 4th, ...
  Destroyed,
};

struct  Filter : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::cache_filter>, public std::enable_shared_from_this<Filter>
{
  State  m_State = State::Unknown;

  std::shared_ptr<Collapser>  m_Collapser;

  std::string  m_Key;
  std::optional<Pending>  m_Channel;  // i hate this but there's not much i can do

  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers;
  std::string  m_Data;

  auto  onDestroy ( ) -> void override;
  auto  onStreamComplete ( ) -> void override;
  auto  decodeHeaders  ( Http::RequestHeaderMap & headers,
                         bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeHeaders  ( Http::ResponseHeaderMap & headers,
                         bool  is_last ) -> Http::FilterHeadersStatus override;
  auto  encodeData     ( Buffer::Instance & data,
                         bool  is_last ) -> Http::FilterDataStatus override;
  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override;

  auto  touch          ( Http::RequestHeaderMap & headers ) -> void;
  auto  post_headers   ( const Http::ResponseHeaderMap & headers,
                         bool  is_last ) -> void;
  auto  post_data      ( const std::string & data,
                         bool  is_last ) -> void;
  auto  post_trailers  ( const Http::ResponseTrailerMap & trailers ) -> void;
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
