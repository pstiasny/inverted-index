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

class Entity {
public:
    std::string id;
    std::string content;

    Entity(const std::string &id, const std::string &content)
        : id(id), content(content)
    {}

    bool operator<(const Entity &r) {
        return id < r.id;
    }

    bool operator==(const Entity &r) {
        return id == r.id;
    }
};

class Query;
class Term;
class And;
class IDB {
    public:
    virtual std::forward_list<std::shared_ptr<Entity>> query(const Term &q) = 0;
    virtual std::forward_list<std::shared_ptr<Entity>> query(const And &q) = 0;
    virtual std::forward_list<std::shared_ptr<Entity>> query(const Query &q) = 0;
};

class Query {
public:
    virtual std::forward_list<std::shared_ptr<Entity>> visit(IDB &db) const = 0;
};

class Term : public Query {
    public:
    std::string t;
    Term(const std::string &t) : t(t) {}
    std::forward_list<std::shared_ptr<Entity>> visit(IDB &db) const override {
        return db.query(*this);
    }
};

class And : public Query {
    public:
    std::shared_ptr<Query> l;
    std::shared_ptr<Query> r;
    And(std::shared_ptr<Query> l, std::shared_ptr<Query> r) : l(l), r(r) {}
    std::forward_list<std::shared_ptr<Entity>> visit(IDB &db) const override {
        return db.query(*this);
    }
};

class EntityExists : public std::exception {};

class DB : public IDB {
    public:
    std::forward_list<std::shared_ptr<Entity>> query(const Term &q) override {
        return inverted_index[q.t];
    }

    std::forward_list<std::shared_ptr<Entity>> query(const And &q) override {
        auto l = this->query(*q.l),
             r = this->query(*q.r);

        std::forward_list<std::shared_ptr<Entity>> result;
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

    std::forward_list<std::shared_ptr<Entity>> query(const Query &q) override {
        return q.visit(*this);
    }

    std::shared_ptr<Entity> get(const std::string &id) {
        return forward_index[id];
    }

    void add(std::shared_ptr<Entity> &e) {
        if (forward_index[e->id])
            throw EntityExists();

        using namespace std;
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
    std::unordered_map<std::string, std::forward_list<std::shared_ptr<Entity>>> inverted_index;
    std::unordered_map<std::string, std::shared_ptr<Entity>> forward_index;
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
    Symbol(std::string label) : label(label) {};
    virtual void visit(IInterpreter &i) const override { i.interpret(*this); };

    std::string label;
};
class String : public InputExpr {
    public:
    String(std::string str) : str(str) {};
    virtual void visit(IInterpreter &i) const override { i.interpret(*this); };

    std::string str;
};
class List : public InputExpr {
    public:
    List(std::vector<std::shared_ptr<InputExpr>> items) : items(items) {};
    virtual void visit(IInterpreter &i) const override { i.interpret(*this); };

    std::vector<std::shared_ptr<InputExpr>> items;
};

class Parser {
    public:
    std::shared_ptr<InputExpr> parse(std::string input) {
        auto tokens = tokenize(input);
        return parse_tokens(tokens);
    };

    private:
    std::shared_ptr<InputExpr> parse_tokens(std::deque<std::string> &tokens) {
        if (tokens.empty()) {
            std::cout << "PREMEATURE END OF INPUT" << std::endl;
            return std::make_shared<Symbol>("#eof");
        }
        auto tok = tokens.front();
        tokens.pop_front();

        if (tok == "(") {
            std::vector<std::shared_ptr<InputExpr>> items;
            while (tokens.front() != ")")
                items.push_back(parse_tokens(tokens));
            tokens.pop_front();

            return std::make_shared<List>(items);
        } else if (tok.front() == '"' && tok.back() == '"') {
            return std::make_shared<String>(tok.substr(1, tok.size() - 2));
        } else {
            return std::make_shared<Symbol>(tok);
        }
        // TODO: stray )
    }

    std::deque<std::string> tokenize(std::string input) {
        std::deque<std::string> tokens = {};
        std::string buf;
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

class CommandError : public std::exception {
    public:
    CommandError(std::string message) : message(message) {};
    virtual const char* what() const throw()
    {
        return message.c_str();
    }

    private:
    std::string message;
};

class Printer : public IInterpreter {
    public:
    virtual void interpret(const InputExpr &e) { 
        e.visit(*this);
    };
    virtual void interpret(const List &e) {
        std::cout << "BEGIN LIST" << std::endl;
        for (const auto& item : e.items)
            interpret(*item);
        std::cout << "END LIST" << std::endl;
    };
    virtual void interpret(const Symbol &e) {
        std::cout << "SYMBOL " << e.label << std::endl;
    };
    virtual void interpret(const String &e) {
        std::cout << "STRING " << e.str << std::endl;
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
            std::cout << "UNKNOWN COMMAND " << command << "(" << arity << ")" << std::endl;
        }
    };
    virtual void interpret(const List &e) { };
    virtual void interpret(const Symbol &e) { };
    virtual void interpret(const String &e) { };

    private:
    void exit(const std::vector<std::shared_ptr<InputExpr>> &args) {
        ::exit(0);
    }

    void add(const std::vector<std::shared_ptr<InputExpr>> &args) {
        auto id = getString(*args[1]),
             content = getString(*args[2]);
        auto entity = std::make_shared<Entity>(id, content);
        try {
            db.add(entity);
        } catch (EntityExists &e) {
            throw CommandError("Entity exists");
        }
    }

    void query(const std::vector<std::shared_ptr<InputExpr>> &args) {
        auto i = args.begin() + 1;
        std::shared_ptr<Query> q = std::make_shared<Term>(getString(**i));
        while (++i != args.end()) {
            auto term = std::make_shared<Term>(getString(**i));
            q = std::make_shared<And>(term, q);
        }
        
        auto r = db.query(*q);
        for (auto &e: r)
            std::cout << e->id << std::endl;

    }

    void get(const std::vector<std::shared_ptr<InputExpr>> &args) {
        auto eptr = db.get(getString(*args[1]));
        if (eptr)
            std::cout << eptr->content << std::endl;
    }

    std::vector<std::shared_ptr<InputExpr>> getList(const InputExpr& e) {
        try {
            return dynamic_cast<const List&>(e).items;
        } catch (std::bad_cast) {
            throw CommandError("Expected a list");
        }
    }

    std::string getSymbol(const InputExpr& e) {
        try {
            return dynamic_cast<const Symbol&>(e).label;
        } catch (std::bad_cast) {
            throw CommandError("Expected a symbol");
        }
    }

    std::string getString(const InputExpr& e) {
        try {
            return dynamic_cast<const String&>(e).str;
        } catch (std::bad_cast) {
            throw CommandError("Expected a string");
        }
    }

    DB &db;
};


int main(int argc, char** argv) {
    DB db;
    Parser p;
    Interpreter i(db);

    std::string input;
    while (std::cin.good()) {
        std::cout << "ii> ";
        std::getline(std::cin, input);
        if (input.empty())
            continue;
        //Printer pr;
        //pr.interpret(*p.parse(input));
        try {
            i.interpret(*p.parse(input));
        } catch (CommandError &e) {
            std::cout << "COMMAND ERROR " << e.what() << std::endl;
        }
    }

    return 0;
}
