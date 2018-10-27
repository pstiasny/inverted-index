#include "inverted_index.h"


void AddOp::operate(InvertedIndex &ii, ForwardIndex &fi) {
    addEntityToForwardIndex(fi, e);
    addEntityToInvertedIndex(ii, e);
}

void AddOp::addEntityToInvertedIndex(InvertedIndex &ii, const shared_ptr<Entity> &e) {
    istringstream iss(e->content);
    vector<string> tokens{istream_iterator<string>{iss},
                          istream_iterator<string>{}};
    for (auto it = begin(tokens); it != end(tokens); ++it) {
        auto &bucket = ii[*it];
        bucket.add(e);
    }
}

void AddOp::addEntityToForwardIndex(ForwardIndex &fi, const shared_ptr<Entity> &e) {
    fi[e->id] = e;
}
