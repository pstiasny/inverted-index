#include <assert.h>
#include <unistd.h>


#include "inverted_index.h"


typedef uint32_t StringIndex;


struct NodeItem {
    StringIndex key_idx;
    uint32_t content_idx;
    uint16_t inner_key_length;
    uint16_t inner_key_data_offset;
};


struct Node {
    enum {NODE, LEAF} type;
    uint32_t update_seqid;
    uint32_t next_idx;
    uint16_t num_items;
    uint16_t inner_key_data_start;
    NodeItem items[];

    StringIndex get_id_idx_at(int idx) {
        assert(idx >= 0);
        assert(idx < num_items);
        return items[idx].key_idx;
    }

    char * get_inner_key_data_ptr(int idx) {
        assert(idx >= 0);
        assert(idx < num_items);
        char* data_ptr = (char*)this + items[idx].inner_key_data_offset;
        assert(data_ptr >= (char*)this + inner_key_data_start);
        assert(data_ptr >= (char*)(&items[num_items]));
        return data_ptr;
    }

    string copy_inner_key_data(int idx) {
        return string(get_inner_key_data_ptr(idx), items[idx].inner_key_length);
    }

    bool has_space(uint16_t inner_key_length) const {
        return (
            (char*)(items + num_items + 1) <
            ((char*)this + inner_key_data_start - inner_key_length)
        );
    }

    void initialize(uint16_t size) {
        type = Node::LEAF;
        num_items = 0;
        next_idx = -1;
        inner_key_data_start = size;
    }
};


class StringPool {
    public:
    StringPool();
    ~StringPool();

    StringIndex append(const char *s);
    const char * get(StringIndex i) const;

    private:
    int size = 1024;
    int free_idx = 0;
    char *data = 0;

    void resize(int fit);
};


class BTreeForwardIndex;


class NodeRef {
    private:
    BTreeForwardIndex *tree;

    public:
    int index;
    NodeRef(BTreeForwardIndex *tree, int index);
    Node * operator->() const;
    Node& operator*() const;
    Node * get() const;
};


class BTreeForwardIndex : public IForwardIndex {
    public:
    BTreeForwardIndex(const string &path, uint32_t block_size, uint16_t max_inner_key_length);
    ~BTreeForwardIndex();
    virtual shared_ptr<Entity> get(const string &id) override;
    virtual void insert(shared_ptr<Entity> e) override;  // seqid?

    StringPool sp;

    int node_count = 1;
    int item_count = 0;
    int last_used_node = 0;
    int root_node = 0;
    char *nodes;
    int fd;
    uint32_t block_size;
    uint16_t max_inner_key_length;

    void print_as_dot(ostream &outstream);

    NodeRef get_node_at_index(uint32_t i);
    void allocate_nodes();
    NodeRef initialize_new_node(decltype(Node::type) type);
    void insert_rec(
        forward_list<pair<NodeRef, int>> path,
        StringIndex key_idx,
        uint16_t inner_key_length,
        const char *key_data,
        int content_idx,
        int insert_at
    );
    void split_node(const NodeRef &n1, const NodeRef &n2, int split_pos);
    int find_insert_pos(
        const NodeRef &n, 
        StringIndex key_idx,
        uint16_t inner_key_length,
        const char *key_data
    );
    bool has_space(const Node *n, uint16_t item_size) const;
    void add_item(
        Node *n,
        int pos,
        StringIndex key_idx,
        uint16_t inner_key_length,
        const char *key_data,
        uint32_t content_idx
    );
    int compare_keys(const NodeRef &nr, int item_idx, size_t key_len, const char *key) const;
};


class TreeCursor;


class TreeVisitor {
    public:
    virtual void enterNode(TreeCursor &tc, NodeRef nr) {};
    virtual void enterLeaf(TreeCursor &tc, NodeRef nr) {};
    virtual void enterNodeItem(TreeCursor &tc, NodeRef nr, int item_idx, NodeItem *item) {};
    virtual void enterLeafItem(TreeCursor &tc, NodeRef nr, int item_idx, NodeItem *item) {};
    virtual void exitNode(TreeCursor &tc, NodeRef nr) {};
    virtual void exitLeaf(TreeCursor &tc, NodeRef nr) {};
};


class TreePrinter : public TreeVisitor {
    public:
    TreePrinter(ostream &out, bool print_inner_key_data = false);
    virtual void enterNode(TreeCursor &tc, NodeRef nr) override;
    virtual void enterLeaf(TreeCursor &tc, NodeRef nr) override;
    virtual void enterNodeItem(TreeCursor &tc, NodeRef nr, int item_idx, NodeItem *item) override;
    virtual void enterLeafItem(TreeCursor &tc, NodeRef nr, int item_idx, NodeItem *item) override;
    virtual void exitNode(TreeCursor &tc, NodeRef nr) override;
    virtual void exitLeaf(TreeCursor &tc, NodeRef nr) override;

    private:
    ostream &out;
    string prefix;
    bool print_inner_key_data;
};


class TreeCursor {
    public:
    TreeCursor(BTreeForwardIndex *tree);
    void next();
    void down();
    void up();
    bool top();
    bool last();
    bool lastInNode();
    bool nodeHasNext();
    bool leaf();
    const char * key();
    const char * nextKey();
    const char * content();
    NodeRef node();
    int getItemIdx() { return item_idx; }
    const forward_list<pair<NodeRef, int>> & path();

    void navigateToLeaf(const char *key);
    void walk(TreeVisitor &tv);

    private:
    BTreeForwardIndex *tree;
    int node_idx = 0;
    int item_idx = 0;
    forward_list<pair<NodeRef, int>> _path;
};
