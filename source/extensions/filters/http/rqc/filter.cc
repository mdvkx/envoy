
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

namespace  Envoy::Extensions::HttpFilters::Rqc {


auto  Filter::onDestroy ( ) -> void
{
}

auto  Filter::onStreamComplete ( ) -> void
{
}

auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "" );

  this -> touch ( headers );
  assert ( 0 );
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
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
  assert ( 0 );
}

}
