
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>

namespace  Envoy::Extensions::HttpFilters::Rqc {


auto  Pending::publish_headers ( const Http::ResponseHeaderMap & headers, bool  is_last ) -> void
{
  for ( auto & w : m_Waiting )
    w -> post_headers ( headers, is_last );
}

auto  Pending::publish_trailers ( const Http::ResponseTrailerMap & trailers ) -> void
{
  for ( auto & w : m_Waiting )
    w -> post_trailers ( trailers );
}

auto  Pending::publish_data ( const std::string & data, bool is_last ) -> void
{
  for ( auto & w : m_Waiting )
    w -> post_data ( data, is_last );
}

auto  Pending::subscribe ( std::shared_ptr<Filter>  x ) -> void
{
  m_Waiting . emplace_back ( x );
}


auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG ( debug, "on destroy ()" );
  m_State = State::Destroyed;
}

auto  Filter::onStreamComplete ( ) -> void
{
  ENVOY_LOG ( debug, "on stream complete ()" );
}

auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "decode headers = {}, is last = {}", headers, is_last );
  m_Key =  absl::StrCat ( headers . getSchemeValue (), "://", headers . getHostValue (), headers . getPathValue () );
  if ( m_Collapser -> insert_or ( m_Key, [ ] ( ) { return  Pending {}; }, [ this ] ( Pending & p ) -> void
  {
    p . subscribe ( this -> shared_from_this () );
  } ) )
  {
    ENVOY_LOG ( debug, "publisher" );
    m_State = State::Publisher;
    return  Http::FilterHeadersStatus::Continue;
  }
  else
  {
    ENVOY_LOG ( debug, "subscriber" );
    m_State = State::Subscriber;
    return  Http::FilterHeadersStatus::StopIteration;
  }
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "encode headers = {}, is last = {}", headers, is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterHeadersStatus::Continue;
      break;
    case  State::Publisher:
      if ( m_Channel )    // this is a weird case where the publisher receives the headers they published earlier. i dunno, envoy.
        return  Http::FilterHeadersStatus::Continue;
      // no more subscribers are accepted after this point
      m_Channel = m_Collapser -> remove ( m_Key );
      assert ( m_Channel . has_value () && "only publishers are allowed to remove an entry" );
      m_Channel -> publish_headers ( headers, is_last );
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

auto  Filter::encodeData     ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG ( debug, "encode data = {}, is last = {}", data . toString (), is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterDataStatus::Continue;
      break;
    case  State::Publisher:
      if ( m_Channel )
        m_Channel -> publish_data ( data . toString (), is_last );
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
  ENVOY_LOG ( debug, "encode trailers = {}", trailers );
  switch ( m_State )
  {
    case  State::Unknown:
      return  Http::FilterTrailersStatus::Continue;
      break;
    case  State::Publisher:
      if ( m_Channel )
        m_Channel -> publish_trailers ( trailers );
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

auto  Filter::post_headers   ( const Http::ResponseHeaderMap & headers,
                               bool  is_last ) -> void
{
  this -> post ( [ this, headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers ), is_last ] ( ) mutable -> void
  {
    this -> decoder_callbacks_ -> encodeHeaders  ( std::move ( headers ), is_last, "i've no idea what this \"details\" argument is for" );
  } );
}

auto  Filter::post_data      ( const std::string & data,
                               bool  is_last ) -> void
{
  this -> post ( [ this, data = Buffer::OwnedImpl { data }, is_last ] ( ) mutable -> void
  {
    this -> decoder_callbacks_ -> encodeData     ( data, is_last );
  } );
}

auto  Filter::post_trailers  ( const Http::ResponseTrailerMap & trailers ) -> void
{
  this -> post ( [ this, trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers ) ] ( ) mutable -> void
  {
    this -> decoder_callbacks_ -> encodeTrailers ( std::move ( trailers ) );
  } );
}


REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
