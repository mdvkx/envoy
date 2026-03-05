#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>

// --------------------------------------------------------------------------

using  usize = size_t;

//  std::aligned_storage is deprecated, this is a replacement with improved semantics and a richer api
template
<
  typename   T_,
  usize      Capacity_
>
struct  Storage
  final
{
  using  Self = Storage;

  static_assert ( Capacity_ != 0 );

  alignas ( T_ ) std::byte  m_Data [ sizeof ( T_ ) * Capacity_ ];
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  at ( usize  idx ) const
    -> const T_ &
  {
    if ( idx >= this -> capacity () )
      throw  std::invalid_argument { "index out of bounds" };
    return  this -> data () [ idx ];
  }
  // ------------------------------------------------------------------------
  [[nodiscard]] auto  at ( usize  idx ) -> T_ &                       { return  const_cast<T_ &> ( static_cast<const Self &> ( (*this) ) . at ( idx ) ); }
  // ------------------------------------------------------------------------
  [[nodiscard]] auto  operator [] ( usize  idx ) const -> const T_ &  { return  this -> at ( idx ); }
  [[nodiscard]] auto  front ( ) const -> const T_ &                   { return  this -> at ( 0 ); }
  [[nodiscard]] auto  back ( ) const -> const T_ &                    { return  this -> at ( this -> capacity ()-1 ); }
  // ------------------------------------------------------------------------
  [[nodiscard]] auto  operator [] ( usize  idx ) -> T_ &              { return  this -> at ( idx ); }
  [[nodiscard]] auto  front ( ) -> T_ &                               { return  this -> at ( 0 ); }
  [[nodiscard]] auto  back ( ) -> T_ &                                { return  this -> at ( this -> capacity ()-1 ); }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  constexpr auto  capacity ( ) const
    -> usize
  {
    return  Capacity_;
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  data ( ) const
    -> const T_ *
  {
    // https://en.cppreference.com/w/cpp/utility/launder.html
    return  std::launder ( reinterpret_cast<const T_ *> ( m_Data ) );
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  data ( )
    -> T_ *
  {
    return  const_cast<T_ *> ( static_cast<const Self &> ( (*this) ) . data () );
  }
};


template
<
  typename   T_,
  usize      Capacity_ = 8
>
struct  RingBuffer
  final
{
  using  Self  = RingBuffer;

  static_assert ( std::has_single_bit ( Capacity_ ), "capacity must be a power of two" );  // neatly combine != and is power of two

  usize  m_Wr = 0;
  usize  m_Size = 0;
  Storage<T_, Capacity_>  m_Storage;
  // ------------------------------------------------------------------------
  [[nodiscard]]
  constexpr auto  capacity ( ) const
    -> usize
  {
    return  m_Storage . capacity ();
  }
  // ------------------------------------------------------------------------
  template
  <
    typename ...  Args_
  >
  auto  push ( Args_ && ... args )
    -> void
  {
    static_assert ( std::constructible_from<T_, Args_ ...> );
    const auto  wr = m_Wr ++;
    if ( this -> is_full () )
      std::destroy_at ( &m_Storage . at ( wr ) );
    std::construct_at ( &m_Storage . at ( wr ), std::forward<Args_> ( args ) ... );
    m_Wr &= this -> capacity ()-1;
    if ( !this -> is_full () )
      m_Size ++;
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  lookup ( const std::function<bool (const T_ &)>  & predicate ) const
    -> const T_ *
  {
    const auto  r = this -> to_span ();
    const auto  i = std::ranges::find_if ( r, predicate );
    return  ( i == r . end () ) ? nullptr : &(*i);
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  contains ( const std::function<bool (const T_ &)>  & predicate ) const
    -> bool
  {
    return  this -> lookup ( predicate ) != nullptr;
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  contains ( const T_ & t ) const
    -> bool
  {
    return  this -> contains ( [ & ] ( const T_  & x ) -> bool { return  x == t; } );
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  size ( ) const
    -> usize
  {
    return  m_Size;
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  is_full ( ) const
    -> bool
  {
    return  this -> size () >= this -> capacity ();
  }
  // ------------------------------------------------------------------------
  [[nodiscard]]
  auto  to_span ( ) const
    -> std::span<const T_>
  {
    return  std::span { m_Storage . data (), this -> size () };
  }
};
