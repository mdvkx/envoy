#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace  Envoy::Extensions::HttpFilters::Rqc {


struct  MsgHeaders
{
  std::unique_ptr<Http::ResponseHeaderMap>  m_Headers;
  bool  m_IsLast;
};

struct  MsgBody
{
  std::unique_ptr<Buffer::Instance>  m_Body;
  bool  m_IsLast;
};

struct  MsgTrailers
{
  std::unique_ptr<Http::ResponseTrailerMap>  m_Trailers;
};

using  Msg = std::variant<MsgHeaders, MsgBody, MsgTrailers>;

struct  Pending
{
  std::vector<std::function<void (Msg &&)> >  m_Waiting;
};

struct  Cache
{
  mutable std::mutex  m_Mtx;
  std::unordered_map<std::string, Pending>  m_Elements;
  auto  insert_or ( const std::string & k,
                    const std::function<Pending ()> & lazy,
                    const std::function<void (Pending &)> & modify ) -> bool
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
  auto  remove ( const std::string & k ) -> std::optional<Pending>
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
