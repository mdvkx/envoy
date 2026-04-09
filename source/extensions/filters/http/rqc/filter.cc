
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>

namespace  Envoy::Extensions::HttpFilters::Rqc {


auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG ( debug, "on destroy ()" );
}

auto  Filter::onStreamComplete ( ) -> void
{
}

auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "decode headers = {}, is last = {}", headers, is_last );

  this -> touch ( headers );
  assert ( 0 );
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "encode headers = {}, is last = {}", headers, is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      assert ( 0 );
      break;
    case  State::Publisher:
      assert ( 0 );
      break;
    case  State::Subscriber:
      assert ( 0 );
      break;
    default:
      assert ( 0 && "unreachable" );
      break;
  }
}

auto  Filter::encodeData     ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG ( debug, "encode data = {}, is last = {}", data . toString (), is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      assert ( 0 );
      break;
    case  State::Publisher:
      assert ( 0 );
      break;
    case  State::Subscriber:
      assert ( 0 );
      break;
    default:
      assert ( 0 && "unreachable" );
      break;
  }
}

auto  Filter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus
{
  ENVOY_LOG ( debug, "encode trailers = {}", trailers );
  switch ( m_State )
  {
    case  State::Unknown:
      assert ( 0 );
      break;
    case  State::Publisher:
      assert ( 0 );
      break;
    case  State::Subscriber:
      assert ( 0 );
      break;
    default:
      assert ( 0 && "unreachable" );
      break;
  }
}

auto  Filter::touch  ( Http::RequestHeaderMap & headers ) -> void
{
  ENVOY_LOG ( debug, "touch ()" );
}

REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
