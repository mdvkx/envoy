#pragma once

template <typename  T_, std::size_t  N_>
struct  Ring
{
  static_assert ( std::has_single_bit ( N_ ), "ring capacity must be a non-zero power of two" );

  std::size_t               m_Wr = 0;
  std::size_t               m_Size = 0;
  T_                      * m_Data = reinterpret_cast<T_ *> ( m_Mem );
  alignas ( T_ ) std::byte  m_Mem [ sizeof ( T_ ) * N_ ];

  constexpr  Ring ( ) = default;
  constexpr  ~Ring ( ) = default;
  constexpr auto  size ( ) const -> std::size_t;
  constexpr auto  capacity ( ) const -> std::size_t;
  constexpr auto  operator [] ( ) -> T_ &;
  constexpr auto  operator [] ( ) const -> const T_ &;
};
