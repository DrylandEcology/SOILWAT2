#include "gtest/gtest.h"

namespace {

TEST(IsTestTest, Failure) {
  // This test belongs to the IsTestTest test case.

  EXPECT_FALSE(3 == 4);
}

} // namespace
