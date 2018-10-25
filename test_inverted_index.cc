#include "gtest/gtest.h"
#include "inverted_index.cc"


TEST(TestDB, TestSimpleAddThenGet) {
    remove("test_log");

    DB db("test_log");
    auto e = make_shared<Entity>("test_id_1", "test content");

    db.add(e);

    auto result = db.get("test_id_1");
    EXPECT_EQ(result->id, "test_id_1");
    EXPECT_EQ(result->content, "test content");
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
