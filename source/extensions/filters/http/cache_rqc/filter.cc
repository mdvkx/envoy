
#include "filter.h"

#include "envoy/server/filter_config.h"  // NamedHttpFilterConfigFactory

namespace  Envoy::Extensions::HttpFilters::CacheRqC
{

REGISTER_FACTORY ( FilterFactory, Server::Configuration::NamedHttpFilterConfigFactory );

}
