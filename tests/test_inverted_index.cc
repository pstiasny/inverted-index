#include <random>
#include "gtest/gtest.h"
#include "inverted_index.h"


TEST(TestDB, TestSimpleAddThenGet) {
    remove("test_log");

    DB db("test_log");

    ASSERT_EQ(db.get("nonexistent"), nullptr);

    auto e = make_shared<Entity>("test_id_1", "test content");
    db.add(e);

    auto result = db.get("test_id_1");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->id, "test_id_1");
    EXPECT_EQ(result->content, "test content");
}


TEST(TestDB, TestLargeAddThenGet) {
    remove("test_log");

    DB db("test_log");

    int i;
    for (i = 0; i < 10000; i++) {
        auto e = make_shared<Entity>(to_string(i), "test content " + to_string(i));
        db.add(e);
    }

    for (i = 0; i < 10000; i++) {
        auto result = db.get(to_string(i));
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->id, to_string(i));
        EXPECT_EQ(result->content, "test content " + to_string(i));
    }

    ASSERT_EQ(db.get("nonexistent"), nullptr);
}


TEST(TestDB, TestLargeAddThenGetInRandomOrder) {
    remove("test_log");

    DB db("test_log");

    int i;
    vector<int> to_add;
    for (i = 0; i < 10000; i++) {
        to_add.push_back(i);
    }
    shuffle(to_add.begin(), to_add.end(), default_random_engine(1234));

    for (int j : to_add) {
        auto e = make_shared<Entity>(to_string(j), "test content " + to_string(j));
        db.add(e);
    }

    shuffle(to_add.begin(), to_add.end(), default_random_engine(4321));
    for (int j : to_add) {
        auto result = db.get(to_string(j));
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->id, to_string(j));
        EXPECT_EQ(result->content, "test content " + to_string(j));
    }

    ASSERT_EQ(db.get("nonexistent"), nullptr);
}


TEST(TestDB, TestRestoreFromLog) {
    remove("test_log");

    {
        DB db("test_log");
        auto e = make_shared<Entity>("test_id_1", "test content");
        db.add(e);
        db.close();
    }

    {
        DB db("test_log");
        auto result = db.get("test_id_1");
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->id, "test_id_1");
        EXPECT_EQ(result->content, "test content");
    }
}


TEST(TestDB, TestQuery) {
    remove("test_log");

    DB db("test_log");

    auto e1 = make_shared<Entity>("test_id_1", "x y z");
    db.add(e1);
    auto e2 = make_shared<Entity>("test_id_2", "x y");
    db.add(e2);
    auto e3 = make_shared<Entity>("test_id_3", "x z");
    db.add(e3);
    auto e4 = make_shared<Entity>("test_id_4", "y z");
    db.add(e4);

    shared_ptr<Query> q_x = make_shared<Term>("x");
    shared_ptr<Query> q_y = make_shared<Term>("y");
    shared_ptr<Query> q_z = make_shared<Term>("z");

    shared_ptr<Query> q_xy = make_shared<And>(q_x, q_y);
    shared_ptr<Query> q_xz = make_shared<And>(q_x, q_z);
    shared_ptr<Query> q_yz = make_shared<And>(q_y, q_z);

    shared_ptr<Query> q_xyz = make_shared<And>(q_xy, q_z);

    EXPECT_EQ(db.query(*q_x), PostingList({e1->id, e2->id, e3->id}));
    EXPECT_EQ(db.query(*q_y), PostingList({e1->id, e2->id, e4->id}));
    EXPECT_EQ(db.query(*q_z), PostingList({e1->id, e3->id, e4->id}));
    EXPECT_EQ(db.query(*q_xy), PostingList({e1->id, e2->id}));
    EXPECT_EQ(db.query(*q_xz), PostingList({e1->id, e3->id}));
    EXPECT_EQ(db.query(*q_yz), PostingList({e1->id, e4->id}));
    EXPECT_EQ(db.query(*q_xyz), PostingList({e1->id}));
}


TEST(TestInterpreter, TestSimple) {
    remove("test_log");

    DB db("test_log");
    Parser p;

    stringstream output;
    Interpreter i(db, output);

    i.interpret(*p.parse("(add \"foo\" \"bar\")"));
    EXPECT_EQ(output.str(), "");

    output.str("");
    i.interpret(*p.parse("(get \"foo\")"));
    EXPECT_EQ(output.str(), "bar\n");

    output.str("");
    i.interpret(*p.parse("(query \"bar\")"));
    EXPECT_EQ(output.str(), "foo\n");

    output.str("");
    i.interpret(*p.parse("(query \"bar\" \"baz\")"));
    EXPECT_EQ(output.str(), "");
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
