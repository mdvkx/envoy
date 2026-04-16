#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>


namespace  Envoy::Extensions::HttpFilters::Rqc
{

struct  Filter;

struct  Ticket
{
  std::vector<std::shared_ptr<Filter> >  m_Waiting;
};

struct  Cache
{
  mutable std::mutex  m_Mtx;
  std::unordered_map<std::string, Ticket>  m_Requests;
  [[nodiscard]]
  auto  size ( ) const -> std::size_t;
  auto  insert ( const std::string & k,
                 const std::function<Ticket ()> & v ) -> bool;
  auto  insert_or ( const std::string & k,
                    const std::function<Ticket ()> & v,
                    const std::function<void (Ticket &)> & f ) -> bool;
  auto  lookup ( const std::string & k,
                 const std::function<void (const Ticket &)> & f ) const -> bool;
  [[nodiscard]]
  auto  contains ( const std::string & k ) const -> bool;
  auto  remove ( const std::string & k ) -> std::optional<Ticket>;
};


[[nodiscard]]
auto  Cache::size ( ) const -> std::size_t
{
  auto  l = std::unique_lock { m_Mtx };
  return  m_Requests . size ();
}

auto  Cache::insert ( const std::string & k,
                      const std::function<Ticket ()> & v ) -> bool
{
  auto  l = std::unique_lock { m_Mtx };
  auto  i = m_Requests . find ( k );
  if ( i != m_Requests . end () )
    return  false;
  m_Requests . emplace_hint ( i, k, v () );
  return  true;
}

auto  Cache::insert_or ( const std::string & k,
                         const std::function<Ticket ()> & v,
                         const std::function<void (Ticket &)> & f ) -> bool
{
  auto  l = std::unique_lock { m_Mtx };
  auto  i = m_Requests . find ( k );
  if ( i != m_Requests . end () )
  {
    f ( i -> second );
    return  false;
  }
  else
  {
    m_Requests . emplace_hint ( i, k, v () );
    return  true;
  }
}

auto  Cache::lookup ( const std::string & k,
                      const std::function<void (const Ticket &)> & f ) const -> bool
{
  auto  l = std::unique_lock { m_Mtx };
  auto  i = m_Requests . find ( k );
  if ( i == m_Requests . end () )
    return  false;
  f ( i -> second );
  return  true;
}

[[nodiscard]]
auto  Cache::contains ( const std::string & k ) const -> bool
{
  auto  l = std::unique_lock { m_Mtx };
  return  m_Requests . contains ( k );
}

auto  Cache::remove ( const std::string & k ) -> std::optional<Ticket>
{
  auto  l = std::unique_lock { m_Mtx };
  auto  i = m_Requests . find ( k );
  if ( i == m_Requests . end () )
    return  std::nullopt;
  auto  x = std::move ( i -> second );
  m_Requests . erase ( i );
  return  x;
}

}
