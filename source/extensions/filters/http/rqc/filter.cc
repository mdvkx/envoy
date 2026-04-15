
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>
#include <type_traits>

namespace  Envoy::Extensions::HttpFilters::Rqc
{

      Filter::Filter ( std::shared_ptr<Cache>  cache )
  : m_Cache { cache }
{
}

auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  return  Http::FilterHeadersStatus::Continue;
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  return  Http::FilterHeadersStatus::Continue;
}

auto  Filter::encodeData     ( Buffer::Instance & data,
                               bool  is_last ) -> Http::FilterDataStatus
{
  return  Http::FilterDataStatus::Continue;
}

auto  Filter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus
{
  return  Http::FilterTrailersStatus::Continue;
}

auto  Filter::encodeComplete ( ) -> void
{
}

auto  Filter::onStreamComplete ( ) -> void
{
}

auto  Filter::onDestroy      ( ) -> void
{
}

}

namespace  Envoy::Extensions::HttpFilters::Rqc
{

REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
