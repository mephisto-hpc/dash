#ifndef DASH__VIEW__DOMAIN_H__INCLUDED
#define DASH__VIEW__DOMAIN_H__INCLUDED

#include <dash/Types.h>
#include <dash/Range.h>

#include <dash/view/ViewTraits.h>


namespace dash {

// ------------------------------------------------------------------------
// dash::domain(View)

/**
 *
 * \concept{DashViewConcept}
 */
template <
  class    ViewT,
  typename ViewValueT =
             typename std::remove_const<
               typename std::remove_reference<ViewT>::type
             >::type
>
constexpr auto
domain(ViewT && view)
  -> typename std::enable_if<
       dash::view_traits<ViewValueT>::is_view::value,
    // const typename dash::view_traits<ViewT>::domain_type &
    // decltype(view.domain())
       decltype(std::forward<ViewT>(view).domain())
     >::type {
//return view.domain();
  return std::forward<ViewT>(view).domain();
}
#if 0
template <
  class    ViewT,
  typename ViewValueT =
             typename std::remove_const<
               typename std::remove_reference<ViewT>::type
             >::type
>
constexpr auto
domain(ViewT && view)
  -> typename std::enable_if<
       dash::view_traits<ViewValueT>::is_view::value,
       const typename dash::view_traits<ViewValueT>::domain_type &
    // decltype(view.domain())
     >::type {
  return view.domain();
}
#endif

// ------------------------------------------------------------------------
// dash::domain(Container)

/**
 *
 * \concept{DashViewConcept}
 */
template <
  class    ContainerT,
  typename ContainerValueT =
             typename std::remove_const<
               typename std::remove_reference<ContainerT>::type
             >::type
>
constexpr typename std::enable_if<
  !dash::view_traits<ContainerValueT>::is_view::value,
  ContainerT &
>::type
domain(ContainerT & container) {
  return container;
}

} // namespace dash

#endif // DASH__VIEW__DOMAIN_H__INCLUDED
