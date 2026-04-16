
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>
#include <type_traits>

namespace  Envoy::Extensions::HttpFilters::Rqc
{

auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG ( debug, "@@@ destroy" );
}

auto  Filter::decodeHeaders ( Http::RequestHeaderMap & headers,
                              bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "@@@ decoding headers // stream id = {:08x}", this -> decoder_callbacks_ -> streamId () );
  return  Http::FilterHeadersStatus::Continue;
}

auto  Filter::encodeHeaders ( Http::ResponseHeaderMap & headers,
                              bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "@@@ encoding headers" );
  return  Http::FilterHeadersStatus::Continue;
}

auto  Filter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus
{
  ENVOY_LOG ( debug, "@@@ encoding trailers" );
  return  Http::FilterTrailersStatus::Continue;
}

auto  Filter::encodeData    ( Buffer::Instance & body,
                              bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG ( debug, "@@@ encoding {} bytes body", body . length () );
  return  Http::FilterDataStatus::Continue;
}

auto  Filter::derive_key ( const Http::RequestHeaderMap & headers ) -> std::string
{
  return  absl::StrCat ( headers . getSchemeValue (), headers . getHostValue (), headers . getPathValue () );
}


}

namespace  Envoy::Extensions::HttpFilters::Rqc
{

REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
