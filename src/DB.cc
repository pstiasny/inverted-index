#include "inverted_index.h"


DB::DB(string log_path) : log(log_path) {
    shared_ptr<AddOp> op;
    while ((op = log.readOp())) {
        op->operate(inverted_index, forward_index);
    }
}

DB::~DB() {
    close();
}

forward_list<shared_ptr<Entity>> DB::query(const Term &q) {
    return inverted_index[q.t];
}

forward_list<shared_ptr<Entity>> DB::query(const And &q) {
    auto l = this->query(*q.l),
         r = this->query(*q.r);

    forward_list<shared_ptr<Entity>> result;
    auto lb = l.begin(), le = l.end(), rb = r.begin(), re = r.end();
    while (lb!=le && rb!=re)
    {
        auto &lid = (*lb)->id, &rid = (*rb)->id;
        if (lid<rid) ++lb;
        else if (rid<lid) ++rb;
        else {
            result.push_front(*lb);
            ++lb;
            ++rb;
        }
    }
    result.reverse();
    return result;
}

forward_list<shared_ptr<Entity>> DB::query(const Query &q) {
    return q.visit(*this);
}

shared_ptr<Entity> DB::get(const string &id) {
    return forward_index[id];
}

void DB::add(const shared_ptr<Entity> &e) {
    if (forward_index[e->id])
        throw EntityExists();

    AddOp add_op {log.getNextSeqid(), e};
    log.writeOp(add_op);
    add_op.operate(inverted_index, forward_index);
}

void DB::close() {
    log.close();
}

