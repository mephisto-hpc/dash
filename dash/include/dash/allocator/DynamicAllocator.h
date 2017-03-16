#ifndef DASH__ALLOCATOR__DYNAMIC_ALLOCATOR_H__INCLUDED
#define DASH__ALLOCATOR__DYNAMIC_ALLOCATOR_H__INCLUDED

#include <dash/dart/if/dart.h>

#include <dash/Team.h>
#include <dash/Types.h>

#include <dash/internal/Logging.h>

#include <dash/allocator/AllocatorTraits.h>
#include <dash/allocator/LocalSpaceAllocator.h>
#include <dash/allocator/internal/Types.h>
#include <dash/memory/MemorySpace.h>
#include <dash/GlobPtr.h>

#include <algorithm>
#include <utility>
#include <vector>
#include <vector>

namespace dash {
namespace allocator {

/**
 * Encapsulates a memory allocation and deallocation strategy of global
 * memory regions distributed across local memory of units in a specified
 * team.
 *
 * Satisfied STL concepts:
 *
 * - Allocator
 * - CopyAssignable
 *
 * \concept{DashAllocatorConcept}
 */
template <
    typename ElementType,
    typename MSpaceCategory = dash::memory_space_host_tag,
    typename LocalAllocator = LocalSpaceAllocator<ElementType, MSpaceCategory>
>
class DynamicAllocator {
  template <class T, class U>
  friend bool operator==(const DynamicAllocator<T> &lhs,
                         const DynamicAllocator<U> &rhs);
  template <class T, class U>
  friend bool operator!=(const DynamicAllocator<T> &lhs,
                         const DynamicAllocator<U> &rhs);


  /// Type definitions required for std::allocator concept:
 public:
  // clang-format off
  using AllocatorTraits                  = std::allocator_traits<LocalAllocator>;
  using allocator_type                  =  typename AllocatorTraits::allocator_type;
//  using propagate_on_container_move_assignment  = std::true_type;
  using allocator_category                      = dash::noncollective_allocator_tag;
  // clang-format on

 public:
  typedef typename AllocatorTraits::value_type value_type;
  typedef dash::gptrdiff_t difference_type;
  typedef dash::default_size_t size_type;
  typedef dash::GlobPtr<value_type> pointer;
  typedef dash::GlobPtr<void> void_pointer;
  typedef dash::GlobPtr<const value_type> const_pointer;
  typedef dash::GlobPtr<const void> const_void_pointer;


  typedef typename AllocatorTraits::pointer local_pointer;
  typedef typename AllocatorTraits::const_pointer const_local_pointer;
  // clang-format on
  //
 private:
  using self_t = DynamicAllocator<ElementType, MSpaceCategory, LocalAllocator>;
  using block_t = dash::allocator::memory_block;
  using internal_value_type = std::pair<block_t, pointer>;
  using memory_space = dash::MemorySpace<MSpaceCategory>;

 public:
  /// Convert DynamicAllocator<T> to DynamicAllocator<U>.
  /*
  template <typename U>
  struct rebind {
    typedef DynamicAllocator<U, MSpaceCategory, LocalAllocator> other;
  };
  */

 public:
  /**
   * Constructor.
   * Creates a new instance of \c dash::DynamicAllocator for a given team.
   */
  DynamicAllocator(memory_space * space, Team &team = dash::Team::All()) noexcept
    : _team(&team)
    , _nunits(team.size())
    , _alloc(space)
  {
  }

  /**
   * Move-constructor.
   * Takes ownership of the moved instance's allocation.
   */
  DynamicAllocator(self_t &&other) noexcept
    : _team(nullptr)
  {
    std::swap(_allocated, other._allocated);
    std::swap(_team, other._team);
    std::swap(_alloc, other._alloc);
  }

  /**
   * Default constructor, deleted.
   */
  DynamicAllocator() noexcept = delete;

  /**
   * Copy constructor.
   *
   * This copy constructor does not copy _allocated since memory is strictly
   * coupled to a specific team.
   *
   * \see DashAllocatorConcept
   */
  DynamicAllocator(const self_t &other) noexcept
    : _team(other._team)
    , _nunits(other._nunits)
    , _alloc(other._alloc)
  {
  }

  /**
   * Copy-constructor.
   * Does not take ownership of the copied instance's allocation.
   */
  template <class U>
  DynamicAllocator(const DynamicAllocator<U> &other) noexcept
    : _team(other._team)
    , _nunits(other._nunits)
    , _alloc(other._alloc)
  {
  }

  /**
   * Destructor.
   * Frees all global memory regions allocated by this allocator instance.
   */
  ~DynamicAllocator() noexcept { clear(); }
  /**
   * Assignment operator.
   *
   * \todo[TF] This smells like bad a surprise at some point...
   *
   * \see DashAllocatorConcept
   */
  self_t &operator=(const self_t &other) noexcept
  {
    // noop
    return *this;
  }

  /**
   * Move-assignment operator.
   */
  self_t &operator=(const self_t &&other) noexcept
  {
    DASH_LOG_DEBUG("DynamicAllocator.=(&&)()");
    if (this != &other) {
      // Take ownership of other instance's allocation vector:
      std::swap(_allocated, other._allocated);
    }
    DASH_LOG_DEBUG("DynamicAllocator.=(&&) >");
    return *this;
  }

  /**
   * Estimate the largest supported size
   */
  size_type max_size() const noexcept
  {
    return size_type(-1) / sizeof(ElementType);
  }

  /**
   * Whether storage allocated by this allocator can be deallocated
   * through the given allocator instance.
   * Establishes reflexive, symmetric, and transitive relationship.
   * Does not throw exceptions.
   *
   * \returns  true if the storage allocated by this allocator can be
   *           deallocated through the given allocator instance.
   *
   * \see DashAllocatorConcept
   */
  bool operator==(const self_t &rhs) const noexcept
  {
    return (_team->dart_id() == rhs._team->dart_id());
  }

  /**
   * Whether storage allocated by this allocator cannot be deallocated
   * through the given allocator instance.
   * Establishes reflexive, symmetric, and transitive relationship.
   * Does not throw exceptions.
   * Same as \c !(operator==(rhs)).
   *
   * \returns  true if the storage allocated by this allocator cannot be
   *           deallocated through the given allocator instance.
   *
   * \see DashAllocatorConcept
   */
  bool operator!=(const self_t &rhs) const noexcept { return !(*this == rhs); }
  /**
   * Team containing units associated with the allocator's memory space.
   */
  inline dash::Team &team() const noexcept
  {
    if (_team == nullptr) {
      return dash::Team::Null();
    }
    return *_team;
  }

  /**
   * Register pre-allocated local memory segment of \c num_local_elem
   * elements in global memory space.
   *
   * Collective operation.
   * The number of allocated elements may differ between units.
   *
   * \see DashDynamicAllocatorConcept
   */
  pointer attach(local_pointer lptr, size_type num_local_elem)
  {
    DASH_LOG_DEBUG("DynamicAllocator.attach(nlocal)", "number of local values:",
                   num_local_elem, "pointer: ", lptr);

    block_t pseudoBlock(lptr, num_local_elem);

    // Search for corresponding memory block
    auto const end = std::end(_allocated);
    auto const found =
        std::find_if(std::begin(_allocated), end,
                     [&pseudoBlock](internal_value_type const &val) {
                       return val.first == pseudoBlock;
                     });

    if (found == end) {
      // memory block not found
      DASH_THROW(dash::exception::InvalidArgument, "attach invalid pointer");
    }
    else if (found->second) {
      // memory block is already attached
      DASH_LOG_ERROR("local memory alread attach to memory", found->second);

      DASH_THROW(dash::exception::InvalidArgument,
                 "cannot repeatedly attach local pointer");
    }

    // Attach the block
    dart_storage_t ds = dart_storage<value_type>(num_local_elem);
    dart_gptr_t dgptr;
    if (dart_team_memregister(_team->dart_id(), ds.nelem, ds.dtype,
                              found->first.ptr, &dgptr) != DART_OK) {
      // reset to DART_GPTR_NULL
      found->second = pointer(DART_GPTR_NULL);
      DASH_LOG_ERROR("DynamicAllocator.attach", "cannot attach local memory",
                     found->first.ptr);
    } else {
      found->second = pointer(dgptr);
    }
    DASH_LOG_DEBUG("DynamicAllocator.attach > ", found->second);
    return found->second;
  }

  /**
   * Unregister local memory segment from global memory space.
   * Does not deallocate local memory.
   *
   * Collective operation.
   *
   * \see DashDynamicAllocatorConcept
   */
  void detach(pointer gptr)
  {
    DASH_LOG_DEBUG("DynamicAllocator.detach()", "gptr:", gptr);
    if (!dash::is_initialized()) {
      // If a DASH container is deleted after dash::finalize(), global
      // memory has already been freed by dart_exit() and must not be
      // deallocated again.
      DASH_LOG_DEBUG("DynamicAllocator.detach >",
                     "DASH not initialized, abort");
      return;
    }

    auto const end = std::end(_allocated);
    // Look up if we can
    auto const found = std::find_if(
        std::begin(_allocated), end,
        [&gptr](internal_value_type const &val) { return val.second == gptr; });

    if (found == end) {
      DASH_LOG_DEBUG("DynamicAllocator.detach >",
                     "cannot detach untracked pointer");
      return;
    }

    if (dart_team_memderegister(gptr.dart_gptr()) != DART_OK) {
      DASH_LOG_ERROR("DynamicAllocator.detach >",
                     "cannot detach global pointer", gptr);
      DASH_ASSERT(false);
    }

    found->second = DART_GPTR_NULL;

    DASH_LOG_DEBUG("DynamicAllocator.detach >");
  }

  /**
   * Allocates \c num_local_elem local elements in the active unit's local
   * memory.
   *
   * Local operation.
   *
   * \see DashDynamicAllocatorConcept
   */
  local_pointer allocate_local(size_type num_local_elem)
  {

    local_pointer lp = AllocatorTraits::allocate(_alloc, num_local_elem);

    if (!lp) return nullptr;

    block_t b{lp, num_local_elem};

    _allocated.push_back(std::make_pair(b, pointer(DART_GPTR_NULL)));

    DASH_LOG_TRACE("DynamicAllocator.allocate_local", "allocated local pointer",
                   mem.ptr);

    return lp;
  }

  /**
   * Deallocates memory segment in the active unit's local memory.
   *
   * Local operation.
   *
   * \see DashDynamicAllocatorConcept
   */
  void deallocate_local(local_pointer lptr, size_type num_local_elem)
  {
    block_t pseudoBlock(lptr, num_local_elem);

    auto const end = std::end(_allocated);
    auto const found =
        std::find_if(std::begin(_allocated), end,
                     [&pseudoBlock](internal_value_type const &val) {
                       return val.first == pseudoBlock;
                     });

    if (found == end) return;

    bool const attached = found->second;
    if (attached) {
      // TODO rko: detach memory from window...
      DASH_LOG_ERROR("DynamicAllocator.deallocate_local",
                     "deallocating local pointer which is still attached",
                     found->second);
    }

    // TODO rko: first call the destructor??
    AllocatorTraits::deallocate(_alloc, found->first.ptr, num_local_elem);

    if (!attached) _allocated.erase(found);
  }

  /**
   * Allocates \c num_local_elem local elements at active unit and attaches
   * the local memory segment in global memory space.
   *
   * Collective operation.
   * The number of allocated elements may differ between units.
   *
   * \see DashAllocatorConcept
   */
  pointer allocate(size_type num_local_elem)
  {
    local_pointer lp = allocate_local(num_local_elem);
    pointer gp = attach(lp, num_local_elem);
    if (!gp) {
      // Attach failed, free requested local memory:
      deallocate_local(lp);
    }
    return gp;
  }

  /**
   * Detaches memory segment from global memory space and deallocates the
   * associated local memory region.
   *
   * Collective operation.
   *
   * \see DashAllocatorConcept
   */
  void deallocate(pointer gptr, size_type num_local_elem)
  {
    DASH_LOG_DEBUG("DynamicAllocator.deallocate()", "gptr:", gptr);
    if (!dash::is_initialized()) {
      // If a DASH container is deleted after dash::finalize(), global
      // memory has already been freed by dart_exit() and must not be
      // deallocated again.
      DASH_LOG_DEBUG("DynamicAllocator.deallocate >",
                     "DASH not initialized, abort");
      return;
    }
    DASH_LOG_DEBUG("DynamicAllocator.deallocate", "deallocate local memory");
    auto const end = std::end(_allocated);
    auto const found = std::find_if(
        std::begin(_allocated), end,
        [&gptr](internal_value_type &val) { return val.second == gptr; });
    if (found != end) {
      // Unregister from global memory space, removes gptr from _allocated:
      detach(found->second);
      // Free local memory:
      AllocatorTraits::deallocate(_alloc, found->first.ptr, num_local_elem);

      //erase from locally tracked blocks
      _allocated.erase(found);
    }
    else {
      DASH_LOG_ERROR("DynamicAllocator.deallocate", "cannot deallocate gptr",
                     gptr);
    }

    DASH_LOG_DEBUG("DynamicAllocator.deallocate >");
  }

  allocator_type allocator() {
    return _alloc;
  }

 private:
  /**
   * Frees and detaches all global memory regions allocated by this allocator
   * instance.
   */
  void clear() noexcept
  {
    DASH_LOG_DEBUG("DynamicAllocator.clear()");
    /*
    for (auto & e : _allocated) {
      // Null-buckets have lptr set to nullptr
      if (e.first != nullptr) {
        DASH_LOG_DEBUG("DynamicAllocator.clear", "deallocate local memory:",
                       e.first);
        delete[] e.first;
        e.first = nullptr;
      }
      if (!DART_GPTR_ISNULL(e.second)) {
        DASH_LOG_DEBUG("DynamicAllocator.clear", "detach global memory:",
                       e.second);
        // Cannot use DASH_ASSERT due to noexcept qualifier:
        dart_ret_t ret = dart_team_memderegister(e.second);
        assert(ret == DART_OK);
      }
    }
    */

    auto &alloc_capture = _alloc;
    auto const teamId = _team->dart_id();
    std::for_each(
        std::begin(_allocated), std::end(_allocated),
        [&alloc_capture, &teamId](internal_value_type & block) {
          // Deallocate local memory
          DASH_LOG_DEBUG("DynamicAllocator.clear",
                         "deallocate local memory block:", block.first.ptr);
          AllocatorTraits::deallocate(alloc_capture, static_cast<local_pointer>(block.first.ptr), block.first.length);
          // Deregister global memory
          if (block.second.dart_gptr()) {
            DASH_LOG_DEBUG("DynamicAllocator.clear", "detach global memory:",
                           block.second);
            // Cannot use DASH_ASSERT due to noexcept qualifier:
            DASH_ASSERT_RETURNS(dart_team_memderegister(block.second.dart_gptr()), DART_OK);
          }
        });
    _allocated.clear();
    DASH_LOG_DEBUG("DynamicAllocator.clear >");
  }

 private:
  dash::Team *_team;
  size_t _nunits = 0;
  std::vector<internal_value_type> _allocated;
  LocalAllocator _alloc;

};  // class DynamicAllocator

template <class T, class U>
bool operator==(const DynamicAllocator<T> &lhs, const DynamicAllocator<U> &rhs)
{
  return (sizeof(T) == sizeof(U) &&
          lhs._team->dart_id() == rhs._team->dart_id() &&
          lhs._nunits == rhs._nunits && lhs._alloc == rhs._alloc);
}

template <class T, class U>
bool operator!=(const DynamicAllocator<T> &lhs, const DynamicAllocator<U> &rhs)
{
  return !(lhs == rhs);
}

}  // namespace allocator
}  // namespace dash

#endif  // DASH__ALLOCATOR__DYNAMIC_ALLOCATOR_H__INCLUDED
