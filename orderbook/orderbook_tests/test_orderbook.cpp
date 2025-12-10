#include <gtest/gtest.h>
#include <iostream>

// Example function to test
int add(int a, int b) {
    std::cout << "Adding " << a << " and " << b << std::endl;
    return a + b;
}

TEST(OrderbookTest, AddFunction) {
    EXPECT_EQ(add(2, 3), 5);
    EXPECT_EQ(add(-1, 1), 0);
    EXPECT_EQ(add(0, 0), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}