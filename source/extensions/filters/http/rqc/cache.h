#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace  Envoy::Extensions::HttpFilters::Rqc {


struct  Cache
{
  mutable std::mutex  m_Mtx;
  std::unordered_map<std::string, std::size_t>  m_Elements;
  auto  insert_or ( const std::string & k,
                    const std::function<std::size_t ()> & lazy,
                    const std::function<void (std::size_t &)> & modify ) -> bool
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Elements . find ( k );
    if ( i == m_Elements . end () )
    {
      m_Elements . emplace_hint ( i, k, lazy () );
      return  true;
    }
    else
    {
      modify ( i -> second );
      return  false;
    }
  }
  auto  remove ( const std::string & k ) -> std::optional<std::size_t>
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Elements . find ( k );
    if ( i == m_Elements . end () )
      return  std::nullopt;
    auto  x = std::move ( i -> second );
    m_Elements . erase ( i );
    l . unlock ();
    return  x;
  }
};

}
