
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>
#include <type_traits>

namespace  Envoy::Extensions::HttpFilters::Rqc {


template <typename  Enum>
[[nodiscard]]
constexpr auto  to_underlying ( Enum e ) noexcept -> std::underlying_type_t<Enum>
{
  return  static_cast<std::underlying_type_t<Enum> > ( e );
}


auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "decoding: state = {}, headers = [host=\"{}\", path=\"{}\", ...], is_last = {}",
                      to_underlying ( m_State ), headers . getHostValue (), headers . getPathValue (), is_last );
  m_Key = absl::StrCat ( headers . getSchemeValue (), "://", headers . getHostValue (), headers . getPathValue () );
  std::size_t  cnt = 0;
  if ( m_Cache -> insert_or ( m_Key, [ ] ( ) { return  Pending {}; }, [ this, &cnt ] ( Pending & p ) -> void
  {
    p . m_Waiting . emplace_back ( [ p = this -> shared_from_this () ] ( Msg && msg ) mutable -> void
    {
      p -> msg ( std::move ( msg ) );
    } );
    cnt = p . m_Waiting . size ();  // dirty hack, temporary
  } ) )
  {
    m_State = State::Publisher;
    ENVOY_LOG ( debug, "request: publisher for key \"{}\"", m_Key );
    return  Http::FilterHeadersStatus::Continue;
  }
  else
  {
    m_State = State::Subscriber;
    ENVOY_LOG ( debug, "request: subscriber #{} for key \"{}\"", cnt, m_Key );
    return  Http::FilterHeadersStatus::StopAllIterationAndWatermark;
  }
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "encoding: state = {}, headers = [{}, ...], is_last = {}", to_underlying ( m_State ), headers . getStatusValue (), is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterHeadersStatus::Continue;
      break;
    case  State::Publisher:
      assert ( ! m_Pending . has_value () );
      m_Pending = m_Cache -> remove ( m_Key );
      assert ( m_Pending . has_value () );
      ENVOY_LOG ( debug, "pending request for key \"{}\" sealed with {} subscribers.", m_Key, m_Pending -> m_Waiting . size () );
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
  ENVOY_LOG ( debug, "encoding: state = {}, {} bytes of data; is_last = {}",
                     to_underlying ( m_State ), data . length (), is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterDataStatus::Continue;
      break;
    case  State::Publisher:
      assert ( m_Pending . has_value () );
      for ( auto & send_msg : m_Pending -> m_Waiting )
      {
        send_msg ( MsgBody { std::make_unique<Buffer::OwnedImpl> ( data ), is_last } );
      }
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
  ENVOY_LOG ( debug, "encoding: state = {}, trailers", to_underlying ( m_State ) );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterTrailersStatus::Continue;
      break;
    case  State::Publisher:
      assert ( m_Pending . has_value () );
      for ( auto & send_msg : m_Pending -> m_Waiting )
      {
        send_msg ( MsgTrailers {} );
      }
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

auto  Filter::encodeComplete ( ) -> void
{
  ENVOY_LOG ( debug, "state = {}, encode complete", to_underlying ( m_State ) );
}

auto  Filter::onStreamComplete ( ) -> void
{
  ENVOY_LOG ( debug, "state = {}, on stream complete", to_underlying ( m_State ) );
}

auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG ( debug, "state = {}, on destroy", to_underlying ( m_State ) );
}

auto  Filter::msg ( Msg && msg ) -> void
{
  std::visit ( [ this ] ( auto && msg ) -> void
  {
    using  T = std::remove_cvref_t<decltype ( msg )>;
    if constexpr ( std::is_same_v<T, MsgHeaders> )
    {
      this -> post ( [ this, msg = std::move ( msg ) ] ( ) mutable -> void
      {
        this -> decoder_callbacks_ -> encodeHeaders ( std::move ( msg . m_Headers ), msg . m_IsLast, "details" );
      } );
    }
    else if constexpr ( std::is_same_v<T, MsgBody> )
    {
      this -> post ( [ this, msg = std::move ( msg ) ] ( ) mutable -> void
      {
        this -> decoder_callbacks_ -> encodeData    ( *msg . m_Body, msg . m_IsLast );
      } );
    }
    else if constexpr ( std::is_same_v<T, MsgTrailers> )
    {
      /*
      this -> post ( [ this ] ( ) mutable -> void
      {
      } );
       */
    }
    else
    {
      assert ( 0 );
    }
  }, std::move ( msg ) );
}


REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
