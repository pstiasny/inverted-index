#include "phy.h"


BTreeForwardIndex::BTreeForwardIndex() {
    nodes = (char*)calloc(node_count, NODE_SIZE);

    int i;
    for (i = 0; i < node_count; i++) {
        auto n = get_node_at_index(i);
        n->initialize();
    }
}


BTreeForwardIndex::~BTreeForwardIndex() {
    free(nodes);
}


void BTreeForwardIndex::allocate_nodes() {
    auto old_node_count = node_count;

    node_count = 2 * node_count +  1;
    nodes = (char*)realloc(nodes, node_count * NODE_SIZE);
    if (!nodes) throw Error("Could not add new nodes - out of memory");

    int i;
    for (i = old_node_count; i < node_count; i++) {
        get_node_at_index(i)->initialize();
    }
}


NodeRef BTreeForwardIndex::initialize_new_node(decltype(ForwardIndexNode::type) type) {
    last_used_node++;
    if (last_used_node >= node_count) allocate_nodes();
    assert(last_used_node < node_count);
    auto n = get_node_at_index(last_used_node);
    n->type = type;
    return NodeRef(this, last_used_node);
}


NodeRef BTreeForwardIndex::get_node_at_index(uint32_t i) {
    if (i < 0 || i >= node_count) throw Error("bad node index");
    return NodeRef(this, i);
}


shared_ptr<Entity> BTreeForwardIndex::get(const string &id) {
    TreeCursor ki(this);

    ki.navigateToLeaf(id);

    for (; !ki.lastInNode(); ki.next()) {
    //for (; ki.nodeHasNext(); ki.next()) {
        auto k = ki.key();
        if (id == k) {
            return make_shared<Entity>(k, ki.content());
        }
    }
    return nullptr;
}


int BTreeForwardIndex::find_insert_pos(const NodeRef &n, StringIndex id_idx) {
    const char *new_id = sp.get(id_idx);

    int i;
    if (n->type == ForwardIndexNode::NODE)
        i = 1;  // for non-leaf nodes we always ingore the first ID
    else
        i = 0;

    for (; i < n->num_items; i++) {
        auto ki = n->get_id_idx_at(i);
        const char *id = sp.get(ki);
        if (strcmp(new_id, id) < 0) return i;
    }
    return n->num_items;
}


void BTreeForwardIndex::split_node(
    const NodeRef &left,
    const NodeRef &right,
    int split_pos
) {
    for (int i = split_pos; i < left->num_items; i++) {
        auto src = &(left->items[i]);
        right->add_item(right->num_items, src->id_idx, src->content_idx);
    }
    left->num_items = split_pos;

    right->next_idx = left->next_idx;
    left->next_idx = right.index;
}


void BTreeForwardIndex::insert_rec(
    forward_list<pair<NodeRef, int>> path,
    StringIndex id_idx,
    int content_idx,
    int insert_at
) {

    auto n = path.front().first;
    path.pop_front();

    if (n->has_space()) {
        n->add_item(insert_at, id_idx, content_idx);
        return;
    } else {
        // grow tree

        // add a new node, split items between both
        auto new_sibling = initialize_new_node(n->type);
        auto low_half = n->num_items / 2;
        if (insert_at <= low_half) {
            split_node(n, new_sibling, low_half);
            // insert left
            n->add_item(insert_at, id_idx, content_idx);
        } else {
            split_node(n, new_sibling, low_half + 1);
            // insert right
            new_sibling->add_item(insert_at - low_half - 1,
                                  id_idx, content_idx);
        }
        assert(abs(n->num_items - new_sibling->num_items) <= 1);

        // insert the new leaf into parent (recurse if necessary)
        if (!path.empty()) {
            auto parent = path.front().first;
            auto idx_in_parent = path.front().second;
            insert_rec(
                path,
                new_sibling->get_id_idx_at(0),
                new_sibling.index,
                idx_in_parent + 1);
        } else {
            auto new_root = initialize_new_node(ForwardIndexNode::NODE);
            new_root->add_item(0, n->get_id_idx_at(0), n.index);
            new_root->add_item(1, new_sibling->get_id_idx_at(0), new_sibling.index);
            root_node = new_root.index;
        }
    }
}


void BTreeForwardIndex::insert(shared_ptr<Entity> e) {
    auto id_idx = sp.append(e->id.c_str());
    auto content_idx = sp.append(e->content.c_str());

    TreeCursor ki(this);

    // find leaf to insert to
    ki.navigateToLeaf(e->id);

    insert_rec(ki.path(), id_idx, content_idx, find_insert_pos(ki.node(), id_idx));
}


shared_ptr<IForwardIndex> open_forward_index() {
    return make_shared<BTreeForwardIndex>();
}
