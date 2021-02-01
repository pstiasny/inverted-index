#include <assert.h>
#include <unistd.h>


#include "inverted_index.h"


typedef uint32_t StringIndex;

struct ForwardIndexNode {
    enum {NODE, LEAF} type;  // byte size of enum?
    uint32_t num_items;
    uint32_t update_seqid;

    uint32_t next_idx;

    struct {
        StringIndex id_idx;
        uint32_t content_idx;
    } items[];

    void initialize() {
        type = ForwardIndexNode::LEAF;
        num_items = 0;
        next_idx = -1;
    }

    int get_id_idx_at(int idx) {
        return items[idx].id_idx;
    }

    int find(StringIndex id_idx) {
        for (int i = 0; i < num_items; i++)
            if (items[i].id_idx == id_idx)
                return i;
        assert(false);
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
    ForwardIndexNode * operator->() const;
    ForwardIndexNode& operator*() const;
};


class BTreeForwardIndex : public IForwardIndex {
    public:
    BTreeForwardIndex(const string &path, uint32_t block_size);
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

    void print_as_dot(ostream &outstream);

    NodeRef get_node_at_index(uint32_t i);
    void allocate_nodes();
    NodeRef initialize_new_node(decltype(ForwardIndexNode::type) type);
    void insert_rec(forward_list<pair<NodeRef, int>> path,
                    StringIndex id_idx, int content_idx,
                    int insert_at);
    void split_node(const NodeRef &n1, const NodeRef &n2, int split_pos);
    void add_node_item_inorder(const NodeRef &n, StringIndex id_idx, int content_idx);
    int find_insert_pos(const NodeRef &n, StringIndex id_idx);
    bool good_idx(const NodeRef &n, int i) const;
    bool has_space(const NodeRef &n) const;
    void add_item(const NodeRef &nr, int pos, StringIndex id_idx, uint32_t content_idx);
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
    const forward_list<pair<NodeRef, int>> & path();

    void navigateToLeaf(const string &id);

    private:
    BTreeForwardIndex *tree;
    int node_idx = 0;
    int item_idx = 0;
    forward_list<pair<NodeRef, int>> _path;
};
