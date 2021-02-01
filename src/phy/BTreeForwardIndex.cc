#include <algorithm>
#include "phy.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <fcntl.h>


BTreeForwardIndex::BTreeForwardIndex(const string &path, uint32_t block_size) {
    nodes = (char*)calloc(node_count, block_size);
    this->block_size = block_size;

    fd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC);
    if (fd == -1) {
        perror("open");
        throw Error("could not open btree file");
    }

    unsigned int fsize = lseek(fd, 0, SEEK_END);
    if (fsize == -1)
        throw Error("Could not determine file size");
    if (fsize < node_count * block_size)
        ftruncate(fd, node_count * block_size);

    nodes = (char*)mmap(
        NULL,
        std::max(fsize, node_count * block_size),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0);
    if (nodes == (char*)-1) {
        perror("mmap");
        throw Error("Could not add new nodes (mmap)");
    }

    int i;
    for (i = 0; i < node_count; i++) {
        auto n = get_node_at_index(i);
        n->initialize();
    }
}


BTreeForwardIndex::~BTreeForwardIndex() {
    munmap(nodes, node_count * block_size);
    close(fd);
}


void BTreeForwardIndex::allocate_nodes() {
    auto old_node_count = node_count;

    // TODO: use a more efficient method of remapping
    if (-1 == msync(nodes, old_node_count * block_size, MS_SYNC)) {
        perror("msync");
        throw Error("Could not synchronize mmpapped memory");
    }
    if (-1 == munmap(nodes, old_node_count * block_size)) {
        perror("munmap");
        throw Error("Could not unmap mmapped memory");
    };
    node_count = 2 * node_count +  1;
    if (-1 == ftruncate(fd, node_count * block_size)) {
        perror("ftruncate");
        throw Error("Could not resize data file (ftruncate)");
    }
    nodes = (char*)mmap(
        nodes,
        node_count * block_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0);

    if (nodes == (char*)MAP_FAILED) {
        perror("mmap");
        throw Error("Could not add new nodes (mmap)");
    }

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


bool BTreeForwardIndex::has_space(const NodeRef &rn) const {
    ForwardIndexNode &n = *rn;
    return ((char*)(n.items + n.num_items + 2) <= (char*)&n + block_size);
}


void BTreeForwardIndex::add_item(const NodeRef &nr, int pos, StringIndex id_idx, uint32_t content_idx) {
    assert(has_space(nr));

    ForwardIndexNode &n = *nr;
    assert(pos <= n.num_items);

    for (int i = n.num_items; i >= pos; i--) {
        n.items[i + 1].id_idx = n.items[i].id_idx;
        n.items[i + 1].content_idx = n.items[i].content_idx;
    }
    n.num_items++;

    n.items[pos].id_idx = id_idx;
    n.items[pos].content_idx = content_idx;
}


void BTreeForwardIndex::split_node(
    const NodeRef &left,
    const NodeRef &right,
    int split_pos
) {
    for (int i = split_pos; i < left->num_items; i++) {
        auto src = &(left->items[i]);
        add_item(right, right->num_items, src->id_idx, src->content_idx);
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

    if (has_space(n)) {
        add_item(n, insert_at, id_idx, content_idx);
        return;
    } else {
        // grow tree

        // add a new node, split items between both
        auto new_sibling = initialize_new_node(n->type);
        auto low_half = n->num_items / 2;
        if (insert_at <= low_half) {
            split_node(n, new_sibling, low_half);
            // insert left
            add_item(n, insert_at, id_idx, content_idx);
        } else {
            split_node(n, new_sibling, low_half + 1);
            // insert right
            add_item(new_sibling, insert_at - low_half - 1, id_idx, content_idx);
        }
        assert(abs((int64_t)n->num_items - (int64_t)new_sibling->num_items) <= 1);

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
            add_item(new_root, 0, n->get_id_idx_at(0), n.index);
            add_item(new_root, 1, new_sibling->get_id_idx_at(0), new_sibling.index);
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

    item_count++;
}


shared_ptr<IForwardIndex> open_forward_index() {
    return make_shared<BTreeForwardIndex>("data/btree", 4096);
}
