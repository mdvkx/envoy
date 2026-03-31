#pragma once

#include "source/extensions/filters/http/cache_rqc/config.pb.h"
#include "source/extensions/filters/http/cache_rqc/config.pb.validate.h"
#include "source/extensions/filters/http/common/factory_base.h"  // Common::FactoryBase<>

namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

struct  Filter;
struct  FilterFactory;

}

namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

struct  Filter
  : public Http::StreamFilter, public std::enable_shared_from_this<Filter>
{
};

struct  FilterFactory
  : public Common::FactoryBase<envoy::extensions::filters::http::cache_rqc::Config>
{
};

}
