
#include "./filter.h"

#include "source/common/http/header_map_impl.h"  // createHeaderMap

#include <cassert>
#include <type_traits>

namespace  Envoy::Extensions::HttpFilters::Rqc
{

auto  Filter::derive_key ( const Http::RequestHeaderMap & headers ) const -> std::string
{
  return  absl::StrCat ( headers . getSchemeValue (), headers . getHostValue (), headers . getPathValue () );
}

// Http::StreamFilterBase
auto  Filter::onDestroy ( ) -> void override
{
  if ( ! m_X )
    return;
  auto  l = std::unique_lock { m_Cache -> m_Mtx };
  m_Cache -> m_Requests . erase ( m_Key );
}

// Http::StreamDecoderFilter
auto  Filter::decodeHeaders ( Http::RequestHeaderMap & headers,
                              bool  is_last ) -> Http::FilterHeadersStatus override
{
  m_Key = this -> derive_key ( headers );
  auto  l = std::unique_lock { m_Cache -> m_Mtx };
  auto  i = m_Cache -> m_Requests . find ( m_Key );
  if ( i == m_Cache -> m_Requests . end () )
  {
    m_Cache -> m_Requests . emplace_hint ( i, m_Key, Ticket {} );
    m_X = true;
    return  Http::FilterHeadersStatus::Continue;
  }
  else
    return  Http::FilterHeadersStatus::StopIteration;
}

// Http::StreamEncoderFilter
auto  Filter::encodeHeaders ( Http::ResponseHeaderMap & headers,
                              bool  is_last ) -> Http::FilterHeadersStatus override
{
  return  Http::FilterHeadersStatus::Continue;
}

auto  Filter::encodeTrailers ( Http::ResponseTrailerMap & trailers ) -> Http::FilterTrailersStatus override
{
  return  Http::FilterTrailersStatus::Continue;
}

auto  Filter::encodeData    ( Buffer::Instance & body,
                              bool  is_last ) -> Http::FilterDataStatus override
{
  return  Http::FilterDataStatus::Continue;
}

}

namespace  Envoy::Extensions::HttpFilters::Rqc
{

REGISTER_FACTORY ( Factory, Server::Configuration::NamedHttpFilterConfigFactory );

}
