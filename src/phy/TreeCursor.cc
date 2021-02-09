#include "phy.h"
#include <locale>


TreeCursor::TreeCursor(BTreeForwardIndex *tree) : tree(tree) {
    _path.push_front(make_pair(NodeRef(tree, tree->root_node), 0));
    node_idx = tree->root_node;
}


NodeRef TreeCursor::node() { 
    auto &head = _path.front();
    assert(node_idx == head.first.index);
    return tree->get_node_at_index(node_idx);  // head
}


const forward_list<pair<NodeRef, int>> & TreeCursor::path() {
    return _path;
}


void TreeCursor::next() {
    auto n = node();

    ++item_idx;

    _path.pop_front();
    _path.push_front(make_pair(n, item_idx));

    if (item_idx >= n->num_items && n->next_idx != -1) {
        //assert(false);

        //node_idx = n->next_idx;
        //item_idx = 0;
        //assert(node_idx < tree->node_count);
    }
}


void TreeCursor::moveToItem(int idx) {
    auto n = node();

    assert(idx >= 0);
    assert(idx < n->num_items);

    item_idx = idx;
    _path.pop_front();
    _path.push_front(make_pair(n, item_idx));
}


void TreeCursor::down() {
    assert(!leaf());
    auto n = node();
    node_idx = n->items[item_idx].content_idx;
    item_idx = 0;
    _path.push_front(make_pair(NodeRef(tree, node_idx), 0));
}


void TreeCursor::up() {
    assert(!_path.empty());
    _path.pop_front();

    assert(!_path.empty());
    auto p = _path.front();
    node_idx = p.first.index;
    item_idx = p.second;
}


bool TreeCursor::top() {
    return node_idx == tree->root_node;
}


bool TreeCursor::last() {
    auto n = node();
    return n->next_idx == -1 && lastInNode();
}


bool TreeCursor::lastInNode() {
    auto n = node();
    return item_idx >= n->num_items;
}


bool TreeCursor::nodeHasNext() {
    auto n = node();
    return item_idx + 1 < n->num_items;
}


bool TreeCursor::leaf() {
    auto n = node();
    return n->type == Node::LEAF;
}


const char * TreeCursor::key() {
    auto n = node();
    assert(item_idx < n->num_items);
    return tree->sp.get(n->items[item_idx].key_idx);
}

const char * TreeCursor::nextKey() {
    auto n = node();
    assert(nodeHasNext());
    return tree->sp.get(n->items[item_idx + 1].key_idx);
}

const char * TreeCursor::content() {
    auto n = node();
    assert(n->type == Node::LEAF);
    assert(item_idx < n->num_items);
    return tree->sp.get(n->items[item_idx].content_idx);
}


void TreeCursor::navigateToItem(size_t key_len, const char *key_) {
    NodeRef n = node();
    int low = 0, high = n->num_items;
    int mid;
    KeyCmp cmp;

    while (high > low) {
        mid = (low + high) / 2;
        moveToItem(mid);
        cmp = tree->compare_keys(n, mid, key_len, key_);
        if (cmp == KEY_MATCHES_ITEM) {
            break;
        } else if (cmp == KEY_BELOW_ITEM) {
            high = mid;
        } else if (cmp == KEY_ABOVE_ITEM) {
            low = mid + 1;
        }
    }
    if (cmp == KEY_BELOW_ITEM) moveToItem(max(0, item_idx - 1));
}


void TreeCursor::navigateToLeaf(const char *key_) {
    auto key_len = strlen(key_);
    while (!leaf()) {
        navigateToItem(key_len, key_);
        down();
    }
    navigateToItem(key_len, key_);
}


void TreeCursor::walk(TreeVisitor &tv) {
    if (leaf()) tv.enterLeaf(*this, node()); else tv.enterNode(*this, node());

    while (true) {
        if (top() && lastInNode()) break;

        if (lastInNode()) {
            if (leaf()) tv.exitLeaf(*this, node()); else tv.exitNode(*this, node());
            up();
            next();
            continue;
        }

        if (leaf()) {
            while (!lastInNode()) {
                tv.enterLeafItem(*this, node(), item_idx, &node()->items[item_idx]);
                next();
            }
        } else {
            tv.enterNodeItem(*this, node(), item_idx, &node()->items[item_idx]);
            down();
            if (leaf()) tv.enterLeaf(*this, node()); else tv.enterNode(*this, node());
        }
    }

    if (leaf()) tv.exitLeaf(*this, node()); else tv.exitNode(*this, node());
}
