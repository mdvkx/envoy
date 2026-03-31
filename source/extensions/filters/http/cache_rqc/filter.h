#pragma once

#include "source/common/common/logger.h"  // Loggable, Id
#include "source/extensions/filters/http/cache_rqc/config.pb.h"
#include "source/extensions/filters/http/cache_rqc/config.pb.validate.h"
#include "source/extensions/filters/http/common/factory_base.h"  // FactoryBase<>
#include "source/extensions/filters/http/common/pass_through_filter.h"  // PassThroughFilter

#include <memory>
#include <string>

namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

struct  Filter;
struct  FilterFactory;

}

namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

struct  Filter
  : public Http::PassThroughFilter, public Logger::Loggable<Logger::Id::cache_filter>, public std::enable_shared_from_this<Filter>
{
  Filter ( )
  {
    ENVOY_LOG ( debug, "Filter ()" );
  }

  ~Filter ( ) override
  {
    ENVOY_LOG ( debug, "~Filter ()" );
  }

};

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
    return  [ ] ( Http::FilterChainFactoryCallbacks & callbacks ) -> void
    {
      callbacks . addStreamFilter ( std::make_shared<Filter> () );
    };
  }

  FilterFactory ( )
    : Base { "envoy.filters.http.cache_rqc" }
  {
  }

};

}
