#include "phy.h"


NodeRef::NodeRef(BTreeForwardIndex *tree, int index) {
    this->tree = tree;
    this->index = index;
}


Node * NodeRef::get() const {
    assert(index < tree->node_count);
    return (Node*)(tree->nodes + index * tree->block_size);
}

Node * NodeRef::operator->() const {
    return get();
}


Node& NodeRef::operator*() const {
    return *get();
}

bool NodeRef::operator==(const NodeRef &other) const {
    return (tree == other.tree) && (index == other.index);
}

ostream& operator<<(ostream& os, const NodeRef& nr) {
    os << "NodeRef(..., " << nr.index << ")";
    return os;
}