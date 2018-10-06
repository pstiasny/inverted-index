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

class Error : public exception {
    public:
    Error(string message) : message(message) {};
    virtual const char* what() const throw()
    {
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

class EntityExists : public exception {};

class DB : public IDB {
    public:
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

    void add(shared_ptr<Entity> &e) {
        if (forward_index[e->id])
            throw EntityExists();

        istringstream iss(e->content);
        vector<string> tokens{istream_iterator<string>{iss},
                              istream_iterator<string>{}};
        for (auto it = begin(tokens); it != end(tokens); ++it) {
            forward_list<shared_ptr<Entity>> ins = {e};
            auto &bucket = inverted_index[*it];
            bucket.merge(ins);
            bucket.unique();
        }

        forward_index[e->id] = e;
    }

    private:
    unordered_map<string, forward_list<shared_ptr<Entity>>> inverted_index;
    unordered_map<string, shared_ptr<Entity>> forward_index;
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
    Interpreter(DB &db) : db(db) {}

    virtual void interpret(const InputExpr &e) { 
        auto root = getList(e);
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
            cout << e->id << endl;

    }

    void get(const vector<shared_ptr<InputExpr>> &args) {
        auto eptr = db.get(getString(*args[1]));
        if (eptr)
            cout << eptr->content << endl;
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
};


int main(int argc, char** argv) {
    DB db;
    Parser p;
    Interpreter i(db);

    string input;
    while (cin.good()) {
        cout << "ii> ";
        getline(cin, input);
        if (input.empty())
            continue;
        //Printer pr;
        //pr.interpret(*p.parse(input));
        try {
            i.interpret(*p.parse(input));
        } catch (ParseError &e) {
            cout << "PARSE ERROR " << e.what() << endl;
        } catch (CommandError &e) {
            cout << "COMMAND ERROR " << e.what() << endl;
        }
    }

    return 0;
}
