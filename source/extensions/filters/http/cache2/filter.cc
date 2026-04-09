
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>
#include <chrono>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace  Envoy::Extensions::HttpFilters::Cache2 {

static const auto  CACHEABLE_STATUS_CODES = std::unordered_set<std::string_view>
{  //  taken from file://./../cache/cacheability_utils.cc
  "200", "203", "204", "206",
  "300", "301", "308",
  "404", "405", "410", "414", "451",
  "501",
};

auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG ( debug, "on destroy ()" );
  m_State = State::Destroyed;
}

auto  Filter::onStreamComplete ( ) -> void
{
}

auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "decode headers = {}, is last = {}", headers, is_last );

  using  namespace std::literals;

  // is the request even cacheable
  if (
    headers . Host () == nullptr  // missing host
    || headers . Path () == nullptr  // missing resource id
    || headers . getMethodValue () != "GET"sv  // only get requests are cacheable, TODO: uppercase is important, find a more foolproof way
    || ! is_last  // requests with body/trailers aren't cacheable
  )
  {
    m_State = State::N_A;
    return  Http::FilterHeadersStatus::Continue;
  }

  m_Key =  absl::StrCat ( headers . getSchemeValue (), "://", headers . getHostValue (), headers . getPathValue () );
  ENVOY_LOG ( debug, "key = \"{}\"", m_Key );

  auto  response = m_Cache -> lookup ( m_Key );

  if (
    ! response
    || std::chrono::system_clock::now () - (*response) -> m_Stamp > 60s  // pretend entries older than 60s are expired
  )
  {
    ENVOY_LOG ( debug, "cache miss" );
    m_State = State::Miss;
    return  Http::FilterHeadersStatus::Continue;
  }

  m_State = State::Hit;
  ENVOY_LOG ( debug, "cache hit" );
  // TODO: maybe use sendLocalReply() instead?
  this -> post ( [ this, response = (*response) ] ( ) -> void
  {
    const auto  is_last = response -> m_Data . empty () && response -> m_Trailers == nullptr;
    this -> decoder_callbacks_ -> encodeHeaders  ( Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( * response -> m_Headers ), is_last, "i've no idea what this \"details\" argument is for" );
  } );
  this -> post ( [ this, response = (*response) ] ( ) -> void
  {
    auto  data = Buffer::OwnedImpl { response -> m_Data };
    const auto  is_last = response -> m_Trailers == nullptr;
    this -> decoder_callbacks_ -> encodeData     ( data, is_last );
  } );
  this -> post ( [ this, response = (*response) ] ( ) -> void
  {
    this -> decoder_callbacks_ -> encodeTrailers ( Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( * response -> m_Trailers ) );
  } );
  return  Http::FilterHeadersStatus::StopAllIterationAndWatermark;
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "encode headers = {}, is last = {}", headers, is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      assert ( 0 );
      break;
    case  State::N_A:
      return  Http::FilterHeadersStatus::Continue;
      break;
    case  State::Hit:
      return  Http::FilterHeadersStatus::Continue;
      break;
    case  State::Miss:
      if (
        ! CACHEABLE_STATUS_CODES . contains ( headers . getStatusValue () )
        // || no cache-control, etc ...
      )
      {
        m_State = State::N_A;
        return  Http::FilterHeadersStatus::Continue;
      }
      m_Headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( headers );
      m_Stamp  = std::chrono::system_clock::now ();
      if ( is_last )
        this -> commit ();
      return  Http::FilterHeadersStatus::Continue;
      break;
    default:
      assert ( 0 && "unreachable" );
      break;
  }
}

auto  Filter::encodeData     ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG ( debug, "encode data = \"{}\", is last = {}", data . toString (), is_last );
  switch ( m_State )
  {
    case  State::Unknown:
      assert ( 0 );
      break;
    case  State::N_A:
      return  Http::FilterDataStatus::Continue;
      break;
    case  State::Hit:
      return  Http::FilterDataStatus::Continue;
      break;
    case  State::Miss:
      m_Data += data . toString ();
      if ( is_last )
        this -> commit ();
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
      assert ( 0 );
      break;
    case  State::N_A:
      return  Http::FilterTrailersStatus::Continue;
      break;
    case  State::Hit:
      return  Http::FilterTrailersStatus::Continue;
      break;
    case  State::Miss:
      m_Trailers = Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( trailers );
      this -> commit ();
      return  Http::FilterTrailersStatus::Continue;
      break;
    default:
      assert ( 0 && "unreachable" );
      break;
  }
}

auto  Filter::commit  ( ) -> void
{
  m_Cache -> insert ( m_Key, std::make_shared<const Response> ( std::move ( m_Headers ), std::move ( m_Trailers ), std::move ( m_Data ), std::move ( m_Stamp ) ) );
}


REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );


}
