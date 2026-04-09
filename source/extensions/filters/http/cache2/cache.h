#pragma once

#include "./response.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace  Envoy::Extensions::HttpFilters::Cache2 {


struct  Cache
{
  mutable std::mutex  m_Mtx;
  std::unordered_map<std::string, std::shared_ptr<const Response> >  m_Responses;
  [[nodiscard]]
  auto  lookup ( const std::string & k ) const -> std::optional<std::shared_ptr<const Response> >
  {
    auto  l = std::unique_lock { m_Mtx };
    if ( const auto  i = m_Responses . find ( k ); i == m_Responses . end () )
      return  std::nullopt;
    else
      return  i -> second;
  }
  auto  insert ( const std::string & k, std::shared_ptr<const Response>  v ) -> void
  {
    auto  l = std::unique_lock { m_Mtx };
    m_Responses . insert_or_assign ( k, std::move ( v ) );
  }
};

}
