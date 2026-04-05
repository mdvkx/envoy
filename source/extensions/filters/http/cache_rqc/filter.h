#pragma once

#include "envoy/common/time.h"  // SystemTime
#include "envoy/http/header_map.h"  // RequestHeaderMap

#include "source/common/common/logger.h"  // Loggable, Id
#include "source/common/http/header_map_impl.h"  // createHeaderMap
#include "source/extensions/filters/http/cache_rqc/config.pb.h"
#include "source/extensions/filters/http/cache_rqc/config.pb.validate.h"
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase<>
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

// tl;dr:
namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

struct  Response;
struct  Pending;
struct  Cache;
struct  Coalescer;

struct  CacheFilter;
struct  RqcFilter;
struct  FilterFactory;

}

// --------------------------------------------------------------------------------------------------------------------


template <typename  T_>
[[nodiscard]]
constexpr auto  copy ( T_ t ) -> T_
{
  return  T_ { t };
}

namespace  Envoy::Extensions::HttpFilters::CacheRqC
{


struct  Response
{
  //using  Self = Response;

  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::string  m_Body = "";

  //Envoy::SystemTime  m_Stamp {};   // "response metadata", note: imo, this should really be MonotonicTime

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


struct  Cache
{
  std::mutex  m_Mtx;
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
    using namespace  std::literals;
    ENVOY_LOG ( debug, "CacheFilter::decodeHeaders (): {}, {}", headers, is_last );
    // todo: is the request even cacheable?
    m_Key = this -> derive_key ( headers );
    auto  response = this -> lookup ( m_Key );
    // todo: https://www.rfc-editor.org/rfc/rfc9211.html
    ENVOY_LOG ( debug, "CacheFilter::decodeHeaders (): response? = {}", response . has_value () );
    if (
      !response
      // todo: response expired?
    )
    {
      m_State = State::Miss;
      return  Http::FilterHeadersStatus::Continue;
    }
    else
    {
      m_State = State::Hit;
      this -> reply ( *response );
      return  Http::FilterHeadersStatus::StopAllIterationAndWatermark;
    }
  }

  auto  encodeHeaders ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    ENVOY_LOG ( debug, "CacheFilter::encodeHeaders (): headers = {}, is_last = {}", headers, is_last );
    switch ( m_State )
    {
      case  State::Unknown:
        assert ( 0 );  // ???
        break;
      case  State::Hit:  // served, but still observed via reverse filter chain
        return  Http::FilterHeadersStatus::Continue;
        break;
      case  State::Miss:
        m_Response . m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
        if ( is_last )
          this -> commit ();
        return  Http::FilterHeadersStatus::Continue;
        break;
      case  State::Stale:
        assert ( 0 );  // not yet implemented
        break;
      default:
        assert ( 0 );
        break;
    }
    /*
    if ( m_State == State::Unknown || m_State == State::Hit )
      return  Http::FilterHeadersStatus::Continue;
    m_Response . m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
    if ( is_last )
      this -> commit ();
    return  Http::FilterHeadersStatus::Continue;
    */
  }

  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override
  {
    ENVOY_LOG ( debug, "CacheFilter::encodeTrailers (): trailers = {}", trailers );
    switch ( m_State )
    {
      case  State::Unknown:
        assert ( 0 );  // ???
        break;
      case  State::Hit:  // served, but still observed via reverse filter chain
        return  Http::FilterTrailersStatus::Continue;
        break;
      case  State::Miss:
        m_Response . m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
        this -> commit ();
        return  Http::FilterTrailersStatus::Continue;
        break;
      case  State::Stale:
        assert ( 0 );  // not yet implemented
        break;
      default:
        assert ( 0 );
        break;
    }
    /*
    if ( m_State == State::Unknown || m_State == State::Hit )
      return  Http::FilterTrailersStatus::Continue;
    m_Response . m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
    this -> commit ();
    return  Http::FilterTrailersStatus::Continue;
    */
  }

  auto  encodeData ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus override
  {
    ENVOY_LOG ( debug, "CacheFilter::encodeData (): body = \"{}\", is_last = {}", data . toString (), is_last );
    switch ( m_State )
    {
      case  State::Unknown:
        assert ( 0 );  // ???
        break;
      case  State::Hit:  // served, but still observed via reverse filter chain
        return  Http::FilterDataStatus::Continue;
        break;
      case  State::Miss:
        m_Response . m_Body += data . toString ();
        if ( is_last )
          this -> commit ();
        return  Http::FilterDataStatus::Continue;
        break;
      case  State::Stale:
        assert ( 0 );  // not yet implemented
        break;
      default:
        assert ( 0 );
        break;
    }
    /*
    if ( m_State == State::Unknown || m_State == State::Hit )
      return  Http::FilterDataStatus::Continue;
    m_Response . m_Body += data . toString ();
    if ( is_last )
      this -> commit ();
    return  Http::FilterDataStatus::Continue;
    */
  }


  std::shared_ptr<Cache>  m_Cache;
  std::string  m_Key;
  Response  m_Response;

  enum struct  State { Unknown, Hit, Miss, Stale, };
  State  m_State = State::Unknown;

  auto  reply ( std::shared_ptr<Response>  response ) -> void
  {
    this -> decoder_callbacks_ -> encodeHeaders ( Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *response -> m_Headers ), ! ( response -> m_Trailers || !response -> m_Body . empty () ), "<details>" );
    if ( !response -> m_Body . empty () )
    {
      auto  data = Buffer::OwnedImpl { response -> m_Body };
      this -> decoder_callbacks_ -> encodeData ( data, ! response -> m_Trailers );
    }
    if ( response -> m_Trailers )
      this -> decoder_callbacks_ -> encodeTrailers ( Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( *response -> m_Trailers ) );
  }

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
    auto  l = std::unique_lock { m_Cache -> m_Mtx };
    m_Cache -> m_Responses . insert_or_assign ( key, std::make_shared<Response> ( std::move ( response ) ) );
  }

  auto  lookup ( const std::string & key ) const -> std::optional<std::shared_ptr<Response> >
  {
    auto  l = std::unique_lock { m_Cache -> m_Mtx };
    auto  i = m_Cache -> m_Responses . find ( key );
    if ( i == m_Cache -> m_Responses . end () )
      return  std::nullopt;
    else
      return  i -> second;
  }

};

struct  Pending
{
  std::vector<std::function<void (Response)> >  m_Subscribers;
  auto  publish ( const Response & response )
  {
    for ( const auto & subscriber : m_Subscribers )
    {
      auto  copy = Response {
        response . m_Headers ? Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *response . m_Headers ) : nullptr,
        response . m_Trailers ? Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( *response . m_Trailers ) : nullptr,
        response . m_Body
      };
      subscriber ( std::move ( copy ) );
    }
  }
  auto  subscribe ( std::function<void (Response)>  callback ) -> void
  {
    m_Subscribers . emplace_back ( std::move ( callback ) );
  }
};

struct  Coalescer
{
  std::mutex  m_Mtx;
  std::unordered_map<std::string, Pending>  m_Pending;
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
    const auto  first = this -> try_insert (
      m_Key,
      [ this ] ( ) -> Pending
      {
        m_State = State::Owner;
        return  Pending {};
      },
      [ this ] ( Pending & pending ) -> void
      {
        m_State = State::Subscriber;
        pending . subscribe ( [ this ] ( Response  response ) -> void
        {
          ENVOY_LOG ( debug, "RqcFilter::decodeHeaders ()::<anonymous> (): subscriber received response" );
          // toss it onto the dispatcher because otherwise i'm gonna get shit for running on an alien thread
          this -> post ( [ this, response = std::move ( response ) ] ( ) mutable -> void
          {
            // i have to manually disect the response and send it off piece by piece. it's annoying, but alas ...
            this -> decoder_callbacks_ -> encodeHeaders ( std::move ( response . m_Headers ), ! ( response . m_Trailers || !response . m_Body . empty () ), "<details>" );
            if ( !response . m_Body . empty () )
            {
              auto  data = Buffer::OwnedImpl { response . m_Body };
              this -> decoder_callbacks_ -> encodeData ( data, ! response . m_Trailers );
            }
            if ( response . m_Trailers )
              this -> decoder_callbacks_ -> encodeTrailers ( std::move ( response . m_Trailers ) );
          } );
        } );
      } );
    ENVOY_LOG ( debug, "RqcFilter::decodeHeaders (): first? = {}", first );
    if ( !first )
      return  Http::FilterHeadersStatus::StopIteration;
    else
      return  Http::FilterHeadersStatus::Continue;
  }

  auto  encodeHeaders ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    ENVOY_LOG ( debug, "RqcFilter::encodeHeaders (): headers = {}, is_last = {}", headers, is_last );
    switch ( m_State )
    {
      case  State::Unknown:
        return  Http::FilterHeadersStatus::Continue;
        break;
      case  State::Owner:
        m_Response . m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
        if ( is_last )
          this -> commit ();
        return  Http::FilterHeadersStatus::Continue;
        break;
      case  State::Subscriber:
        return  Http::FilterHeadersStatus::Continue;
        break;
      default:
        assert ( 0 );
        break;
    }
    /*
    if ( !m_First )
      return  Http::FilterHeadersStatus::Continue;
    m_Response . m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
    if ( is_last )
      this -> commit ();
    return  Http::FilterHeadersStatus::Continue;
    */
  }

  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override
  {
    ENVOY_LOG ( debug, "RqcFilter::encodeTrailers (): trailers = {}", trailers );
    switch ( m_State )
    {
      case  State::Unknown:
        return  Http::FilterTrailersStatus::Continue;
        break;
      case  State::Owner:
        m_Response . m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
        this -> commit ();
        return  Http::FilterTrailersStatus::Continue;
        break;
      case  State::Subscriber:
        return  Http::FilterTrailersStatus::Continue;
        break;
      default:
        assert ( 0 );
        break;
    }
    /*
    if ( !m_First )
      return  Http::FilterTrailersStatus::Continue;
    m_Response . m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
    this -> commit ();
    return  Http::FilterTrailersStatus::Continue;
    */
  }

  auto  encodeData ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus override
  {
    ENVOY_LOG ( debug, "RqcFilter::encodeData (): body = \"{}\", is_last = {}", data . toString (), is_last );
    switch ( m_State )
    {
      case  State::Unknown:
        return  Http::FilterDataStatus::Continue;
        break;
      case  State::Owner:
        m_Response . m_Body += data . toString ();
        if ( is_last )
          this -> commit ();
        return  Http::FilterDataStatus::Continue;
        break;
      case  State::Subscriber:
        return  Http::FilterDataStatus::Continue;
        break;
      default:
        assert ( 0 );
        break;
    }
    /*
    if ( !m_First )
      return  Http::FilterDataStatus::Continue;
    m_Response . m_Body += data . toString ();
    if ( is_last )
      this -> commit ();
    return  Http::FilterDataStatus::Continue;
    */
  }

  std::shared_ptr<Coalescer>  m_Coalescer;
  std::string  m_Key;
  //bool  m_First = false;  // on cache hit, decodeHeaders () isn't called but encodeHeaders () et al. still *is*.
  Response  m_Response;

  enum struct  State { Unknown, Owner, Subscriber, };
  State  m_State = State::Unknown;

  template
  <  typename  F_
   >
  auto  post ( F_ && fn ) -> void
  {
    // todo: test if the filter is still alive?
    this -> decoder_callbacks_ -> dispatcher () . post ( std::forward<F_> ( fn ) );
  }

  auto  commit ( ) -> void
  {
    //assert ( m_First );  // it's a programmer error if you try to commit but you're not the one who's responsible
    assert ( m_State == State::Owner );  // it's a programmer error if you try to commit but you're not the one who's responsible
    auto  l = std::unique_lock { m_Coalescer -> m_Mtx };
    auto  i = m_Coalescer -> m_Pending . find ( m_Key );
    assert ( i != m_Coalescer -> m_Pending . end () );
    auto  x = std::move ( i -> second );
    m_Coalescer -> m_Pending . erase ( i );
    l . unlock ();
    x . publish ( m_Response );
    m_State = State::Subscriber;  // switch to subscriber (aka observer) mode
  }

  static auto  derive_key ( const Http::RequestHeaderMap & headers ) -> std::string
  {
    using namespace  std::literals;
    return  absl::StrCat ( headers . getSchemeValue (), "://"s, headers . getHostValue (), headers . getPathValue () );
  }

  auto  try_insert ( const std::string & key,
                     const std::function<Pending ()> & create,
                     const std::function<void (Pending &)> & modify ) -> bool
  {
    auto  l = std::unique_lock { m_Coalescer -> m_Mtx };
    auto  i = m_Coalescer -> m_Pending . find ( key );
    if ( i != m_Coalescer -> m_Pending . end () )
    {
      modify ( i -> second );
      return  false;
    }
    else
    {
      m_Coalescer -> m_Pending . emplace_hint ( i, key, create () );
      return  true;
    }
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
