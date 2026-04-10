
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
  if ( m_Cache -> insert_or ( m_Key, [ ] ( ) { return  Pending {}; }, [ this, &q ] ( Pending & p ) -> void
  {
    q = p . m_Waiting . size ();  // dirty hack, temporary
    p . m_Waiting . emplace_back ( [ this ] ( ) mutable -> void
    {
      ENVOY_LOG ( debug, "(subscriber) received a message" );
    } );
  } ) )
  {
    m_State = State::Publisher;
    ENVOY_LOG ( debug, "request: creating new pending request for key \"{}\"", m_Key );
    return  Http::FilterHeadersStatus::Continue;
  }
  else
  {
    m_State = State::Subscriber;
    ENVOY_LOG ( debug, "request: request for key \"{}\" already pending with {} subscribers in queue excluding myself", m_Key, q );
    return  Http::FilterHeadersStatus::StopAllIterationAndWatermark;
  }
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
    case  State::Publisher:
      if ( auto  x = m_Cache -> remove ( m_Key );  x . has_value () )
      {
        ENVOY_LOG ( debug, "response: removing \"{}\" from pending, there are {} subscribers attached.", m_Key, *x );

      }
      else
      {
        ENVOY_LOG ( debug, "response: \"{}\" was not in cache, weird!", m_Key );
      }
      return  Http::FilterHeadersStatus::Continue;
      break;
    case  State::Subscriber:
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
    case  State::Publisher:
      return  Http::FilterDataStatus::Continue;
      break;
    case  State::Subscriber:
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
    case  State::Publisher:
      return  Http::FilterTrailersStatus::Continue;
      break;
    case  State::Subscriber:
      return  Http::FilterTrailersStatus::Continue;
      break;
    default:
      assert ( 0 && "unreachable" );
      break;
  }
}


REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
