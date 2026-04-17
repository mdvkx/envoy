#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

template <typename  Key_, typename  Value_>
struct  ConcurrentHashMap
{
  using  Key = Key_;
  using  Value = Value_;

  mutable std::mutex  m_Mtx;
  std::unordered_map<Key, Value>  m_Entries;

  [[nodiscard]]
  auto  size ( ) const -> std::size_t
  {
    auto  l = std::unique_lock { m_Mtx };
    return  m_Entries . size ();
  }

  auto  lookup ( const Key & k,
                 const std::function<void (const Value &)> & f ) const -> bool
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Entries . find ( k );
    if ( i == m_Entries . end () )
      return  false;
    f ( i -> second );
    return  true;
  }

  [[nodiscard]]
  auto  contains ( const Key & k ) const -> bool
  {
    auto  l = std::unique_lock { m_Mtx };
    return  m_Entries . contains ( k );
  }

  auto  insert ( const Key & k,
                 const std::function<Value ()> & v ) -> bool
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Entries . find ( k );
    if ( i != m_Entries . end () )
      return  false;
    m_Entries . emplace_hint ( i, k, v () );
    return  true;
  }

  auto  insert_or ( const Key & k,
                    const std::function<Value ()> & v,
                    const std::function<void (Value &)> & f ) -> bool
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Entries . find ( k );
    if ( i != m_Entries . end () )
    {
      f ( i -> second );
      return  false;
    }
    else
    {
      m_Entries . emplace_hint ( i, k, v () );
      return  true;
    }
  }

  auto  remove ( const Key & k ) -> std::optional<Value>
  {
    auto  l = std::unique_lock { m_Mtx };
    auto  i = m_Entries . find ( k );
    if ( i == m_Entries . end () )
      return  std::nullopt;
    auto  x = std::move ( i -> second );
    m_Entries . erase ( i );
    return  x;
  }
};
