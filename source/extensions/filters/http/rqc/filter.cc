
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>
#include <type_traits>

namespace  Envoy::Extensions::HttpFilters::Rqc
{

auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG (
    debug,
    "@@@ [stream id = {:08x}] destroy",
    this -> decoder_callbacks_ -> streamId ()
  );
}

auto  Filter::decodeHeaders ( Http::RequestHeaderMap & headers,
                              bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG (
    debug,
    "@@@ [stream id = {:08x}] decoding headers",
    this -> decoder_callbacks_ -> streamId ()
  );

  return  Http::FilterHeadersStatus::Continue;
}

auto  Filter::encodeHeaders ( Http::ResponseHeaderMap & headers,
                              bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG (
    debug,
    "@@@ [stream id = {:08x}] encoding headers",
    this -> decoder_callbacks_ -> streamId ()
  );

  return  Http::FilterHeadersStatus::Continue;
}

auto  Filter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus
{
  ENVOY_LOG (
    debug,
    "@@@ [stream id = {:08x}] encoding trailers",
    this -> decoder_callbacks_ -> streamId ()
  );

  return  Http::FilterTrailersStatus::Continue;
}

auto  Filter::encodeData    ( Buffer::Instance & body,
                              bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG (
    debug,
    "@@@ [stream id = {:08x}] encoding {} bytes body",
    this -> decoder_callbacks_ -> streamId (), body . length ()
  );

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
