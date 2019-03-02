#include "phy.h"


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


void TreeCursor::down() {
    assert(!leaf());
    auto n = node();
    node_idx = n->items[item_idx].content_idx;
    item_idx = 0;
    _path.push_front(make_pair(NodeRef(tree, node_idx), 0));
}


//void TreeCursor::up() {
    //assert(!_path.empty());
    //auto p = _path.front();
    //node_idx = p.first.index;
    //item_idx = p.second;
    //_path.pop_front();
//}


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
    return n->type == ForwardIndexNode::LEAF;
}


const char * TreeCursor::key() {
    auto n = node();
    assert(n->type == ForwardIndexNode::LEAF);
    assert(item_idx < n->num_items);
    return tree->sp.get(n->items[item_idx].id_idx);
}

const char * TreeCursor::nextKey() {
    auto n = node();
    assert(nodeHasNext());
    return tree->sp.get(n->items[item_idx + 1].id_idx);
}

const char * TreeCursor::content() {
    auto n = node();
    assert(n->type == ForwardIndexNode::LEAF);
    assert(item_idx < n->num_items);
    return tree->sp.get(n->items[item_idx].content_idx);
}


void TreeCursor::navigateToLeaf(const string &id) {
    while (!leaf()) {
        while (nodeHasNext() && (nextKey() <= id))
            next();
        down();
    }
}
