
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>
#include <type_traits>

template <typename  Enum_>
[[nodiscard]]
constexpr auto  to_underlying ( Enum_  e ) noexcept -> std::underlying_type_t<Enum_>
{
  static_assert ( std::is_enum_v<Enum_>,
                  "e must be a complete enumeration type" );
  return  static_cast<std::underlying_type_t<Enum_> >  ( e );
}

namespace  Envoy::Extensions::HttpFilters::Rqc
{

auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG (
    debug,
    "@@@ destroy  // state = {}, stream id = {:08x}",
    to_underlying ( m_State ),
    this -> decoder_callbacks_ -> streamId ()
  );

}

auto  Filter::decodeHeaders ( Http::RequestHeaderMap & headers,
                              bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG (
    debug,
    "@@@ decoding headers  // state = {}, stream id = {:08x}",
    to_underlying ( m_State ),
    this -> decoder_callbacks_ -> streamId ()
  );

  m_Key = Self::derive_key ( headers );
  if ( ! m_Cache -> insert_or ( m_Key, [ ] ( ) { return  Ticket {}; }, [ this ] ( Ticket & x ) -> void
  {
    x . m_Waiting . emplace_back ( this -> shared_from_this () );
  } )
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
    debug,
    "@@@ encoding headers  // state = {}, stream id = {:08x}",
    to_underlying ( m_State ),
    this -> decoder_callbacks_ -> streamId ()
  );

  switch ( m_State )
  {
    case  State::Publisher:
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
    debug,
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
    debug,
    "@@@ encoding {} bytes body  // state = {}, stream id = {:08x}",
    body . length (),
    to_underlying ( m_State ),
    this -> decoder_callbacks_ -> streamId ()
  );

  switch ( m_State )
  {
    case  State::Publisher:
      return  Http::FilterDataStatus::Continue;
      break;
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


}

namespace  Envoy::Extensions::HttpFilters::Rqc
{

REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
