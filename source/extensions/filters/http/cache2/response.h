#pragma once

#include "envoy/common/time.h"  // SystemTime
#include "envoy/http/header_map.h"  // ResponseHeaderMap, ResponseTrailerMap

#include <memory>
#include <string>

namespace  Envoy::Extensions::HttpFilters::Cache2 {


struct  Response
{
  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers = nullptr;
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers = nullptr;
  std::string  m_Body = "";
  Envoy::SystemTime  m_Stamp;
};

}
