#include <random>
#include "gtest/gtest.h"
#include "phy.h"


class TestBTreeInvertedIndex : public ::testing::Test {
    protected:
    void SetUp() override {
        remove("data/test-tree");
        btree = new BTreeForwardIndex("data/test-tree", 256);
    }
    void TearDown() override {
        delete btree;
    }

    BTreeForwardIndex *btree;
};


TEST_F(TestBTreeInvertedIndex, TestSequentialAddThenGet) {
    int i;
    for (i = 0; i < 10000; i++) {
        auto e = make_shared<Entity>(to_string(i), "test content " + to_string(i));
        btree->insert(e);
    }

    for (i = 0; i < 10000; i++) {
        auto result = btree->get(to_string(i));
        ASSERT_NE(result, nullptr) << "Missing item: " << i;
        EXPECT_EQ(result->id, to_string(i));
        EXPECT_EQ(result->content, "test content " + to_string(i));
    }

    ASSERT_EQ(btree->get("nonexistent"), nullptr);
}


TEST_F(TestBTreeInvertedIndex, TestRandomAddThenGet) {
    int i;
    vector<int> to_add;
    for (i = 0; i < 10000; i++) {
        to_add.push_back(i);
    }
    shuffle(to_add.begin(), to_add.end(), default_random_engine(1234));

    for (int j : to_add) {
        auto e = make_shared<Entity>(to_string(j), "test content " + to_string(j));
        btree->insert(e);
    }

    shuffle(to_add.begin(), to_add.end(), default_random_engine(4321));
    for (int j : to_add) {
        auto result = btree->get(to_string(j));
        ASSERT_NE(result, nullptr) << "Missing item: " << j;
        EXPECT_EQ(result->id, to_string(j));
        EXPECT_EQ(result->content, "test content " + to_string(j));
    }

    ASSERT_EQ(btree->get("nonexistent"), nullptr);
}
