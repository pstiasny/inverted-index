#include "inverted_index.h"


void PostingList::add(shared_ptr<Entity> e) {
    // The insert algorithm is obviously terribly inefficient.

    forward_list<string> ins = {e->id};
    // Note that we need a comparison function to compare on entity IDs, not shared_pointers.
    lst.merge(ins);
    lst.unique();
}


PostingList PostingList::intersect(PostingList other) {
    auto &l = lst, &r = other.lst;
    PostingList result;

    auto lb = l.begin(), le = l.end(), rb = r.begin(), re = r.end();
    while (lb!=le && rb!=re)
    {
        auto &lid = *lb, &rid = *rb;
        if (lid<rid) ++lb;
        else if (rid<lid) ++rb;
        else {
            result.lst.push_front(*lb);
            ++lb;
            ++rb;
        }
    }

    result.lst.reverse();
    return result;
}

bool PostingList::operator==(const PostingList &r) const {
    return lst == r.lst;
}

ostream& operator<<(ostream &os, PostingList const &pl) {
    os << "[";
    for (const auto &id : pl.lst)
        os << id << ", ";
    os << "]";
    return os;
}
