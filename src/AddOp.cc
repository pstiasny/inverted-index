#include "inverted_index.h"


void AddOp::operate(IInvertedIndex &ii, IForwardIndex &fi) {
    fi.insert(e);
    addEntityToInvertedIndex(ii, e);
}

void AddOp::addEntityToInvertedIndex(IInvertedIndex &ii, const shared_ptr<Entity> &e) {
    istringstream iss(e->content);
    vector<string> tokens{istream_iterator<string>{iss},
                          istream_iterator<string>{}};
    for (auto it = begin(tokens); it != end(tokens); ++it) {
        ii.insert(*it, e);
    }
}
