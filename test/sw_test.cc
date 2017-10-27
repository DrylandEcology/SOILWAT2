#include "gtest/gtest.h"

namespace {

TEST(IsTestTest, Failure) {
  // This test belongs to the IsTestTest test case.

  EXPECT_FALSE(3 == 4);
}

} // namespace


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
