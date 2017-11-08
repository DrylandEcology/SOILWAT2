#include "gtest/gtest.h"
#include "../rands.h"

namespace {
  // This test belongs to the beta random number generator
  TEST(BetaGenerator, ZeroToOneOutput) {
    EXPECT_LT(genbet(0.5, 2), 1);
    EXPECT_LT(genbet(1, 3), 1);
    EXPECT_GT(genbet(1, 4), 0);
    EXPECT_GT(genbet(0.25, 1), 0);

  }

  TEST(BetaGenerator, Errors) {
    EXPECT_ANY_THROW(genbet(-0.5, 2));
    EXPECT_ANY_THROW(genbet(1, -3));
    EXPECT_ANY_THROW(genbet(-1, -3));
  }


TEST(IsTestTest, Failure) {
  // This test belongs to the IsTestTest test case.

  EXPECT_FALSE(3 == 4);
}

} // namespace


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();


}
