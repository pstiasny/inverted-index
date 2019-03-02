#include "inverted_index.h"


DB::DB(string log_path) : log(log_path) {
    forward_index = open_forward_index();

    shared_ptr<AddOp> op;
    while ((op = log.readOp())) {
        op->operate(inverted_index, *forward_index);
    }
}

DB::~DB() {
    close();
}

PostingList DB::query(const Term &q) {
    return inverted_index[q.t];
}

PostingList DB::query(const And &q) {
    auto l = this->query(*q.l),
         r = this->query(*q.r);

    return l.intersect(r);
}

PostingList DB::query(const Query &q) {
    return q.visit(*this);
}

shared_ptr<Entity> DB::get(const string &id) {
    return forward_index->get(id);
}

void DB::add(const shared_ptr<Entity> &e) {
    if (forward_index->get(e->id))
        throw EntityExists();

    AddOp add_op {log.getNextSeqid(), e};
    log.writeOp(add_op);
    add_op.operate(inverted_index, *forward_index);
}

void DB::close() {
    log.close();
}

