
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
}

auto  Filter::onStreamComplete ( ) -> void
{
}

auto  Filter::decodeHeaders  ( Http::RequestHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
  ENVOY_LOG ( debug, "" );

  using  namespace std::literals;

  // is the request even cacheable
  if (
    headers . Host () == nullptr  // missing host
    || headers . Path () == nullptr  // missing resource id
    || headers . getMethodValue () != "GET"sv  // only get requests are cacheable
    || ! is_last  // requests with body/trailers aren't cacheable
  )
  {
    m_State = State::NotCacheable;
    assert ( 0 );
  }

  auto  response = this -> lookup ( headers );

  if (
    ! response
    //|| std::chrono::duration_cast<std::chrono::seconds> ( std::chrono::system_clock::now () - response . m_Stamp ) > 60s
  )
  {
    m_State = State::Miss;
    assert ( 0 );
  }
  else
  {
    assert ( 0 );
  }
}

auto  Filter::encodeHeaders  ( Http::ResponseHeaderMap & headers, bool  is_last ) -> Http::FilterHeadersStatus
{
  switch ( m_State )
  {
    case  State::Unknown:
      assert ( 0 );
      break;
    case  State::NotCacheable:
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

auto  Filter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus
{
  switch ( m_State )
  {
    case  State::Unknown:
      assert ( 0 );
      break;
    case  State::NotCacheable:
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

auto  Filter::encodeData     ( Buffer::Instance & data, bool  is_last ) -> Http::FilterDataStatus
{
  switch ( m_State )
  {
    case  State::Unknown:
      assert ( 0 );
      break;
    case  State::NotCacheable:
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

auto  Filter::commit  ( ) -> void
{
  assert ( 0 );
}

auto  Filter::lookup  ( Http::RequestHeaderMap & headers ) const -> std::optional<Response>
{
  assert ( 0 );
}

}
