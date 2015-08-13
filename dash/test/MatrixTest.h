#ifndef DASH__TEST__MATRIX_TEST_H_
#define DASH__TEST__MATRIX_TEST_H_

#include <gtest/gtest.h>
#include <libdash.h>

/**
 * Test fixture for class dash::Matrix
 */
class MatrixTest : public ::testing::Test {
protected:
  size_t _dash_id;
  size_t _dash_size;

  MatrixTest() 
  : _dash_id(0),
    _dash_size(0) {
    LOG_MESSAGE(">>> Test suite: MatrixTest");
  }

  virtual ~MatrixTest() {
    LOG_MESSAGE("<<< Closing test suite: MatrixTest");
  }

  virtual void SetUp() {
    dash::Team::All().barrier();
    _dash_id   = dash::myid();
    _dash_size = dash::size();
    LOG_MESSAGE("===> Running test case with %d units ...",
                _dash_size);
  }

  virtual void TearDown() {
    dash::Team::All().barrier();
    LOG_MESSAGE("<=== Finished test case with %d units",
                _dash_size);
  }
};

#endif // DASH__TEST__MATRIX_TEST_H_
