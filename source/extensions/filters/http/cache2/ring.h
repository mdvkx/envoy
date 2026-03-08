#pragma once

#include <bit>  // std::has_single_bit
#include <cstddef>  // std::{byte, size_t}
#include <memory>  // std::{addressof, construct_at, destroy_at}
#include <optional>  // std::{nullopt, optional}
#include <stdexcept>  // std::invalid_argument
#include <utility>  // std::move

template <typename  T_, std::size_t  N_>
struct  Ring
{
  using  Self = Ring<T_, N_>;

  static_assert ( std::has_single_bit ( N_ ), "ring's capacity must be a non-zero power of two" );

  std::size_t               m_Rd = 0;
  std::size_t               m_Wr = 0;
  std::size_t               m_Size = 0;
  T_                      * m_Data = reinterpret_cast<T_ *> ( m_Mem );
  alignas ( T_ ) std::byte  m_Mem [ sizeof ( T_ ) * N_ ];

  [[nodiscard]]
  static constexpr auto  wrap ( std::size_t  x ) -> std::size_t
  {
    return  x & (N_ - 1);  // aka x % N_ when N_ is a power of two
  }
  static constexpr auto  inc ( std::size_t & x ) -> std::size_t
  {
    auto  p = x ++;
    x = Self::wrap ( x );
    return  p;
  }

  constexpr  ~Ring ( ) noexcept
  {
    while ( this -> size () > 0 )
      this -> pop ();  // i'm lazy
  }
  [[nodiscard]]
  constexpr auto  size ( ) const -> std::size_t
  {
    return  m_Size;
  }
  [[nodiscard]]
  constexpr auto  capacity ( ) const -> std::size_t
  {
    return  N_;
  }
  [[nodiscard]]
  constexpr auto  operator [] ( std::size_t  idx ) -> T_ &
  {
    if ( idx >= this -> size () )
      throw  std::invalid_argument { "index out of bounds" };
    return  m_Data [ Self::wrap ( m_Rd + idx ) ];
  }
  [[nodiscard]]
  constexpr auto  operator [] ( std::size_t  idx ) const -> const T_ &
  {
    if ( idx >= this -> size () )
      throw  std::invalid_argument { "index out of bounds" };
    return  m_Data [ Self::wrap ( m_Rd + idx ) ];
  }
  constexpr auto  push ( T_ t ) -> std::optional<T_>
  {
    auto  r = this -> size () >= this -> capacity () ? this -> pop () : std::nullopt;
    std::construct_at ( std::addressof ( m_Data [ m_Wr ] ), std::move ( t ) );
    Self::inc ( m_Wr );
    m_Size ++;
    return  r;
  }
  constexpr auto  pop ( ) -> std::optional<T_>
  {
    if ( this -> size () == 0 )
      return  std::nullopt;
    auto * px = std::addressof ( m_Data [ m_Rd ] );
    auto  x = std::move ( *px );
    std::destroy_at ( px );
    Self::inc ( m_Rd );
    m_Size --;
    return  x;
  }

};
