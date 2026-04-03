#pragma once

#include "envoy/http/header_map.h"  // RequestHeaderMap

#include "source/common/common/logger.h"  // Loggable, Id
#include "source/common/http/header_map_impl.h"  // createHeaderMap
#include "source/extensions/filters/http/cache_rqc/config.pb.h"
#include "source/extensions/filters/http/cache_rqc/config.pb.validate.h"
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase<>
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter

#include <memory>
#include <optional>
#include <string>
#include <utility>

// tl;dr:
namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

struct  Filter;
struct  FilterFactory;

}

// --------------------------------------------------------------------------------------------------------------------


template <typename  T_>
[[nodiscard]]
constexpr auto  copy ( T_ t ) -> T_  // thought: accept T_ && to allow user-defined overloads
{
  return  t;
}

namespace  Envoy::Extensions::HttpFilters::CacheRqC
{


struct  Response
{
  using  Self = Response;

  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers;
  std::string  m_Body;

  /*
    Response ( ) = default;

    ~Response ( ) = default;

    Response ( Self && src ) = default;

    Response ( const Self & src )
    : m_Headers { src . m_Headers ? Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *src . m_Headers ) : nullptr },
      m_Trailers { src . m_Trailers ? Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( *src . m_Trailers ) : nullptr },
      m_Body { src . m_Body }
  {
  }

  auto  operator = ( Self && src ) -> Self & = default;

  auto  operator = ( const Self & src ) -> Self &
  {
    if ( this == std::addressof ( src ) )
      return  *this;
    m_Headers = src . m_Headers ? Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *src . m_Headers ) : nullptr;
    m_Trailers = src . m_Trailers ? Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( *src . m_Trailers ) : nullptr;
    m_Body = src . m_Body;
    return  *this;
  }
    */
};


struct  Cache  // not thread-safe (yet)
{
  std::unordered_map<std::string, std::shared_ptr<Response> >  m_Responses;
};


struct  CacheFilter
  : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::cache_filter>, public std::enable_shared_from_this<CacheFilter>
{
    CacheFilter ( std::shared_ptr<Cache>  cache )
    : m_Cache { cache }
  {
    ENVOY_LOG ( debug, "CacheFilter ()" );
  }

    ~CacheFilter ( ) override
  {
    ENVOY_LOG ( debug, "~CacheFilter ()" );
  }

  auto  decodeHeaders ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    ENVOY_LOG ( debug, "CacheFilter::decodeHeaders (): {}, {}", headers, is_last );
    // todo: is the request even cacheable?
    m_Key = this -> derive_key ( headers );
    auto  response = this -> lookup ( m_Key );
    ENVOY_LOG ( debug, "CacheFilter::decodeHeaders (): response? = {}", response . has_value () );
    return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeHeaders ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    ENVOY_LOG ( debug, "CacheFilter::encodeHeaders (): headers = {}, is_last = {}", headers, is_last );
    m_Response . m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
    if ( is_last )
      this -> commit ();
    return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override
  {
    ENVOY_LOG ( debug, "CacheFilter::encodeTrailers (): trailers = {}", trailers );
    m_Response . m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
    this -> commit ();
    return  Http::FilterTrailersStatus::Continue;
  }

  auto  encodeData ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus override
  {
    ENVOY_LOG ( debug, "CacheFilter::encodeData (): body = \"{}\", is_last = {}", data . toString (), is_last );
    m_Response . m_Body += data . toString ();
    if ( is_last )
      this -> commit ();
    return  Http::FilterDataStatus::Continue;
  }

  std::shared_ptr<Cache>  m_Cache;
  std::string  m_Key;
  Response  m_Response;

  auto  commit ( ) -> void
  {
    // todo: is the response cacheable?
    this -> insert ( m_Key, std::move ( m_Response ) );
  }

  static auto  derive_key ( const Http::RequestHeaderMap & headers ) -> std::string
  {
    using namespace  std::literals;
    return  absl::StrCat ( headers . getSchemeValue (), "://"s, headers . getHostValue (), headers . getPathValue () );
  }

  auto  insert ( const std::string & key, Response && response ) -> void
  {
    m_Cache -> m_Responses . insert_or_assign ( key, std::make_shared<Response> ( std::move ( response ) ) );
  }

  auto  lookup ( const std::string & key ) const -> std::optional<std::shared_ptr<Response> >
  {
    auto  i = m_Cache -> m_Responses . find ( key );
    if ( i == m_Cache -> m_Responses . end () )
      return  std::nullopt;
    else
      return  i -> second;
  }

};

struct  Pending
{
};

struct  Coalescer
{
  std::unordered_map<std::string, std::shared_ptr<Pending> >  m_Pending;
};

struct  RqcFilter
  : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::cache_filter>, public std::enable_shared_from_this<RqcFilter>
{
    RqcFilter ( std::shared_ptr<Coalescer>  coalescer )
    : m_Coalescer { coalescer }
  {
    ENVOY_LOG ( debug, "RqcFilter ()" );
  }

    ~RqcFilter ( ) override
  {
    ENVOY_LOG ( debug, "~RqcFilter ()" );
  }

  auto  decodeHeaders ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    ENVOY_LOG ( debug, "RqcFilter::decodeHeaders (): {}, {}", headers, is_last );
    m_Key = this -> derive_key ( headers );
    m_First = this -> try_insert ( m_Key, [ ] ( ) -> std::shared_ptr<Pending> { return  std::make_shared<Pending> (); } );
    ENVOY_LOG ( debug, "RqcFilter::decodeHeaders (): first? = {}", m_First );
    return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeHeaders ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    ENVOY_LOG ( debug, "RqcFilter::encodeHeaders (): headers = {}, is_last = {}", headers, is_last );
    if ( is_last )
      this -> commit ();
    return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override
  {
    ENVOY_LOG ( debug, "RqcFilter::encodeTrailers (): trailers = {}", trailers );
    this -> commit ();
    return  Http::FilterTrailersStatus::Continue;
  }

  auto  encodeData ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus override
  {
    ENVOY_LOG ( debug, "RqcFilter::encodeData (): body = \"{}\", is_last = {}", data . toString (), is_last );
    if ( is_last )
      this -> commit ();
    return  Http::FilterDataStatus::Continue;
  }

  std::shared_ptr<Coalescer>  m_Coalescer;
  std::string  m_Key;
  bool  m_First;

  auto  commit ( ) -> void
  {
    assert ( m_First );  // it's a programmer error if you try to commit but you're not the one who's responsible
    auto  i = m_Coalescer -> m_Pending . find ( m_Key );
    assert ( i != m_Coalescer -> m_Pending . end () );
    auto  x = i -> second;
    m_Coalescer -> m_Pending . erase ( i );
  }

  static auto  derive_key ( const Http::RequestHeaderMap & headers ) -> std::string
  {
    using namespace  std::literals;
    return  absl::StrCat ( headers . getSchemeValue (), "://"s, headers . getHostValue (), headers . getPathValue () );
  }

  auto  try_insert ( const std::string & key, const std::function<std::shared_ptr<Pending> ()> & generator ) -> bool
  {
    auto  i = m_Coalescer -> m_Pending . find ( key );
    if ( i != m_Coalescer -> m_Pending . end () )
      return  false;
    m_Coalescer -> m_Pending . emplace_hint ( i, key, generator () );
    return  true;
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
    auto  coalescer = std::make_shared<Coalescer> ();
    return  [ cache, coalescer ] ( Http::FilterChainFactoryCallbacks & callbacks ) -> void
    {
      callbacks . addStreamFilter ( std::make_shared<CacheFilter> ( cache ) );
      callbacks . addStreamFilter ( std::make_shared<RqcFilter> ( coalescer ) );
    };
  }

    FilterFactory ( )
    : Base { "envoy.filters.http.cache_rqc" }
  {
  }

};

}
