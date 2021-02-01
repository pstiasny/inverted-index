#include "phy.h"


NodeRef::NodeRef(BTreeForwardIndex *tree, int index) {
    this->tree = tree;
    this->index = index;
}


ForwardIndexNode * NodeRef::operator->() const {
    assert(index < tree->node_count);
    return (ForwardIndexNode*)(tree->nodes + index * tree->block_size);
}


ForwardIndexNode& NodeRef::operator*() const {
    assert(index < tree->node_count);
    return *(ForwardIndexNode*)(tree->nodes + index * tree->block_size);
}
