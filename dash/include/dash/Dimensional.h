#ifndef DASH__DIMENSIONAL_H_
#define DASH__DIMENSIONAL_H_

#include <assert.h>
#include <array>

#include <dash/Types.h>
#include <dash/Distribution.h>
#include <dash/Team.h>
#include <dash/Exception.h>

namespace dash {

/**
 * Base class for dimensional attributes, stores an
 * n-dimensional value with identical type all dimensions.
 *
 * Different from a SizeSpec or cartesian space, a Dimensional
 * does not define metric/scalar extents or a size, but just a 
 * vector of possibly non-scalar attributes.
 *
 * \tparam  ElementType    The type of the contained values
 * \tparam  NumDimensions  The number of dimensions
 *
 * \see SizeSpec
 * \see CartesianIndexSpace
 */
template<typename ElementType, dim_t NumDimensions>
class Dimensional {
/* 
 * Concept Dimensional:
 *   + Dimensional<T,D>::Dimensional(values[D]);
 *   + Dimensional<T,D>::dim(d | 0 < d < D);
 *   + Dimensional<T,D>::[d](d | 0 < d < D);
 */
private:
  typedef Dimensional<ElementType, NumDimensions> self_t;

protected:
  std::array<ElementType, NumDimensions> _values;

public:
  template<typename E_, dim_t ND_>
  friend std::ostream& operator<<(
    std::ostream & os,
    const Dimensional<E_, ND_> & dimensional);

public:
  /**
   * Constructor, expects one value for every dimension.
   */
  template<typename ... Values>
  Dimensional(ElementType & value, Values ... values)
  : _values {{ value, (ElementType)values... }} {
    static_assert(
      sizeof...(values) == NumDimensions-1,
      "Invalid number of arguments");
  }

  /**
   * Constructor, expects array containing values for every dimension.
   */
  Dimensional(const std::array<ElementType, NumDimensions> & values)
  : _values(values) {
  }

  /**
   * Copy-constructor.
   */
  Dimensional(const self_t & other) {
    for (unsigned int d = 0; d < NumDimensions; ++d) {
      _values[d] = other._values[d];
    }
  }

  /**
   * Return value with all dimensions as array of \c NumDimensions
   * elements.
   */
  const std::array<ElementType, NumDimensions> & values() const {
    return _values;
  }

  /**
   * The value in the given dimension.
   *
   * \param  dimension  The dimension
   * \returns  The value in the given dimension
   */
  ElementType dim(dim_t dimension) const {
    if (dimension >= NumDimensions) {
      DASH_THROW(
        dash::exception::OutOfRange,
        "Dimension for Dimensional::extent() must be lower than " <<
        NumDimensions);
    }
    return _values[dimension];
  }

  /**
   * Subscript operator, access to value in dimension given by index.
   * Alias for \c dim.
   *
   * \param  dimension  The dimension
   * \returns  The value in the given dimension
   */
  ElementType operator[](size_t dimension) const {
    return _values[dimension];
  }

  /**
   * Subscript assignment operator, access to value in dimension given by 
   * index.
   * Alias for \c dim.
   *
   * \param  dimension  The dimension
   * \returns  A reference to the value in the given dimension
   */
  ElementType & operator[](size_t dimension) {
    return _values[dimension];
  }

  /**
   * Equality comparison operator.
   */
  constexpr bool operator==(const self_t & other) const {
    for (dim_t d = 0; d < NumDimensions; ++d) {
      if (dim(d) != other.dim(d)) return false;
    }
    return true;
  }

  /**
   * Equality comparison operator.
   */
  constexpr bool operator!=(const self_t & other) const {
    return !(*this == other);
  }
 
  /**
   * The number of dimensions of the value.
   */
  dim_t rank() const {
    return NumDimensions;
  }
 
  /**
   * The number of dimensions of the value.
   */
  dim_t ndim() const {
    return NumDimensions;
  }

protected:
  /// Prevent default-construction for non-derived types, as initial values
  /// for \c _values have unknown defaults.
  Dimensional() {
  }
};

/**
 * DistributionSpec describes distribution patterns of all dimensions,
 * \see dash::Distribution.
 */
template<dim_t NumDimensions>
class DistributionSpec
: public Dimensional<Distribution, NumDimensions> {
public:
  /**
   * Default constructor, initializes default blocked distribution 
   * (BLOCKED, NONE*).
   */
/* 
  TODO:
  Set defaults depending on pattern traits (e.g. tiled / blocked).
  Sketch of implementation:

  DistributionSpec(
    const PatternTraits & traits)
  : _traits(traits) {
    this->_values = _traits.default_distribution(NumDimensions);
  }
*/
  DistributionSpec()
  : _is_tiled(false) {
    this->_values[0] = BLOCKED;
    for (dim_t i = 1; i < NumDimensions; ++i) {
      this->_values[i] = NONE;
    }
  }

  /**
   * Constructor, initializes distribution with given distribution types
   * for every dimension.
   *
   * \b Example: 
   * \code
   *   // Blocked distribution in second dimension (y), cyclic distribution
   *   // in third dimension (z)
   *   DistributionSpec<3> ds(NONE, BLOCKED, CYCLIC);
   * \endcode
   */
  template<typename ... Values>
  DistributionSpec(Distribution value, Values ... values)
  : Dimensional<Distribution, NumDimensions>::Dimensional(value, values...),
    _is_tiled(false) {
    for (dim_t i = 1; i < NumDimensions; ++i) {
      if (this->_values[i].type == dash::internal::DIST_TILE) {
        _is_tiled = true;
        break;
      }
    }
  }
  
  /**
   * Whether the distribution in the given dimension is tiled.
   */
  bool is_tiled_in_dimension(
    unsigned int dimension) const {
    return (_is_tiled &&
            this->_values[dimension].type == dash::internal::DIST_TILE);
  }
  
  /**
   * Whether the distribution is tiled in any dimension.
   */
  bool is_tiled() const {
    return _is_tiled;
  }

private:
  bool _is_tiled;
};

/**
 * Offset and extent in a single dimension.
 */
template<typename IndexType = int>
struct ViewPair {
  typedef typename std::make_unsigned<IndexType>::type SizeType;
  /// Offset in dimension
  IndexType offset;
  /// Extent in dimension
  SizeType  extent;
};

/**
 * Equality comparison operator for ViewPair.
 */
template<typename IndexType>
static bool operator==(
  const ViewPair<IndexType> & lhs,
  const ViewPair<IndexType> & rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  return (
    lhs.offset == rhs.offset && 
    lhs.extent == rhs.extent);
}

/**
 * Inequality comparison operator for ViewPair.
 */
template<typename IndexType>
static bool operator!=(
  const ViewPair<IndexType> & lhs,
  const ViewPair<IndexType> & rhs) {
  return !(lhs == rhs);
}

template<typename IndexType>
std::ostream & operator<<(
  std::ostream & os,
  const ViewPair<IndexType> & viewpair) {
  os << "dash::ViewPair<" << typeid(IndexType).name() << ">(offset:"
     << viewpair.offset << " extent:"
     << viewpair.extent << ")";
  return os;
}

/**
 * Specifies view parameters for implementing submat, rows and cols
 *
 * \concept(DashCartesianSpaceConcept)
 */
template<
  dim_t NumDimensions,
  typename IndexType = int>
class ViewSpec : public Dimensional<ViewPair<IndexType>, NumDimensions> {
private:
  typedef ViewSpec<NumDimensions, IndexType>
    self_t;
  typedef typename std::make_unsigned<IndexType>::type
    SizeType;
  typedef ViewPair<IndexType>
    ViewPair_t;

public:
  /**
   * Default constructor, initialize with extent and offset 0 in all
   * dimensions.
   */
  ViewSpec()
  : _size(0),
    _rank(NumDimensions) {
    for (dim_t i = 0; i < NumDimensions; i++) {
      ViewPair_t vp { 0, 0 };
      this->_values[i] = vp;
      _extents[i]      = 0;
      _offsets[i]      = 0;
    }
  }

  /**
   * Constructor, initialize with given extents and offset 0 in all
   * dimensions.
   */
  ViewSpec(
    const std::array<SizeType, NumDimensions> & extents)
  : Dimensional<ViewPair_t, NumDimensions>(),
    _size(1),
    _rank(NumDimensions),
    _extents(extents) {
    for (auto i = 0; i < NumDimensions; ++i) {
      ViewPair_t vp { 0, extents[i] };
      this->_values[i]  = vp;
      _size            *= extents[i];
    }
  }

  /**
   * Constructor, initialize with given extents and offsets.
   */
  ViewSpec(
    const std::array<IndexType, NumDimensions> & offsets,
    const std::array<SizeType, NumDimensions>  & extents)
  : Dimensional<ViewPair_t, NumDimensions>(),
    _size(1),
    _rank(NumDimensions),
    _extents(extents),
    _offsets(offsets) {
    for (auto i = 0; i < NumDimensions; ++i) {
      ViewPair_t vp { offsets[i], extents[i] };
      this->_values[i]  = vp;
      _size            *= extents[i];
    }
  }

  /**
   * Copy constructor.
   */
  ViewSpec(const self_t & other)
  : Dimensional<ViewPair_t, NumDimensions>(
      static_cast< const Dimensional<ViewPair_t, NumDimensions> & >(other)),
    _size(other._size),
    _rank(other._rank),
    _extents(other._extents) {
    _offsets = { };
  }

  /**
   * Change the view specification's extent in every dimension.
   */
  template<typename ... Args>
  void resize(SizeType arg, Args... args) {
    static_assert(
      sizeof...(Args) == (NumDimensions-1),
      "Invalid number of arguments");
    std::array<SizeType, NumDimensions> extents = 
      { arg, (SizeType)(args)... };
    resize(extents);
  }

  /**
   * Change the view specification's extent and offset in every dimension.
   */
  void resize(const std::array<ViewPair_t, NumDimensions> & view) {
    for (dim_t i = 0; i < NumDimensions; i++) {
      this->_values[i] = view[i];
    }
    update_size();
  }

  /**
   * Change the view specification's extent in every dimension.
   */
  template<typename SizeType_>
  void resize(const std::array<SizeType_, NumDimensions> & extent) {
    for (dim_t i = 0; i < NumDimensions; i++) {
      this->_values[i].extent = extent[i];
    }
    update_size();
  }

  /**
   * Change the view specification's extent and offset in the
   * given dimension.
   */
  void resize_dim(
    dim_t     dimension,
    IndexType offset,
    SizeType  extent) {
    ViewPair_t vp { offset, extent };
    this->_values[dimension] = vp;
    update_size();
  }

  /**
   * Set rank of the view spec to a dimensionality between 1 and
   * \c NumDimensions.
   */
  void set_rank(dim_t dimensions) {
    if (dimensions > NumDimensions || dimensions < 1) {
      DASH_THROW(
        dash::exception::InvalidArgument,
        "Dimension for ViewSpec::set_rank must be between 1 and "
        << NumDimensions);
    }
    _rank = dimensions;
    update_size();
  }

  IndexType begin(unsigned int dimension) const {
    return this->_values[dimension].offset;
  }

  IndexType size() const {
    return _size;
  }

  IndexType size(unsigned int dimension) const {
    return this->_values[dimension].extent;
  }

  std::array<SizeType, NumDimensions> extents() const {
    return _extents;
  }

  std::array<IndexType, NumDimensions> offsets() const {
    return _offsets;
  }

private:
  SizeType                             _size;
  SizeType                             _rank;
  std::array<SizeType, NumDimensions>  _extents;
  std::array<IndexType, NumDimensions> _offsets;

  void update_size() {
    _size = 1;
    for (dim_t i = 0; i < _rank; ++i) {
      auto extent  = this->_values[i].extent;
      auto offset  = this->_values[i].offset;
      _size       *= extent;
      _extents[i]  = extent;
      _offsets[i]  = offset;
    }
  }
};

template<typename ElementType, dim_t NumDimensions>
std::ostream & operator<<(
  std::ostream & os,
  const Dimensional<ElementType, NumDimensions> & dimensional) {
  os << "dash::Dimensional<"
     << typeid(ElementType).name() << ","
     << NumDimensions << ">(";
  for (auto d = 0; d < NumDimensions; ++d) {
    os << dimensional._values[d] << ((d < NumDimensions-1) ? "," : "");
  }
  os << ")";
  return os;
}

} // namespace dash

#endif // DASH__DIMENSIONAL_H_
