#include <random>
#include "gtest/gtest.h"
#include "phy.h"


const uint16_t INNER_KEY_LEN = 8;


class InvariantChecker : public TreeVisitor {
    public:

    string slong;
    string sinner;
    bool init;
    int counted_items;

    InvariantChecker() {
        init = false;
        counted_items = 0;
    }

    virtual void enterNodeItem(TreeCursor &tc, NodeRef nr, int item_idx, NodeItem *item) {
        string s = nr->copy_inner_key_data(item_idx);

        if (strlen(tc.key()) <= INNER_KEY_LEN)
            ASSERT_EQ(s, tc.key()) << "node " << nr.index << " item " << item_idx;

        if (item_idx > 0) {
            ASSERT_TRUE(sinner <= s) << "node " << nr.index << " item " << item_idx;
            sinner = s;
        }
    };
    virtual void enterLeafItem(TreeCursor &tc, NodeRef nr, int item_idx, NodeItem *item) {
        ++counted_items;

        string s = nr->copy_inner_key_data(item_idx);

        if (strlen(tc.key()) <= INNER_KEY_LEN)
            ASSERT_EQ(s, tc.key()) << "node " << nr.index << " item " << item_idx;

        ASSERT_TRUE(sinner <= s) << "node " << nr.index << " item " << item_idx;
        sinner = s;
    };
};


class TestBTreeInvertedIndex : public ::testing::Test {
    protected:
    void SetUp() override {
        remove("data/test-tree");
        btree = new BTreeForwardIndex("data/test-tree", 256, INNER_KEY_LEN);
    }
    void TearDown() override {
        delete btree;
    }
    void check_tree_invariants() {
        TreeCursor tc(btree);
        InvariantChecker ic;
        tc.walk(ic);
        ASSERT_EQ(ic.counted_items, btree->item_count);
    }
    void print_tree() {
        TreeCursor tc(btree);
        TreePrinter tp(cout);
        tc.walk(tp);
    }

    BTreeForwardIndex *btree;
};


TEST_F(TestBTreeInvertedIndex, TestSequentialAddThenGet) {
    int i;
    for (i = 0; i < 10000; i++) {
        auto e = make_shared<Entity>(to_string(i), "test content " + to_string(i));
        btree->insert(e);

        // The check scans the whole tree.  For performance reasons
        // we do it only for the first 100 inserts and at the end.
        if (i < 100) {  
            check_tree_invariants();
            if (HasFatalFailure()) { print_tree(); return; }
        }
    }
    check_tree_invariants();
    if (HasFatalFailure()) { print_tree(); return; }

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

        // The check scans the whole tree.  For performance reasons
        // we do it only for the first 100 inserts and at the end.
        if (i < 100) {  
            check_tree_invariants();
            if (HasFatalFailure()) { print_tree(); return; }
        }
    }
    check_tree_invariants();
    if (HasFatalFailure()) { print_tree(); return; }

    shuffle(to_add.begin(), to_add.end(), default_random_engine(4321));
    for (int j : to_add) {
        auto result = btree->get(to_string(j));
        ASSERT_NE(result, nullptr) << "Missing item: " << j;
        EXPECT_EQ(result->id, to_string(j));
        EXPECT_EQ(result->content, "test content " + to_string(j));
    }

    ASSERT_EQ(btree->get("nonexistent"), nullptr);
}


TEST_F(TestBTreeInvertedIndex, TestCompareKeysShort) {
    assert(btree->max_inner_key_length >= 1);

    NodeRef nr = btree->get_node_at_index(0);

    const char *s = "b";
    StringIndex si = btree->sp.append(s);
    btree->add_item(nr.get(), 0, si, 1, s, 1234);

    EXPECT_EQ(btree->compare_keys(nr, 0, 0, ""), KEY_BELOW_ITEM);
    EXPECT_EQ(btree->compare_keys(nr, 0, 1, "a"), KEY_BELOW_ITEM);
    EXPECT_EQ(btree->compare_keys(nr, 0, 1, "b"), KEY_MATCHES_ITEM);
    EXPECT_EQ(btree->compare_keys(nr, 0, 2, "bb"), KEY_ABOVE_ITEM);
    EXPECT_EQ(btree->compare_keys(nr, 0, 1, "c"), KEY_ABOVE_ITEM);
}


TEST_F(TestBTreeInvertedIndex, TestCompareKeysLong) {
    assert(btree->max_inner_key_length < 32);

    NodeRef nr = btree->get_node_at_index(0);

    const char *s = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    StringIndex si = btree->sp.append(s);
    btree->add_item(nr.get(), 0, si, btree->max_inner_key_length, s, 1234);

    const char *a = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
               *b = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
               *c = "cccccccccccccccccccccccccccccccc";
    EXPECT_EQ(
        btree->compare_keys(nr, 0, strlen(a), a),
        KEY_BELOW_ITEM);
    EXPECT_EQ(
        btree->compare_keys(nr, 0, strlen(b), b),
        KEY_MATCHES_ITEM);
    EXPECT_EQ(
        btree->compare_keys(nr, 0, strlen(c), c),
        KEY_ABOVE_ITEM);
}


TEST_F(TestBTreeInvertedIndex, TestNavigateToItem) {
    NodeRef nr = btree->get_node_at_index(0);

    const char *k1 = "b", *k2 = "d";
    StringIndex k1_idx = btree->sp.append(k1),
                k2_idx = btree->sp.append(k2);

    btree->add_item(nr.get(), 0, k1_idx, 1, k1, 1000);
    btree->add_item(nr.get(), 1, k2_idx, 1, k2, 2000);

    {
        TreeCursor tc(btree);
        tc.navigateToItem(1, "a");
        ASSERT_EQ(tc.getItemIdx(), 0);
    }

    {
        TreeCursor tc(btree);
        tc.navigateToItem(1, "b");
        ASSERT_EQ(tc.getItemIdx(), 0);
    }

    {
        TreeCursor tc(btree);
        tc.navigateToItem(1, "c");
        ASSERT_EQ(tc.getItemIdx(), 0);
    }

    {
        TreeCursor tc(btree);
        tc.navigateToItem(1, "d");
        ASSERT_EQ(tc.getItemIdx(), 1);
    }

    {
        TreeCursor tc(btree);
        tc.navigateToItem(1, "e");
        ASSERT_EQ(tc.getItemIdx(), 1);
    }
}

TEST_F(TestBTreeInvertedIndex, TestNavigateToLeaf) {
    NodeRef root  = btree->initialize_new_node(Node::NODE),
            leaf1 = btree->initialize_new_node(Node::LEAF),
            leaf2 = btree->initialize_new_node(Node::LEAF);
    btree->root_node = root.index;

    const char *k1 = "b", *k2 = "d";
    StringIndex k1_idx = btree->sp.append(k1),
                k2_idx = btree->sp.append(k2);

    btree->add_item(leaf1.get(), 0, k1_idx, 1, k1, 1000);
    btree->add_item(leaf2.get(), 0, k2_idx, 1, k2, 2000);
    btree->add_item(root.get(), 0, k1_idx, 1, k1, leaf1.index);
    btree->add_item(root.get(), 1, k2_idx, 1, k2, leaf2.index);

    {
        TreeCursor tc(btree);
        tc.navigateToLeaf("a");
        ASSERT_EQ(tc.path(), (forward_list<pair<NodeRef, int>>{{leaf1, 0}, {root, 0}}));
        ASSERT_EQ(tc.node(), leaf1);
    }

    {
        TreeCursor tc(btree);
        tc.navigateToLeaf("b");
        ASSERT_EQ(tc.path(), (forward_list<pair<NodeRef, int>>{{leaf1, 0}, {root, 0}}));
        ASSERT_EQ(tc.node(), leaf1);
    }

    {
        TreeCursor tc(btree);
        tc.navigateToLeaf("c");
        ASSERT_EQ(tc.path(), (forward_list<pair<NodeRef, int>>{{leaf1, 0}, {root, 0}}));
        ASSERT_EQ(tc.node(), leaf1);
    }

    {
        TreeCursor tc(btree);
        tc.navigateToLeaf("d");
        ASSERT_EQ(tc.path(), (forward_list<pair<NodeRef, int>>{{leaf2, 0}, {root, 1}}));
        ASSERT_EQ(tc.node(), leaf2);
    }

    {
        TreeCursor tc(btree);
        tc.navigateToLeaf("e");
        ASSERT_EQ(tc.path(), (forward_list<pair<NodeRef, int>>{{leaf2, 0}, {root, 1}}));
        ASSERT_EQ(tc.node(), leaf2);
    }
}


TEST_F(TestBTreeInvertedIndex, TestSplitNode) {
    NodeRef nr1 = btree->get_node_at_index(0);

    const char *k1 = "a",
               *k2 = "b";
    StringIndex k1_idx = btree->sp.append(k1);
    StringIndex k2_idx = btree->sp.append(k2);

    btree->add_item(nr1.get(), 0, k1_idx, 1, k1, 1000);
    btree->add_item(nr1.get(), 1, k2_idx, 1, k2, 2000);

    btree->initialize_new_node(Node::LEAF);
    NodeRef nr2 = btree->get_node_at_index(1);
    btree->split_node(nr1, nr2, 1);

    ASSERT_EQ(nr1->num_items, 1);
    ASSERT_EQ(
        strncmp(nr1->get_inner_key_data_ptr(0), "a", 1),
        0);
    ASSERT_EQ(
        nr1->items[0].inner_key_length,
        1);
    ASSERT_EQ(
        nr1->items[0].key_idx,
        k1_idx);

    ASSERT_EQ(nr2->num_items, 1);
    ASSERT_EQ(
        strncmp(nr2->get_inner_key_data_ptr(0), "b", 1),
        0);
    ASSERT_EQ(
        nr2->items[0].inner_key_length,
        1);
    ASSERT_EQ(
        nr2->items[0].key_idx,
        k2_idx);
}


TEST_F(TestBTreeInvertedIndex, TestTreeCursor) {
    NodeRef root  = btree->initialize_new_node(Node::NODE),
            leaf1 = btree->initialize_new_node(Node::LEAF),
            leaf2 = btree->initialize_new_node(Node::LEAF);
    btree->root_node = root.index;

    const char *k1 = "a", *k2 = "b", *k3 = "c", *k4 = "d",
               *c1 = "A", *c2 = "B", *c3 = "C", *c4 = "D";
    StringIndex k1_idx = btree->sp.append(k1),
                k2_idx = btree->sp.append(k2),
                k3_idx = btree->sp.append(k3),
                k4_idx = btree->sp.append(k4),
                c1_idx = btree->sp.append(c1),
                c2_idx = btree->sp.append(c2),
                c3_idx = btree->sp.append(c3),
                c4_idx = btree->sp.append(c4);
    const int len = 1;

    btree->add_item(leaf1.get(), 0, k1_idx, len, k1, c1_idx);
    btree->add_item(leaf1.get(), 1, k2_idx, len, k2, c2_idx);
    btree->add_item(leaf2.get(), 0, k3_idx, len, k3, c3_idx);
    btree->add_item(leaf2.get(), 1, k4_idx, len, k4, c4_idx);
    btree->add_item(root.get(), 0, k1_idx, len, k1, leaf1.index);
    btree->add_item(root.get(), 1, k3_idx, len, k3, leaf2.index);

    TreeCursor tc(btree);

    ASSERT_EQ(tc.node().index, root.index);
    ASSERT_EQ(tc.getItemIdx(), 0);
    ASSERT_STREQ(tc.key(), "a");
    ASSERT_EQ(tc.leaf(), false);
    ASSERT_EQ(tc.nodeHasNext(), true);
    ASSERT_EQ(tc.top(), true);

    tc.down();
    ASSERT_EQ(tc.node().index, leaf1.index);
    ASSERT_EQ(tc.getItemIdx(), 0);
    ASSERT_STREQ(tc.key(), "a");
    ASSERT_EQ(tc.leaf(), true);
    ASSERT_EQ(tc.nodeHasNext(), true);
    ASSERT_STREQ(tc.content(), "A");
    ASSERT_EQ(tc.top(), false);

    tc.next();
    ASSERT_EQ(tc.node().index, leaf1.index);
    ASSERT_EQ(tc.getItemIdx(), 1);
    ASSERT_STREQ(tc.key(), "b");
    ASSERT_EQ(tc.leaf(), true);
    ASSERT_EQ(tc.nodeHasNext(), false);
    ASSERT_STREQ(tc.content(), "B");
    ASSERT_EQ(tc.top(), false);

    tc.up();
    ASSERT_EQ(tc.node().index, root.index);
    ASSERT_EQ(tc.getItemIdx(), 0);
    ASSERT_STREQ(tc.key(), "a");
    ASSERT_EQ(tc.leaf(), false);
    ASSERT_EQ(tc.nodeHasNext(), true);
    ASSERT_EQ(tc.top(), true);

    tc.next();
    ASSERT_EQ(tc.node().index, root.index);
    ASSERT_EQ(tc.getItemIdx(), 1);
    ASSERT_STREQ(tc.key(), "c");
    ASSERT_EQ(tc.leaf(), false);
    ASSERT_EQ(tc.nodeHasNext(), false);
    ASSERT_EQ(tc.top(), true);

    tc.down();
    ASSERT_EQ(tc.node().index, leaf2.index);
    ASSERT_EQ(tc.getItemIdx(), 0);
    ASSERT_STREQ(tc.key(), "c");
    ASSERT_EQ(tc.leaf(), true);
    ASSERT_EQ(tc.nodeHasNext(), true);
    ASSERT_STREQ(tc.content(), "C");
    ASSERT_EQ(tc.top(), false);

    tc.next();
    ASSERT_EQ(tc.node().index, leaf2.index);
    ASSERT_EQ(tc.getItemIdx(), 1);
    ASSERT_STREQ(tc.key(), "d");
    ASSERT_EQ(tc.leaf(), true);
    ASSERT_EQ(tc.nodeHasNext(), false);
    ASSERT_STREQ(tc.content(), "D");
    ASSERT_EQ(tc.top(), false);

    tc.up();
    ASSERT_EQ(tc.node().index, root.index);
    ASSERT_EQ(tc.getItemIdx(), 1);
    ASSERT_STREQ(tc.key(), "c");
    ASSERT_EQ(tc.leaf(), false);
    ASSERT_EQ(tc.nodeHasNext(), false);
    ASSERT_EQ(tc.top(), true);
}


TEST_F(TestBTreeInvertedIndex, TestTreePrinter) {
    NodeRef root  = btree->initialize_new_node(Node::NODE),
            leaf1 = btree->initialize_new_node(Node::LEAF),
            leaf2 = btree->initialize_new_node(Node::LEAF);
    btree->root_node = root.index;

    const char *k1 = "a", *k2 = "b", *k3 = "c", *k4 = "d",
               *c1 = "A", *c2 = "B", *c3 = "C", *c4 = "D";
    StringIndex k1_idx = btree->sp.append(k1),
                k2_idx = btree->sp.append(k2),
                k3_idx = btree->sp.append(k3),
                k4_idx = btree->sp.append(k4),
                c1_idx = btree->sp.append(c1),
                c2_idx = btree->sp.append(c2),
                c3_idx = btree->sp.append(c3),
                c4_idx = btree->sp.append(c4);
    const int len = 1;

    btree->add_item(leaf1.get(), 0, k1_idx, len, k1, c1_idx);
    btree->add_item(leaf1.get(), 1, k2_idx, len, k2, c2_idx);
    btree->add_item(leaf2.get(), 0, k3_idx, len, k3, c3_idx);
    btree->add_item(leaf2.get(), 1, k4_idx, len, k4, c4_idx);
    btree->add_item(root.get(), 0, k1_idx, len, k1, leaf1.index);
    btree->add_item(root.get(), 1, k3_idx, len, k3, leaf2.index);

    TreeCursor tc(btree);
    stringstream output;
    TreePrinter tp(output);
    tc.walk(tp);

    ASSERT_EQ(
        output.str(),
        "enter inner node " + to_string(root.index) + "\n"
        "  inner node key #0: a\n"
        "  enter leaf node " + to_string(leaf1.index) + "\n"
        "    leaf node key #0: a\n"
        "      content: A\n"
        "    leaf node key #1: b\n"
        "      content: B\n"
        "  exit leaf node " + to_string(leaf1.index) + "\n"
        "  inner node key #1: c\n"
        "  enter leaf node " + to_string(leaf2.index) + "\n"
        "    leaf node key #0: c\n"
        "      content: C\n"
        "    leaf node key #1: d\n"
        "      content: D\n"
        "  exit leaf node " + to_string(leaf2.index) + "\n"
        "exit inner node " + to_string(root.index) + "\n"
    );
}
