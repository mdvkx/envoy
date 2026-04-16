
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>
#include <type_traits>

template <typename  Enum_>
[[nodiscard]]
constexpr auto  to_underlying ( Enum_  e ) noexcept -> std::underlying_type_t<Enum_>
{
  static_assert ( std::is_enum_v<Enum_>,
                  "the argument e must be a complete enumeration type" );
  return  static_cast<std::underlying_type_t<Enum_> >  ( e );
}

namespace  Envoy::Extensions::HttpFilters::Rqc
{

auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG (
    trace,
    "@@@ destroy  // state = {}, stream id = {:08x}",
    to_underlying ( m_State ),
    this -> decoder_callbacks_ -> streamId ()
  );

}

auto  Filter::decodeHeaders ( Http::RequestHeaderMap & headers,
                              bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG (
    trace,
    "@@@ decoding headers  // state = {}, stream id = {:08x}",
    to_underlying ( m_State ),
    this -> decoder_callbacks_ -> streamId ()
  );

  m_Key = Self::derive_key ( headers );
  if ( ! m_Cache -> insert_or ( m_Key, [ ] ( ) { return  Ticket {}; }, [ this ] ( Ticket & x ) -> void
  {
    x . m_Waiting . emplace_back ( this -> shared_from_this () );
  } ) )
  {
    m_State = State::Subscriber;
    return  Http::FilterHeadersStatus::StopIteration;
  }
  else
  {
    m_State = State::Publisher;
    return  Http::FilterHeadersStatus::Continue;
  }
}

auto  Filter::encodeHeaders ( Http::ResponseHeaderMap & headers,
                              bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG (
    trace,
    "@@@ encoding headers  // state = {}, stream id = {:08x}",
    to_underlying ( m_State ),
    this -> decoder_callbacks_ -> streamId ()
  );

  switch ( m_State )
  {
    case  State::Publisher:
      if ( auto  x = m_Cache -> remove ( m_Key );
           x . has_value () )
      {
        m_Waiting = std::move ( x -> m_Waiting );
        auto  msg = std::make_shared<const Msg> ( MsgHeaders { Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers ), is_last } );
        for ( auto  w : m_Waiting )
          w -> receive_msg ( msg );
      }
      else
      {
        assert ( 0 );
      }
      return  Http::FilterHeadersStatus::Continue;
      break;
    case  State::Subscriber:
      return  Http::FilterHeadersStatus::Continue;
      break;
    default:
      assert ( 0 && "unreachable!" );
      break;
  }
}

auto  Filter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus
{
  ENVOY_LOG (
    trace,
    "@@@ encoding trailers  // state = {}, stream id = {:08x}",
    to_underlying ( m_State ),
    this -> decoder_callbacks_ -> streamId ()
  );

  switch ( m_State )
  {
    case  State::Publisher:
      return  Http::FilterTrailersStatus::Continue;
      break;
    case  State::Subscriber:
      return  Http::FilterTrailersStatus::Continue;
      break;
    default:
      assert ( 0 && "unreachable!" );
      break;
  }
}

auto  Filter::encodeData    ( Buffer::Instance & body,
                              bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG (
    trace,
    "@@@ encoding {} bytes body  // state = {}, stream id = {:08x}",
    body . length (),
    to_underlying ( m_State ),
    this -> decoder_callbacks_ -> streamId ()
  );

  switch ( m_State )
  {
    case  State::Publisher:
    {
      auto  msg = std::make_shared<const Msg> ( MsgBody { std::make_unique<Buffer::OwnedImpl> ( body ), is_last } );
      for ( auto  w : m_Waiting )
        w -> receive_msg ( msg );
      return  Http::FilterDataStatus::Continue;
      break;
    }
    case  State::Subscriber:
      return  Http::FilterDataStatus::Continue;
      break;
    default:
      assert ( 0 && "unreachable!" );
      break;
  }
}

auto  Filter::derive_key ( const Http::RequestHeaderMap & headers ) -> std::string
{
  return  absl::StrCat ( headers . getSchemeValue (), headers . getHostValue (), headers . getPathValue () );
}

auto  Filter::receive_msg ( std::shared_ptr<const Msg>  msg ) -> void
{
  this -> decoder_callbacks_ -> dispatcher () . post ( [ this, msg, wp = this -> weak_from_this () ] ( ) -> void
  {
    auto  p = wp . lock ();
    if ( ! p )
      return;

    std::visit ( [ this ] ( const auto & x ) -> void
    {
      using  X = std::remove_cvref_t<decltype ( x )> ;
      if constexpr ( std::is_same_v<X, MsgHeaders> )
      {
        auto  headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *x . m_Headers );
        this -> decoder_callbacks_ -> encodeHeaders ( std::move ( headers ), x . m_Last, "hulahoop" );
      }
      else if constexpr ( std::is_same_v<X, MsgTrailers> )
        assert ( 0 );
      else if constexpr ( std::is_same_v<X, MsgBody> )
      {
        auto  body = std::make_unique<Buffer::OwnedImpl> ( *x . m_Body );
        this -> decoder_callbacks_ -> encodeData    ( *body, x . m_Last );
      }
      else
        assert ( 0 );
    }, *msg );
  } );

  /*
  std::visit ( [ this ] ( const auto & x ) -> void
  {
    using  X = std::remove_cvref_t<decltype ( x )> ;
    if constexpr ( std::is_same_v<X, MsgHeaders> )
    {
      this -> decoder_callbacks_ -> dispatcher () . post ( [ this, msg, wp = this -> weak_from_this () ] ( ) -> void
      {
        auto  p = wp . lock ();
        if ( ! p )
          return;
        const auto & mx = std::get<MsgHeaders> ( *msg );
        auto  headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( *mx . m_Headers );
        this -> decoder_callbacks_ -> encodeHeaders ( std::move ( headers ), mx . m_Last, "hulahoop" );
      } );
    }
    else if constexpr ( std::is_same_v<X, MsgTrailers> )
      assert ( 0 );
    else if constexpr ( std::is_same_v<X, MsgBody> )
      assert ( 0 );
    else
      assert ( 0 );
  }, *msg );
  */
}


}

namespace  Envoy::Extensions::HttpFilters::Rqc
{

REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
