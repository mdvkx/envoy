
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>

namespace  Envoy::Extensions::HttpFilters::Rqc {


auto  Filter::onDestroy ( ) -> void
{
}

auto  Filter::onStreamComplete ( ) -> void
{
}

auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "request: headers: [{}], is_last: {}", headers, is_last );
  m_Key = absl::StrCat ( headers . getSchemeValue (), "://", headers . getHostValue (), headers . getPathValue () );
  ENVOY_LOG ( debug, "request: m_Key = \"{}\"", m_Key );
  std::size_t  q = 0;
  if ( m_Cache -> insert_or ( m_Key, [ ] ( ) { return  std::size_t { 0 }; }, [ &q ] ( std::size_t  c ) mutable -> void
  {
    q = c;  // dirty hack, temporary
    c += 1;
  } ) )
  {
    ENVOY_LOG ( debug, "request: creating new pending request for key \"{}\"", m_Key );
  }
  else
  {
    ENVOY_LOG ( debug, "request: request for key \"{}\" already pending with {} subscribers in queue excluding myself", m_Key, q );
  }
  return  Http::FilterHeadersStatus::Continue;
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "response: headers: [{}], is_last: {}", headers, is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterHeadersStatus::Continue;
      break;
    default:
      assert ( 0 && "unreachable" );
      break;
  }
}

auto  Filter::encodeData     ( Buffer::Instance & data,
                               bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG ( debug, "response: data: \"{}\", is_last: {}", data . toString (), is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterDataStatus::Continue;
      break;
    default:
      assert ( 0 && "unreachable" );
      break;
  }
}

auto  Filter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus
{
  ENVOY_LOG ( debug, "response: trailers: [{}]", trailers );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterTrailersStatus::Continue;
      break;
    default:
      assert ( 0 && "unreachable" );
      break;
  }
}


REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
