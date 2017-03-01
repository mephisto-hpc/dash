#ifndef DASH__VIEW__LOCAL_H__INCLUDED
#define DASH__VIEW__LOCAL_H__INCLUDED

#include <dash/Types.h>
#include <dash/Range.h>

#include <dash/view/ViewTraits.h>


namespace dash {

/**
 * \concept{DashViewConcept}
 */
template <
  class    ViewType,
  typename ViewValueT = typename std::decay<ViewType>::type >
constexpr auto
local(ViewType & v)
-> typename std::enable_if<
     std::is_pointer< typename ViewType::iterator >::value,
     ViewType &
   >::type {
  return v;
}

/**
 * \concept{DashViewConcept}
 */
template <class ViewType>
constexpr auto
local(const ViewType & v)
-> typename std::enable_if<
     dash::view_traits<ViewType>::is_view::value,
//   decltype(v.local())
     const typename ViewType::local_type
   >::type {
  return v.local();
}

template <
  class    ViewType,
  typename ViewValueT = typename std::decay<ViewType>::type >
constexpr auto
local(ViewType && v)
-> typename std::enable_if<
     dash::view_traits<ViewValueT>::is_view::value,
     decltype(std::forward<ViewType>(v).local())
   >::type {
 return std::forward<ViewType>(v).local();
}

/**
 * \concept{DashViewConcept}
 */
template <class ContainerType>
constexpr
typename std::enable_if<
  !dash::view_traits<ContainerType>::is_view::value,
  const typename ContainerType::local_type &
>::type
local(const ContainerType & c) {
  return c.local;
}

/**
 * Convert global iterator referencing an element the active unit's
 * memory to a corresponding native pointer referencing the element.
 *
 * Precondition:  \c g_it  is local
 *
 */
template <class GlobalIterator>
constexpr auto local(
  /// Global iterator referencing element in local memory
  const GlobalIterator & g_it)
->  decltype((g_it - g_it.pos()).local()) {
  return g_it.local();
}

// =========================================================================
// Multidimensional Views
// =========================================================================

} // namespace dash

#endif // DASH__VIEW__LOCAL_H__INCLUDED
