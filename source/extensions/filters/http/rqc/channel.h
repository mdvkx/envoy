#pragma once

template <typename  T_>
struct  Channel
{
  std::vector<std::function<void (const T_ &)> >  m_Waiting;
  auto  publish ( const T_ & x ) -> void
  {
    for ( auto & w : m_Waiting )
      w ( x );
  }
  auto  subscribe ( std::invocable<const T_ &> auto && w ) -> void
  {
    m_Waiting . emplace_back ( std::forward<decltype ( w )> ( w ) );
  }
};
