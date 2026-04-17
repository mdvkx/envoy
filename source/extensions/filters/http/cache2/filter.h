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
  Initial,
  NotCacheable,
  Miss,
  Hit,
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
  using  Key = std::string;
  using  Value = std::shared_ptr<const Response>;
  using  Entry = std::pair<Key, Value>;

  mutable std::mutex  m_Mtx;
  Ring<Entry, 10>     m_Ring;

  [[nodiscard]]
  auto  lookup ( const Key & k ) const -> std::optional<Value>
  {
    auto  l = std::unique_lock { m_Mtx };
    // go in reverse, latest to oldest
    for ( std::size_t  i = 0; i < m_Ring . size (); i ++ )
      if ( const auto & [ xk, xv ] = m_Ring [ (m_Ring . size () - (i + 1)) ];
           xk == k )
        return  xv;
    return  std::nullopt;
  }
  auto  insert_or_assign ( const Key & k,
                           const std::function<Value ()> & v ) -> void
  {
    auto  l = std::unique_lock { m_Mtx };
    m_Ring . push ( std::make_pair ( k, v () ) );  // TODO: doesn't really have the "assign" part to it, does it
  }
};

struct  Filter : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::filter>, public std::enable_shared_from_this<Filter>
{
  using  Self = Filter;

  std::shared_ptr<Cache>  m_Cache;
  std::string  m_Key;

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
  static auto  derive_key ( const Http::RequestHeaderMap & headers ) -> std::string;

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
