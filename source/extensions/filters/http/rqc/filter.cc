
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
  ENVOY_LOG ( debug, "on steam complete ()" );
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
    p . m_Waiting . emplace_back ( [ this ] ( Msg && msg ) mutable -> void
    {
      ENVOY_LOG ( debug, "(subscriber {}) received a message", this );
      std::visit ( [ this ] ( auto && x )
      {
        using  T = std::remove_cvref_t<decltype ( x )>;
        if      constexpr ( std::is_same_v<T, MsgHeaders> )
          this -> post ( [ this, x = std::move ( x ) ] ( ) mutable -> void
          {
            ENVOY_LOG ( debug, "(subscriber {}) headers received", this );
          } );
        else if constexpr ( std::is_same_v<T, MsgBody> )
          this -> post ( [ this, x = std::move ( x ) ] ( ) mutable -> void
          {
            ENVOY_LOG ( debug, "(subscriber {}) body received", this );
          } );
        else if constexpr ( std::is_same_v<T, MsgTrailers> )
          this -> post ( [ this, x = std::move ( x ) ] ( ) mutable -> void
          {
            ENVOY_LOG ( debug, "(subscriber {}) trailers received", this );
          } );
        else
          assert ( 0 );
      }, std::move ( msg ) );
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
      assert ( ! m_Pending . has_value () );
      m_Pending = m_Cache -> remove ( m_Key );
      assert ( m_Pending . has_value () );
      ENVOY_LOG ( debug, "response: removing \"{}\" from pending, there are {} subscribers attached.", m_Key, m_Pending -> m_Waiting . size () );
      for ( auto & w : m_Pending -> m_Waiting )
      {
        w ( MsgHeaders {} );
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
      assert ( m_Pending . has_value () );
      for ( auto & w : m_Pending -> m_Waiting )
        w ( MsgBody {} );
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
      assert ( m_Pending . has_value () );
      for ( auto & w : m_Pending -> m_Waiting )
        w ( MsgTrailers {} );
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
