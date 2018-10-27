#include "inverted_index.h"


bool Entity::operator<(const Entity &r) {
    return id < r.id;
}

bool Entity::operator==(const Entity &r) {
    return id == r.id;
}

ostream& operator<<(ostream &os, Entity const &e) {
    return os << "<Entity \"" << e.id << "\">";
}

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

Log::Log(string path) {
        file = fopen(path.c_str(), "a+");
        if (!file)
            throw IOError(string("could not open log: ") + strerror(errno));
        if (0 != fseek(file, 0, SEEK_SET))
            throw IOError(string("could not seek in log: ") + strerror(errno));
    }

Log::~Log() {
        close();
    }

void Log::writeOp(const AddOp &o) {
    assert(o.seqid > last_seqid);

    if (0 != fseek(file, 0, SEEK_END))
        throw IOError(string("could not seek in log: ") + strerror(errno));

    int id_size = o.e->id.size()
      , content_size =o.e->content.size();
    lwrite(&o.seqid, sizeof(o.seqid), 1);
    lwrite(&id_size, sizeof(id_size), 1);
    lwrite(&content_size, sizeof(content_size), 1);
    lwrite(o.e->id.c_str(), o.e->id.size(), 1);
    lwrite(o.e->content.c_str(), o.e->content.size(), 1);
    lflush();

    last_seqid = o.seqid;
}

shared_ptr<AddOp> Log::readOp() {
    auto op = make_shared<AddOp>();

    int r;
    int seqid, id_size, content_size;
    if (!ltry_read(&(op->seqid), sizeof(seqid), 1))
        return nullptr;
    lread(&id_size, sizeof(id_size), 1);
    lread(&content_size, sizeof(content_size), 1);

    assert(op->seqid > last_seqid);
    last_seqid = op->seqid;

    char id[id_size], content[content_size];
    lread(id, sizeof(char), id_size);
    lread(content, sizeof(char), content_size);
    op->e = make_shared<Entity>(string(id, id_size), string(content, content_size));

    return op;
}

void Log::close() {
    fclose(file);
}

int Log::getNextSeqid() {
    return last_seqid + 1;
}

void Log::lread(void *ptr, size_t size, size_t n) {
    if (n != fread(ptr, size, n, file)) {
        if (feof(file)) {
            throw IOError("unexepected end of log");
        } else {
            throw IOError("error when reading log");
        }
    }
}

bool Log::ltry_read(void *ptr, size_t size, size_t n) {
    if (n != fread(ptr, size, n, file)) {
        if (feof(file)) {
            return false;
        } else {
            throw IOError("error when reading log");
        }
    }
    return true;
}

void Log::lwrite(const void *ptr, size_t size, size_t nitems) {
    if (nitems != fwrite(ptr, size, nitems, file))
        throw IOError("could not write to log");
}

void Log::lflush() {
    fflush(file);
}


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


shared_ptr<InputExpr> Parser::parse(string input) {
    auto tokens = tokenize(input);
    return parse_tokens(tokens);
};

shared_ptr<InputExpr> Parser::parse_tokens(deque<string> &tokens) {
    if (tokens.empty())
        throw ParseError("Premature end of intput");
    auto tok = tokens.front();
    tokens.pop_front();

    if (tok == "(") {
        vector<shared_ptr<InputExpr>> items;
        while (tokens.front() != ")")
            items.push_back(parse_tokens(tokens));
        tokens.pop_front();

        return make_shared<List>(items);
    } else if (tok.front() == '"' && tok.back() == '"') {
        return make_shared<String>(tok.substr(1, tok.size() - 2));
    } else {
        return make_shared<Symbol>(tok);
    }
}

deque<string> Parser::tokenize(string input) {
    deque<string> tokens = {};
    string buf;
    auto is_str = false;

    for (const char c : input) {
        if (is_str) {
            buf += c;
            if (c == '"') {
                is_str = false;
                tokens.push_back(buf);
                buf = "";
            }
        } else {
            if (c == '(' || c == ')' || c == ' ') {
                if (!buf.empty())
                    tokens.push_back(buf);
                buf = "";
            }

            if (c == '(')
                tokens.push_back("(");
            else if (c == ')')
                tokens.push_back(")");
            else if (c == '"') {
                buf += c;
                is_str = true;
            } else if (c != ' ')
                buf += c;
        }
    }
    if (!buf.empty())
        tokens.push_back(buf);
    return tokens;
}

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
    for (auto &e: r)
        iout << e->id << endl;
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
