
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>

namespace  Envoy::Extensions::HttpFilters::Rqc {


auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "<{}> request: headers: [{}], is_last: {}",
                      static_cast<const void *> ( this ), headers, is_last );
  m_Key = absl::StrCat ( headers . getSchemeValue (), "://", headers . getHostValue (), headers . getPathValue () );
  ENVOY_LOG ( debug, "<{}> request: m_Key = \"{}\"", static_cast<const void *> ( this ), m_Key );
  std::size_t  cnt = 0;
  if ( m_Cache -> insert_or ( m_Key, [ ] ( ) { return  Pending {}; }, [ this, &cnt ] ( Pending & p ) -> void
  {
    p . m_Waiting . emplace_back ( [ this ] ( Msg && msg ) { this -> msg ( std::move ( msg ) ); } );
    cnt = p . m_Waiting . size ();  // dirty hack, temporary
  } ) )
  {
    m_State = State::Publisher;
    ENVOY_LOG ( debug, "<{}> request: publisher for key \"{}\"", static_cast<const void *> ( this ), m_Key );
    return  Http::FilterHeadersStatus::Continue;
  }
  else
  {
    m_State = State::Subscriber;
    ENVOY_LOG ( debug, "<{}> request: subscriber #{} for key \"{}\"", static_cast<const void *> ( this ), cnt, m_Key );
    return  Http::FilterHeadersStatus::StopAllIterationAndWatermark;
  }
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "<{}> response: headers: [{}], is_last: {}", static_cast<const void *> ( this ), headers, is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterHeadersStatus::Continue;
      break;
    case  State::Publisher:
      assert ( ! m_Pending . has_value () );
      m_Pending = m_Cache -> remove ( m_Key );
      assert ( m_Pending . has_value () );
      ENVOY_LOG ( debug, "<{}> removing pending request for key \"{}\" ({} subscribers).",
                         static_cast<const void *> ( this ), m_Key, m_Pending -> m_Waiting . size () );
      for ( auto & send_msg : m_Pending -> m_Waiting )
      {
        send_msg ( MsgHeaders { Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers ), is_last } );
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
  ENVOY_LOG ( debug, "<{}> response: data: \"{}\", is_last: {}",
                     static_cast<const void *> ( this ), data . toString (), is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterDataStatus::Continue;
      break;
    case  State::Publisher:
      assert ( m_Pending . has_value () );
      for ( auto & send_msg : m_Pending -> m_Waiting )
        send_msg ( MsgBody {} );
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
  ENVOY_LOG ( debug, "<{}> response: trailers: [{}]", static_cast<const void *> ( this ), trailers );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterTrailersStatus::Continue;
      break;
    case  State::Publisher:
      assert ( m_Pending . has_value () );
      for ( auto & send_msg : m_Pending -> m_Waiting )
        send_msg ( MsgTrailers {} );
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

auto  Filter::onStreamComplete ( ) -> void
{
  ENVOY_LOG ( debug, "<{}> stream complete ()", static_cast<const void *> ( this ) );
}

auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG ( debug, "<{}> on destroy ()", static_cast<const void *> ( this ) );
}

auto  Filter::msg ( Msg && msg ) -> void
{
  std::visit ( [ this ] ( auto && msg ) -> void
  {
    using  T = std::remove_cvref_t<decltype ( msg )>;
    if      constexpr ( std::is_same_v<T, MsgHeaders> )
    {
      this -> post ( [ this, msg = std::move ( msg ) ] ( ) mutable -> void
      {
        ENVOY_LOG ( debug, "<{}> received headers",  static_cast<const void *> ( this ) );
        this -> decoder_callbacks_ -> encodeHeaders ( std::move ( msg . m_Headers ), msg . m_IsLast );
      } );
    }
    else if constexpr ( std::is_same_v<T, MsgBody> )
    {
      this -> post ( [ this ] ( ) mutable -> void
      {
        ENVOY_LOG ( debug, "<{}> received body",     static_cast<const void *> ( this ) );
      } );
    }
    else if constexpr ( std::is_same_v<T, MsgTrailers> )
    {
      this -> post ( [ this ] ( ) mutable -> void
      {
        ENVOY_LOG ( debug, "<{}> received trailers", static_cast<const void *> ( this ) );
      } );
    }
    else
    {
      assert ( 0 );
    }
  }, std::move ( msg ) );
}


REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
