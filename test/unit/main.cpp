#include <gtest/gtest.h>  // for InitGoogleTest, RUN_ALL_TESTS

int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
