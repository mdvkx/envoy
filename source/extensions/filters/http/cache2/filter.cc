
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>
#include <chrono>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace  Envoy::Extensions::HttpFilters::Cache2
{

static const auto  CACHEABLE_STATUS_CODES = std::unordered_set<std::string_view>
{  //  taken from file://./../cache/cacheability_utils.cc
  "200", "203", "204", "206",
  "300", "301", "308",
  "404", "405", "410", "414", "451",
  "501",
};

auto  Filter::onDestroy ( ) -> void
{
  ENVOY_LOG ( trace, "on destroy" );
}

auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( trace, "decoding: headers = [host=\"{}\", path=\"{}\", is last = {}", headers . getHostValue (), headers . getPathValue (), is_last );

  using  namespace std::literals;

  // is the request even cacheable
  if (
    headers . Host () == nullptr  // missing host
    || headers . Path () == nullptr  // missing resource id
    || headers . getMethodValue () != "GET"sv  // only get requests are cacheable, TODO: uppercase is important, find a more foolproof way
    || ! is_last  // requests with body/trailers aren't cacheable
  )
  {
    m_State = State::NotCacheable;
    return  Http::FilterHeadersStatus::Continue;
  }

  m_Key = Self::derive_key ( headers );

  auto  response = m_Cache -> lookup ( m_Key );  // safe to copy because the values are shared pointers
  if (
    ! response
    || std::chrono::system_clock::now () - (*response) -> m_Stamp > 60s  // pretend entries older than 60s are expired
  )
  {
    m_State = State::Miss;
    return  Http::FilterHeadersStatus::Continue;
  }

  m_State = State::Hit;
  this -> decoder_callbacks_ -> dispatcher () . post ( [ response = (*response), wp = this -> weak_from_this () ] ( ) mutable -> void
  {
    auto  p = wp . lock ();
    if ( ! p )
      return;
    // TODO: maybe use sendLocalReply() instead?
    // not checking headers for null, they really have to be there
    p -> decoder_callbacks_ -> encodeHeaders ( Http::createHeaderMap<Http::ResponseHeaderMapImpl> ( * response -> m_Headers ),
                                               response -> m_Body == nullptr && response -> m_Trailers == nullptr,
                                               "hulahoop" );
    if ( response -> m_Body )
    {
      auto  body = Buffer::OwnedImpl { *response -> m_Body };
      p -> decoder_callbacks_ -> encodeData     ( body, response -> m_Trailers == nullptr );
    }
    if ( response -> m_Trailers )
      p -> decoder_callbacks_ -> encodeTrailers ( Http::createHeaderMap<Http::ResponseTrailerMapImpl> ( * response -> m_Trailers ) );
  } );

  return  Http::FilterHeadersStatus::StopAllIterationAndWatermark;
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers,
                               bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( trace, "encoding: headers = [status={}, ...]; is last = {}", headers . getStatusValue (), is_last );
  switch ( m_State )
  {
    case  State::NotCacheable:
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
        m_State = State::NotCacheable;
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

auto  Filter::encodeData     ( Buffer::Instance & body,
                               bool  is_last ) -> Http::FilterDataStatus
{
  ENVOY_LOG ( trace, "encoding: {} bytes of body; is last = {}", body . length (), is_last );
  switch ( m_State )
  {
    case  State::NotCacheable:
      return  Http::FilterDataStatus::Continue;
      break;
    case  State::Hit:
      return  Http::FilterDataStatus::Continue;
      break;
    case  State::Miss:
      if ( ! m_Body )
        m_Body = std::make_unique<Buffer::OwnedImpl> ();
      m_Body -> add ( body );
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
  ENVOY_LOG ( trace, "encoding: trailers" );
  switch ( m_State )
  {
    case  State::NotCacheable:
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

[[nodiscard]]
auto  Filter::derive_key  ( const Http::RequestHeaderMap & headers ) -> std::string
{
  return  absl::StrCat ( headers . getSchemeValue (), headers . getHostValue (), headers . getPathValue () );
}

auto  Filter::commit  ( ) -> void
{
  m_Cache -> insert_or_assign ( m_Key, [ this ] ( ) { return  std::make_shared<const Response> ( std::move ( m_Headers ), std::move ( m_Trailers ), std::move ( m_Body ), std::move ( m_Stamp ) ); } );
}


REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );


}
