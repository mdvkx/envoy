#pragma once

#include "./ring.h"

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
  Initial = 0,
  Destroyed = 1,
  NotCacheable = 2,
  Miss = 100,
  Hit  = 200,
};

struct  Response
{
  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::unique_ptr<Buffer::Instance>  m_Body = nullptr;
  Envoy::SystemTime  m_Stamp;
};

/*
using  Cache = ConcurrentHashMap<std::string, std::shared_ptr<const Response> >;
*/

struct  Cache
{
  using  Key1  = std::string;
  using  Key2  = std::string;
  using  Key   = std::pair<Key1, Key2>;
  using  Value = std::shared_ptr<const Response>;
  using  Entry = std::pair<Key2, Value>;
  mutable std::mutex  m_Mtx;
  std::unordered_map<Key1, Ring<Entry, 4> >  m_Rings;
  [[nodiscard]]
  auto  lookup ( const Key & k ) const -> std::optional<Value>
  {

    auto  l = std::unique_lock { m_Mtx };
    const auto & [ k1, k2 ] = k;
    auto  i = m_Rings . find ( k1 );
    if ( i == m_Rings . end () )
      return  std::nullopt;
    const auto & ring = i -> second;
    for ( std::size_t  i = 0; i < ring . size (); i ++ )
    {
      const auto & [ xk, xv ] = ring [ (ring . size () - (i + 1)) ];  // go in reverse, latest to oldest
      if ( xk == k2 )
        return  xv;
    }
    return  std::nullopt;
  }
  auto  insert_or_assign ( const Key & k, const std::function<Value ()> & v ) -> void
  {
    auto  l = std::unique_lock { m_Mtx };
    const auto & [ k1, k2 ] = k;
    m_Rings [ k1 ] . push ( { k2, v () } );  // note: the ring is implicitely default constructed if it didn't exist
  }
};

struct  Filter : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::filter>, public std::enable_shared_from_this<Filter>
{
  using  Self = Filter;

  using  Key = typename  Cache::Key;

  std::shared_ptr<Cache>  m_Cache;
  Key  m_Key;

  State  m_State = State::Initial;

  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::unique_ptr<Buffer::Instance>  m_Body = nullptr;
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
  static auto  derive_key ( const Http::RequestHeaderMap & headers ) -> Key;

  auto  commit ( ) -> void;
};

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
