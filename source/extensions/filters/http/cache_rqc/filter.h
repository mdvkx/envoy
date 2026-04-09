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
#include <type_traits>
#include <utility>

// tl;dr:
namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

struct  Response;
struct  Cache;
struct  Pending;
struct  Coalescer;

struct  CacheFilter;
struct  RqcFilter;
struct  FilterFactory;

}

// --------------------------------------------------------------------------------------------------------------------


// explicit copy() to accompany move()
template <typename  T_>
[[nodiscard]]
constexpr auto  copy ( const T_ & x ) noexcept ( std::is_nothrow_copy_constructible_v<T_> ) -> T_
{
  return  T_ { x };
}

namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

template <typename  T_, std::size_t  N_>
struct  Ring
{
  static_assert ( std::has_single_bit ( N_ ), "ring capacity must be a non-zero power of two" );

  std::size_t               m_Wr = 0;
  std::size_t               m_Size = 0;
  T_                      * m_Data = reinterpret_cast<T_ *> ( m_Mem );
  alignas ( T_ ) std::byte  m_Mem [ sizeof ( T_ ) * N_ ];

  constexpr  Ring ( ) = default;
  constexpr  ~Ring ( )
  {
    this -> clear ();
  }
  [[nodiscard]]
  constexpr auto  size ( ) const -> std::size_t
  {
    return  m_Size;
  }
  [[nodiscard]]
  constexpr auto  capacity ( ) const -> std::size_t
  {
    return  N_;
  }
  /*
  constexpr auto  find_if ( const std::function<bool (const T_ &)> & predicate ) const -> std::optional<std::reference_wrapper<T_> >
  {
    (void) predicate;
    return  std::nullopt;
  }
    */
  constexpr auto  find_if ( const std::function<bool (const T_ &)> &  ) const -> std::optional<std::reference_wrapper<T_> >
  {
    return  std::nullopt;
  }
  constexpr auto  clear ( ) -> void
  {
    for ( std::size_t i = 0; i < m_Size; i ++ )
      std::destroy_at ( std::addressof ( m_Data [ i ] ) );
    m_Wr = 0;
    m_Size = 0;
  }
  template <typename ...  Args_>
  constexpr auto  push ( Args_ && ... args ) -> void
  {
    static_assert ( std::constructible_from<T_, Args_ ...> );
    if ( this -> size () >= this -> capacity () )
      m_Data [ m_Wr ] = T_ { std::forward<Args_> ( args ) ... };
    else
    {
      std::construct_at ( std::addressof ( m_Data [ m_Wr ] ), std::forward<Args_> ( args ) ... );
      m_Size ++;
    }
    m_Wr ++;
    // wrap around
    m_Wr &= N_ - 1;
  }
};

struct  Response
{
  using  Self = Response;

  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::string  m_Body = "";
  Envoy::SystemTime  m_Stamp {};   // "response metadata", note: imo, this should really be MonotonicTime

    Response ( ) = default;

    Response ( Self && src ) = default;

    Response ( const Self & src )
    : m_Headers  { src . m_Headers ? Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *src . m_Headers ) : nullptr },
      m_Trailers { src . m_Trailers ? Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( *src . m_Trailers ) : nullptr },
      m_Body     { src . m_Body },
      m_Stamp    { src . m_Stamp }
  {
  }

    ~Response ( ) = default;

  auto  operator = ( Self && src ) -> Self & = default;

  auto  operator = ( const Self & src ) -> Self &
  {
    if ( this == std::addressof ( src ) )
      return  *this;
    m_Headers  = src . m_Headers ? Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *src . m_Headers ) : nullptr;
    m_Trailers = src . m_Trailers ? Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( *src . m_Trailers ) : nullptr;
    m_Body     = src . m_Body;
    m_Stamp    = src . m_Stamp;
    return  *this;
  }
};

struct  Cache
{
  std::mutex  m_Mtx;
  std::unordered_map<std::string, std::shared_ptr<Response> >  m_Responses;
};

struct  Pending  // maybe extrapolate as: Channel<Pending>?  kis,s
{
  std::vector<std::function<void (Response)> >  m_Subscribers;
  auto  publish ( const Response & response )
  {
    for ( const auto & subscriber : m_Subscribers )
      subscriber ( copy ( response ) );
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

struct  CacheFilter
  : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::cache_filter>, public std::enable_shared_from_this<CacheFilter>
{
  explicit  CacheFilter ( std::shared_ptr<Cache>  cache )
    : m_Cache { cache }
  {
    ENVOY_LOG ( debug, "CacheFilter ()" );
  }

    ~CacheFilter ( ) override
  {
    ENVOY_LOG ( debug, "~CacheFilter ()" );
  }

  auto  onDestroy ( ) -> void override
  {
    ENVOY_LOG ( debug, "CacheFilter::onDestroy ()" );
  }

  auto  onStreamComplete ( ) -> void override
  {
    ENVOY_LOG ( debug, "CacheFilter::onStreamComplete ()" );
  }

  auto  decodeHeaders ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    using namespace  std::literals;
    ENVOY_LOG ( debug, "CacheFilter::decodeHeaders (): {}, {}", headers, is_last );

    // is the request even cacheable?
    if (
      headers . Path () == nullptr
      || headers . Host () == nullptr
      || headers . getMethodValue () != "GET"sv  // uppercase important, TODO: there's probably a better way to check this, though
      || !is_last
    )
    {  // request is not cacheable -> response isn't either
      m_State = State::Ignore;  // n/a
      return  Http::FilterHeadersStatus::Continue;
    }

    m_Key = this -> derive_key ( headers );

    // TODO: Cache-Status HTTP response header field, https://www.rfc-editor.org/rfc/rfc9211.html
    if (
      auto  response = this -> lookup ( m_Key );
      !response
      || std::chrono::duration_cast<std::chrono::seconds> ( std::chrono::system_clock::now () - (*response) -> m_Stamp ) > 60s  // pretend the response expires after 60s
    )
    {
      ENVOY_LOG ( debug, "CacheFilter::decodeHeaders (): cache miss" );
      m_State = State::Miss; // TODO: use State::Stale if expired
      return  Http::FilterHeadersStatus::Continue;
    }
    else
    {
      ENVOY_LOG ( debug, "CacheFilter::decodeHeaders (): cache hit" );
      m_State = State::Hit;
      this -> stream_response ( copy ( *(*response) ) );
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
      case  State::Ignore:
        return  Http::FilterHeadersStatus::Continue;  // not cacheable
        break;
      case  State::Hit:  // served, but still observed because of the full reverse filter chain
        return  Http::FilterHeadersStatus::Continue;
        break;
      case  State::Miss:
        m_Response . m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
        m_Response . m_Stamp  = std::chrono::system_clock::now ();  // todo: use envoy's time api (it's just a wrapper, but it's cleaner)
        if ( is_last )
          this -> commit ();
        return  Http::FilterHeadersStatus::Continue;
        break;
      case  State::Stale:
        assert ( 0 );  // not yet implemented, handled in State::Miss until then
        break;
      default:
        assert ( 0 );
        break;
    }
  }

  auto  encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override
  {
    ENVOY_LOG ( debug, "CacheFilter::encodeTrailers (): trailers = {}", trailers );
    switch ( m_State )
    {
      case  State::Unknown:
        assert ( 0 );  // ???
        break;
      case  State::Ignore:
        return  Http::FilterTrailersStatus::Continue;
        break;
      case  State::Hit:  // served, but still observed because of the full reverse filter chain
        return  Http::FilterTrailersStatus::Continue;
        break;
      case  State::Miss:
        m_Response . m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
        this -> commit ();
        return  Http::FilterTrailersStatus::Continue;
        break;
      case  State::Stale:
        assert ( 0 );  // not yet implemented, handled in State::Miss until then
        break;
      default:
        assert ( 0 );
        break;
    }
  }

  auto  encodeData ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus override
  {
    ENVOY_LOG ( debug, "CacheFilter::encodeData (): body = \"{}\", is_last = {}", data . toString (), is_last );
    switch ( m_State )
    {
      case  State::Unknown:
        assert ( 0 );  // ???
        break;
      case  State::Ignore:
        return  Http::FilterDataStatus::Continue;
        break;
      case  State::Hit:  // served, but still observed because of the full reverse filter chain
        return  Http::FilterDataStatus::Continue;
        break;
      case  State::Miss:
        m_Response . m_Body += data . toString ();
        if ( is_last )
          this -> commit ();
        return  Http::FilterDataStatus::Continue;
        break;
      case  State::Stale:
        assert ( 0 );  // not yet implemented, handled in State::Miss until then
        break;
      default:
        assert ( 0 );
        break;
    }
  }


  inline static const auto  CACHEABLE_STATUS_CODES = std::unordered_set<std::string_view>
  {  //  taken from file://./../cache/cacheability_utils.cc
    "200", "203", "204", "206",
    "300", "301", "308",
    "404", "405", "410", "414", "451",
    "501",
  };

  std::shared_ptr<Cache>  m_Cache;
  std::string  m_Key;
  Response  m_Response;

  enum struct  State { Unknown, Ignore, Hit, Miss, Stale, };
  State  m_State = State::Unknown;

  auto  stream_response ( Response && response ) -> void
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
  }

  auto  commit ( ) -> void
  {
    assert ( m_Response . m_Headers != nullptr );
    // is the response cacheable?
    if (
      CACHEABLE_STATUS_CODES . contains ( m_Response . m_Headers -> getStatusValue () )
      // && Cache-Control headers etc.
    )
    {
      this -> insert ( m_Key, std::move ( m_Response ) );
    }
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

struct  RqcFilter
  : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::cache_filter>, public std::enable_shared_from_this<RqcFilter>
{
  explicit  RqcFilter ( std::shared_ptr<Coalescer>  coalescer )
    : m_Coalescer { coalescer }
  {
    ENVOY_LOG ( debug, "RqcFilter ()" );
  }

    ~RqcFilter ( ) override
  {
    ENVOY_LOG ( debug, "~RqcFilter ()" );
  }

  auto  onDestroy ( ) -> void override
  {
    ENVOY_LOG ( debug, "RqcFilter::onDestroy ()" );
  }

  auto  onStreamComplete ( ) -> void override
  {
    ENVOY_LOG ( debug, "RqcFilter::onStreamComplete ()" );
  }

  auto  decodeHeaders ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus override
  {
    ENVOY_LOG ( debug, "RqcFilter::decodeHeaders (): {}, {}", headers, is_last );
    m_Key = this -> derive_key ( headers );
    const auto  first = this -> try_insert ( m_Key,
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
            this -> stream_response ( std::move ( response ) );
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
  }

  std::shared_ptr<Coalescer>  m_Coalescer;
  std::string  m_Key;
  Response  m_Response;

  enum struct  State { Unknown, Owner, Subscriber, };
  State  m_State = State::Unknown;

  template <typename  F_>
  auto  post ( F_ && fn ) -> void
  {
    // TODO: test if the filter is still alive?
    this -> decoder_callbacks_ -> dispatcher () . post ( std::forward<F_> ( fn ) );
  }

  auto  stream_response ( Response && response ) -> void
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
  }

  auto  commit ( ) -> void
  {
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

// this is actually more of a filter factory factory. anyways ...
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
