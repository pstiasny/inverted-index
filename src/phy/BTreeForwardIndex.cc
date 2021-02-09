#include <algorithm>
#include <cstring>
#include "phy.h"
#include <locale>
#include <random>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <fcntl.h>


BTreeForwardIndex::BTreeForwardIndex(
    const string &path,
    uint32_t block_size,
    uint16_t max_inner_key_length
) {
    this->block_size = block_size;
    this->max_inner_key_length = max_inner_key_length;

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
        n->initialize(block_size);
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
        get_node_at_index(i)->initialize(block_size);
    }
}


NodeRef BTreeForwardIndex::initialize_new_node(decltype(Node::type) type) {
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

    const char *key = id.c_str();
    uint16_t inner_key_length = min(id.length(), (size_t)max_inner_key_length);

    ki.navigateToLeaf(key);

    if (
        !ki.lastInNode() &&
        (KEY_MATCHES_ITEM == compare_keys(ki.node(), ki.getItemIdx(), inner_key_length, key))
    ) {
        return make_shared<Entity>(id, ki.content());
    } else {
        return nullptr;
    }
}


int BTreeForwardIndex::find_insert_pos(
    const NodeRef &n, 
    StringIndex key_idx,
    uint16_t inner_key_length,
    const char *key_data
) {
    int i;
    if (n->type == Node::NODE)
        i = 1;  // for non-leaf nodes we always ingore the first ID
    else
        i = 0;

    for (; i < n->num_items; i++) {
        if (compare_keys(n, i, inner_key_length, key_data) == KEY_BELOW_ITEM) return i;
    }
    return n->num_items;
}


void BTreeForwardIndex::add_item(
    Node *n,
    int pos,
    StringIndex key_idx,
    uint16_t inner_key_length,
    const char *key_data,
    uint32_t content_idx
) {
    assert(n->inner_key_data_start >= 0);
    assert(n->inner_key_data_start <= block_size);
    assert(n->has_space(inner_key_length));
    assert(pos <= n->num_items);

    for (int i = n->num_items - 1; i >= pos; i--) {
        n->items[i + 1] = n->items[i];
    }
    n->num_items++;

    NodeItem *item = &n->items[pos];
    item->key_idx = key_idx;
    item->content_idx = content_idx;
    item->inner_key_length = inner_key_length;

    n->inner_key_data_start -= inner_key_length;
    item->inner_key_data_offset = n->inner_key_data_start;
    char *data_ptr = n->get_inner_key_data_ptr(pos);
    assert(data_ptr < (char*)n + block_size);
    memcpy(data_ptr, key_data, inner_key_length);
}


void BTreeForwardIndex::split_node(
    const NodeRef &left,
    const NodeRef &right,
    int split_pos
) {
    Node *tmp = (Node*)malloc(block_size);
    *tmp = *left;
    tmp->num_items = 0;
    tmp->inner_key_data_start = block_size;

    for (int i = 0; i < split_pos; i++) {
        auto src = &(left->items[i]);
        add_item(
             tmp,
             tmp->num_items,
             src->key_idx,
             src->inner_key_length,
             left->get_inner_key_data_ptr(i),
             src->content_idx);
    }

    for (int i = split_pos; i < left->num_items; i++) {
        auto src = &(left->items[i]);
        add_item(
             right.get(),
             right->num_items,
             src->key_idx,
             src->inner_key_length,
             left->get_inner_key_data_ptr(i),
             src->content_idx);
    }

    right->next_idx = left->next_idx;
    tmp->next_idx = right.index;

    memcpy(left.get(), tmp, block_size);
    free(tmp);
}


void BTreeForwardIndex::insert_rec(
    forward_list<pair<NodeRef, int>> path,
    StringIndex key_idx,
    uint16_t inner_key_length,
    const char *key_data,
    int content_idx,
    int insert_at
) {

    auto n = path.front().first;
    path.pop_front();

    if (n->has_space(inner_key_length)) {
        add_item(n.get(), insert_at, key_idx, inner_key_length, key_data, content_idx);
        return;
    } else {
        // grow tree

        // add a new node, split items between both
        auto new_sibling = initialize_new_node(n->type);
        // TODO: split by size
        auto low_half = n->num_items / 2;
        if (insert_at <= low_half) {
            split_node(n, new_sibling, low_half);
            // insert left
            add_item(
                n.get(),
                insert_at,
                key_idx,
                inner_key_length,
                key_data,
                content_idx);
        } else {
            split_node(n, new_sibling, low_half + 1);
            // insert right
            add_item(
                new_sibling.get(),
                insert_at - low_half - 1,
                key_idx,
                inner_key_length,
                key_data,
                content_idx);
        }
        assert(abs((int64_t)n->num_items - (int64_t)new_sibling->num_items) <= 1);

        // insert the new leaf into parent (recurse if necessary)
        if (!path.empty()) {
            auto parent = path.front().first;
            auto idx_in_parent = path.front().second;

            string tmp_key_data = new_sibling->copy_inner_key_data(0);

            insert_rec(
                path,
                new_sibling->items[0].key_idx,
                tmp_key_data.length(),
                tmp_key_data.c_str(),
                new_sibling.index,
                idx_in_parent + 1);
        } else {
            auto new_root = initialize_new_node(Node::NODE);

            string tmp_key_data0 = n->copy_inner_key_data(0);
            string tmp_key_data1 = new_sibling->copy_inner_key_data(0);

            add_item(
                new_root.get(),
                0,
                n->items[0].key_idx,
                tmp_key_data0.length(),
                tmp_key_data0.c_str(),
                n.index);
            add_item(
                new_root.get(),
                1,
                new_sibling->items[0].key_idx,
                tmp_key_data1.length(),
                tmp_key_data1.c_str(),
                new_sibling.index);
            root_node = new_root.index;
        }
    }
}

KeyCmp BTreeForwardIndex::compare_keys(
    const NodeRef &nr,
    int item_idx,
    size_t key_len,
    const char *key
) const {
    NodeItem *item = &nr->items[item_idx];
    int inner_cmp = strncmp(
        key,
        nr->get_inner_key_data_ptr(item_idx),
        item->inner_key_length);

    int cmp;

    if (item->inner_key_length == max_inner_key_length) {
        if (inner_cmp == 0) {
            cmp = strcmp(
                key,
                sp.get(item->key_idx));
        } else {
            cmp = inner_cmp;
        }
    } else {
        if ((inner_cmp == 0) && (key_len > item->inner_key_length))
            return KEY_ABOVE_ITEM;
        cmp = inner_cmp;
    }

    if (cmp < 0) return KEY_BELOW_ITEM;
    else if (cmp == 0) return KEY_MATCHES_ITEM;
    else return KEY_ABOVE_ITEM;
}

void BTreeForwardIndex::insert(shared_ptr<Entity> e) {
    const char * key_data = e->id.c_str();
    uint16_t inner_key_length = min(e->id.length(), (size_t)max_inner_key_length);

    auto id_idx = sp.append(key_data);
    auto content_idx = sp.append(e->content.c_str());

    TreeCursor ki(this);

    // find leaf to insert to
    ki.navigateToLeaf(key_data);

    insert_rec(
        ki.path(),
        id_idx,
        inner_key_length,
        key_data,
        content_idx,
        find_insert_pos(ki.node(), id_idx, inner_key_length, key_data));

    item_count++;
}


shared_ptr<IForwardIndex> open_forward_index() {
    return make_shared<BTreeForwardIndex>("data/btree", 4096, 128);
}
