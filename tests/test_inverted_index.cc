#include "gtest/gtest.h"
#include "inverted_index.h"


TEST(TestDB, TestSimpleAddThenGet) {
    remove("test_log");

    DB db("test_log");

    auto e = make_shared<Entity>("test_id_1", "test content");
    db.add(e);

    auto result = db.get("test_id_1");
    EXPECT_EQ(result->id, "test_id_1");
    EXPECT_EQ(result->content, "test content");
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

    EXPECT_EQ(db.query(*q_x), PostingList({e1, e2, e3}));
    EXPECT_EQ(db.query(*q_y), PostingList({e1, e2, e4}));
    EXPECT_EQ(db.query(*q_z), PostingList({e1, e3, e4}));
    EXPECT_EQ(db.query(*q_xy), PostingList({e1, e2}));
    EXPECT_EQ(db.query(*q_xz), PostingList({e1, e3}));
    EXPECT_EQ(db.query(*q_yz), PostingList({e1, e4}));
    EXPECT_EQ(db.query(*q_xyz), PostingList({e1}));
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
