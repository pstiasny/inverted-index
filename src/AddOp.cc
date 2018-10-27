#include "inverted_index.h"


void AddOp::operate(InvertedIndex &ii, ForwardIndex &fi) {
    addEntityToForwardIndex(fi, e);
    addEntityToInvertedIndex(ii, e);
}

void AddOp::addEntityToInvertedIndex(InvertedIndex &ii, const shared_ptr<Entity> &e) {
    // The insert algorithm is obviously terribly inefficient.
    istringstream iss(e->content);
    vector<string> tokens{istream_iterator<string>{iss},
                          istream_iterator<string>{}};
    for (auto it = begin(tokens); it != end(tokens); ++it) {
        forward_list<shared_ptr<Entity>> ins = {e};
        auto &bucket = ii[*it];
        // Note that we need a comparison function to compare on entity IDs, not shared_pointers.
        bucket.merge(
            ins,
            [](const shared_ptr<Entity> &x, const shared_ptr<Entity> &y) {
                return x->id < y->id;
            });
        bucket.unique();
    }
}

void AddOp::addEntityToForwardIndex(ForwardIndex &fi, const shared_ptr<Entity> &e) {
    fi[e->id] = e;
}
