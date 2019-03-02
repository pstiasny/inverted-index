#include "inverted_index.h"


Interpreter::Interpreter(DB &db, ostream &out) : db(db), iout(out) {}

void Interpreter::interpret(const InputExpr &e) {
    auto root = getList(e);
    if (root.empty())
        throw CommandError("Expected a command, got empty list");

    const auto &command = getSymbol(*root.front());
    const int arity = root.size() - 1;
    if (command == "exit" && arity == 0) {
        exit(root);
    } else if (command == "add" && arity == 2) {
        add(root);
    } else if (command == "query" && arity >= 1) {
        query(root);
    } else if (command == "get" && arity == 1) {
        get(root);
    } else {
        throw CommandError("Unknown command: " + command + "(" + to_string(arity) + ")");
    }
};

void Interpreter::exit(const vector<shared_ptr<InputExpr>> &args) {
    db.close();
    ::exit(0);
}

void Interpreter::add(const vector<shared_ptr<InputExpr>> &args) {
    auto id = getString(*args[1]),
         content = getString(*args[2]);
    auto entity = make_shared<Entity>(id, content);
    try {
        db.add(entity);
    } catch (EntityExists &e) {
        throw CommandError("Entity exists");
    }
}

void Interpreter::query(const vector<shared_ptr<InputExpr>> &args) {
    auto i = args.begin() + 1;
    shared_ptr<Query> q = make_shared<Term>(getString(**i));
    while (++i != args.end()) {
        auto term = make_shared<Term>(getString(**i));
        q = make_shared<And>(term, q);
    }

    auto r = db.query(*q);
    for (auto &id: r.lst)
        iout << id << endl;
}

void Interpreter::get(const vector<shared_ptr<InputExpr>> &args) {
    auto eptr = db.get(getString(*args[1]));
    if (eptr)
        iout << eptr->content << endl;
}

vector<shared_ptr<InputExpr>> Interpreter::getList(const InputExpr& e) {
    try {
        return dynamic_cast<const List&>(e).items;
    } catch (bad_cast) {
        throw CommandError("Expected a list");
    }
}

string Interpreter::getSymbol(const InputExpr& e) {
    try {
        return dynamic_cast<const Symbol&>(e).label;
    } catch (bad_cast) {
        throw CommandError("Expected a symbol");
    }
}

string Interpreter::getString(const InputExpr& e) {
    try {
        return dynamic_cast<const String&>(e).str;
    } catch (bad_cast) {
        throw CommandError("Expected a string");
    }
}
