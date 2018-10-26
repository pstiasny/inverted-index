#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <forward_list>
#include <deque>
#include <fstream>
#include <stdio.h>

using namespace std;


class Entity {
public:
    string id;
    string content;

    Entity(const string &id, const string &content)
        : id(id), content(content)
    {}

    bool operator<(const Entity &r) {
        return id < r.id;
    }

    bool operator==(const Entity &r) {
        return id == r.id;
    }
};

ostream& operator<<(ostream &os, Entity const &e) {
    return os << "<Entity \"" << e.id << "\">";
}

class Error : public exception {
    public:
    Error(string message) : message(message) {};
    virtual const char* what() const throw() {
        return message.c_str();
    }

    private:
    string message;
};

class CommandError : public Error {
    using Error::Error;
};
class ParseError : public Error {
    using Error::Error;
};
class IOError : public Error {
    using Error::Error;
};

class Query;
class Term;
class And;
class IDB {
    public:
    virtual forward_list<shared_ptr<Entity>> query(const Term &q) = 0;
    virtual forward_list<shared_ptr<Entity>> query(const And &q) = 0;
    virtual forward_list<shared_ptr<Entity>> query(const Query &q) = 0;
};

class Query {
public:
    virtual forward_list<shared_ptr<Entity>> visit(IDB &db) const = 0;
};

class Term : public Query {
    public:
    string t;
    Term(const string &t) : t(t) {}
    forward_list<shared_ptr<Entity>> visit(IDB &db) const override {
        return db.query(*this);
    }
};

class And : public Query {
    public:
    shared_ptr<Query> l;
    shared_ptr<Query> r;
    And(shared_ptr<Query> l, shared_ptr<Query> r) : l(l), r(r) {}
    forward_list<shared_ptr<Entity>> visit(IDB &db) const override {
        return db.query(*this);
    }
};

typedef unordered_map<string, forward_list<shared_ptr<Entity>>> InvertedIndex;
typedef unordered_map<string, shared_ptr<Entity>> ForwardIndex;

struct AddOp {
    int seqid;
    shared_ptr<Entity> e;

    void operate(InvertedIndex &ii, ForwardIndex &fi) {
        addEntityToForwardIndex(fi, e);
        addEntityToInvertedIndex(ii, e);
    }

    private:
    void addEntityToInvertedIndex(InvertedIndex &ii, const shared_ptr<Entity> &e) {
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

    void addEntityToForwardIndex(ForwardIndex &fi, const shared_ptr<Entity> &e) {
        fi[e->id] = e;
    }
};

class Log {
    public:
    Log(string path) {
        file = fopen(path.c_str(), "a+");
        if (!file)
            throw IOError(string("could not open log: ") + strerror(errno));
        if (0 != fseek(file, 0, SEEK_SET))
            throw IOError(string("could not seek in log: ") + strerror(errno));
    }

    ~Log() {
        close();
    }

    void writeOp(const AddOp &o) {
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

    shared_ptr<AddOp> readOp() {
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

    void close() {
        fclose(file);
    }

    int getNextSeqid() {
        return last_seqid + 1;
    }

    private:
    int last_seqid = 0;
    FILE *file;

    void lread(void *ptr, size_t size, size_t n) {
        if (n != fread(ptr, size, n, file)) {
            if (feof(file)) {
                throw IOError("unexepected end of log");
            } else {
                throw IOError("error when reading log");
            }
        }
    }

    bool ltry_read(void *ptr, size_t size, size_t n) {
        if (n != fread(ptr, size, n, file)) {
            if (feof(file)) {
                return false;
            } else {
                throw IOError("error when reading log");
            }
        }
        return true;
    }

    void lwrite(const void *ptr, size_t size, size_t nitems) {
        if (nitems != fwrite(ptr, size, nitems, file))
            throw IOError("could not write to log");
    }

    void lflush() {
        fflush(file);
    }
};

class EntityExists : public exception {};

class DB : public IDB {
    public:
    DB(string log_path) : log(log_path) {
        shared_ptr<AddOp> op;
        while ((op = log.readOp())) {
            op->operate(inverted_index, forward_index);
        }
    }

    ~DB() {
        close();
    }

    forward_list<shared_ptr<Entity>> query(const Term &q) override {
        return inverted_index[q.t];
    }

    forward_list<shared_ptr<Entity>> query(const And &q) override {
        auto l = this->query(*q.l),
             r = this->query(*q.r);

        forward_list<shared_ptr<Entity>> result;
        auto lb = l.begin(), le = l.end(), rb = r.begin(), re = r.end();
        while (lb!=le && rb!=re)
        {
            if (*lb<*rb) ++lb;
            else if (*rb<*lb) ++rb;
            else {
                auto &both = *lb;
                result.push_front(both);
                while (lb != le && *lb == both) ++lb;
                while (rb != re && *rb == both) ++rb;
            }
        }
        result.reverse();
        return result;
    }

    forward_list<shared_ptr<Entity>> query(const Query &q) override {
        return q.visit(*this);
    }

    shared_ptr<Entity> get(const string &id) {
        return forward_index[id];
    }

    void add(const shared_ptr<Entity> &e) {
        if (forward_index[e->id])
            throw EntityExists();

        AddOp add_op {log.getNextSeqid(), e};
        log.writeOp(add_op);
        add_op.operate(inverted_index, forward_index);
    }

    void close() {
        log.close();
    }

    private:
    InvertedIndex inverted_index;
    ForwardIndex forward_index;
    Log log;
};


class InputExpr;
class Symbol;
class String;
class List;
class IInterpreter {
    public:
    virtual void interpret(const InputExpr &e) = 0;
    virtual void interpret(const Symbol &e) = 0;
    virtual void interpret(const String &e) = 0;
    virtual void interpret(const List &e) = 0;
};

class InputExpr {
    public:
    virtual void visit(IInterpreter &i) const = 0;
};
class Symbol : public InputExpr {
    public:
    Symbol(string label) : label(label) {};
    virtual void visit(IInterpreter &i) const override { i.interpret(*this); };

    string label;
};
class String : public InputExpr {
    public:
    String(string str) : str(str) {};
    virtual void visit(IInterpreter &i) const override { i.interpret(*this); };

    string str;
};
class List : public InputExpr {
    public:
    List(vector<shared_ptr<InputExpr>> items) : items(items) {};
    virtual void visit(IInterpreter &i) const override { i.interpret(*this); };

    vector<shared_ptr<InputExpr>> items;
};

class Parser {
    public:
    shared_ptr<InputExpr> parse(string input) {
        auto tokens = tokenize(input);
        return parse_tokens(tokens);
    };

    private:
    shared_ptr<InputExpr> parse_tokens(deque<string> &tokens) {
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

    deque<string> tokenize(string input) {
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
};

class Printer : public IInterpreter {
    public:
    virtual void interpret(const InputExpr &e) { 
        e.visit(*this);
    };
    virtual void interpret(const List &e) {
        cout << "BEGIN LIST" << endl;
        for (const auto& item : e.items)
            interpret(*item);
        cout << "END LIST" << endl;
    };
    virtual void interpret(const Symbol &e) {
        cout << "SYMBOL " << e.label << endl;
    };
    virtual void interpret(const String &e) {
        cout << "STRING " << e.str << endl;
    };
};

class Interpreter : public IInterpreter {
    public:
    Interpreter(DB &db, ostream &out) : db(db), iout(out) {}

    virtual void interpret(const InputExpr &e) {
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
    virtual void interpret(const List &e) { };
    virtual void interpret(const Symbol &e) { };
    virtual void interpret(const String &e) { };

    private:
    void exit(const vector<shared_ptr<InputExpr>> &args) {
        db.close();
        ::exit(0);
    }

    void add(const vector<shared_ptr<InputExpr>> &args) {
        auto id = getString(*args[1]),
             content = getString(*args[2]);
        auto entity = make_shared<Entity>(id, content);
        try {
            db.add(entity);
        } catch (EntityExists &e) {
            throw CommandError("Entity exists");
        }
    }

    void query(const vector<shared_ptr<InputExpr>> &args) {
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

    void get(const vector<shared_ptr<InputExpr>> &args) {
        auto eptr = db.get(getString(*args[1]));
        if (eptr)
            iout << eptr->content << endl;
    }

    vector<shared_ptr<InputExpr>> getList(const InputExpr& e) {
        try {
            return dynamic_cast<const List&>(e).items;
        } catch (bad_cast) {
            throw CommandError("Expected a list");
        }
    }

    string getSymbol(const InputExpr& e) {
        try {
            return dynamic_cast<const Symbol&>(e).label;
        } catch (bad_cast) {
            throw CommandError("Expected a symbol");
        }
    }

    string getString(const InputExpr& e) {
        try {
            return dynamic_cast<const String&>(e).str;
        } catch (bad_cast) {
            throw CommandError("Expected a string");
        }
    }

    DB &db;
    ostream &iout;
};
