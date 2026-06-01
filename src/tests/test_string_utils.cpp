#include <gtest/gtest.h>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>

TEST(SplitTest, BasicSplit) {
    auto result = split("a,b,c", ',');
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(SplitTest, EmptyString) {
    auto result = split("", ',');
    EXPECT_TRUE(result.size() == 1 || result.empty());
}

TEST(SplitTest, NoDelimiter) {
    auto result = split("hello", ',');
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "hello");
}

TEST(SplitTest, MultipleDelimiters) {
    auto result = split("a,,b", ',');
    EXPECT_GT(result.size(), 0);
}

TEST(DumpTest, Integer) {
    EXPECT_EQ(dump(5), "5");
}

TEST(DumpTest, Char) {
    EXPECT_EQ(dump<const char>('a'), "'a'");
}

TEST(DumpTest, String) {
    EXPECT_EQ(dump<std::string>("Hello World"), "\"Hello World\"");
}

TEST(DumpTest, VectorOfStrings) {
    EXPECT_EQ(dump<std::vector<std::string>>({"a", "b", "c"}),
              "{\"a\", \"b\", \"c\"}");
}

TEST(DumpTest, Pair) {
    EXPECT_EQ(dump(std::make_pair(5, std::string("Hello"))),
              "(5, \"Hello\")");
}

TEST(DumpTest, Tuple) {
    EXPECT_EQ(dump(std::make_tuple(5, std::string("Hello"), 3)),
              "(5, \"Hello\", 3)");
}

TEST(DumpTest, VectorOfTuples) {
    EXPECT_EQ(dump(std::vector<std::tuple<int, std::string, int>>(
                  2, std::make_tuple(5, std::string("Hello"), 3))),
              "{(5, \"Hello\", 3), (5, \"Hello\", 3)}");
}