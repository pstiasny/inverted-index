#include "inverted_index.h"


class InvertedIndex : public IInvertedIndex {
    public:
    InvertedIndex() : ii() {};
    virtual PostingList get(const string &word) override;
    virtual void insert(const string &word, shared_ptr<Entity> e) override;
    private:
    unordered_map<string, PostingList> ii;
};


PostingList InvertedIndex::get(const string &word) {
    return ii[word];
}

void InvertedIndex::insert(const string &word, shared_ptr<Entity> e) {
    auto &bucket = ii[word];
    bucket.add(e);
}


shared_ptr<IInvertedIndex> open_inverted_index() {
    return make_shared<InvertedIndex>();
}
