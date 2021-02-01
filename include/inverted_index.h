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

    bool operator<(const Entity &r);
    bool operator==(const Entity &r);
};

ostream& operator<<(ostream &os, Entity const &e);
bool compare_entity_ptrs_by_id(const shared_ptr<Entity> &x, const shared_ptr<Entity> &y);

class PostingList {
public:
    PostingList() {};
    PostingList(const forward_list<string> &lst) : lst(lst) {};

    void add(shared_ptr<Entity> e);
    PostingList intersect(PostingList other);

    bool operator==(const PostingList &r) const;

    forward_list<string> lst;
};

ostream& operator<<(ostream &os, PostingList const &pl);


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
    virtual PostingList query(const Term &q) = 0;
    virtual PostingList query(const And &q) = 0;
    virtual PostingList query(const Query &q) = 0;
};

class Query {
public:
    virtual PostingList visit(IDB &db) const = 0;
};

class Term : public Query {
    public:
    string t;
    Term(const string &t) : t(t) {}
    PostingList visit(IDB &db) const override {
        return db.query(*this);
    }
};

class And : public Query {
    public:
    shared_ptr<Query> l;
    shared_ptr<Query> r;
    And(shared_ptr<Query> l, shared_ptr<Query> r) : l(l), r(r) {}
    PostingList visit(IDB &db) const override {
        return db.query(*this);
    }
};


class IForwardIndex {
    public:
    virtual shared_ptr<Entity> get(const string &id) = 0;
    virtual void insert(shared_ptr<Entity> e) = 0;
};

shared_ptr<IForwardIndex> open_forward_index();


class IInvertedIndex {
    public:
    virtual PostingList get(const string &word) = 0;
    virtual void insert(const string &word, shared_ptr<Entity> e) = 0;
};

shared_ptr<IInvertedIndex> open_inverted_index();


struct AddOp {
    int seqid;
    shared_ptr<Entity> e;

    void operate(IInvertedIndex &ii, IForwardIndex &fi);

    private:
    void addEntityToInvertedIndex(IInvertedIndex &ii, const shared_ptr<Entity> &e);
};

class Log {
    public:
    Log(string path);
    ~Log();

    void writeOp(const AddOp &o);
    shared_ptr<AddOp> readOp();
    void close();
    int getNextSeqid();

    private:
    int last_seqid = 0;
    FILE *file;

    void lread(void *ptr, size_t size, size_t n);
    bool ltry_read(void *ptr, size_t size, size_t n);
    void lwrite(const void *ptr, size_t size, size_t nitems);
    void lflush();
};

class EntityExists : public exception {};

class DB : public IDB {
    public:
    DB(string log_path);
    ~DB();

    PostingList query(const Term &q) override;
    PostingList query(const And &q) override;
    PostingList query(const Query &q) override;
    shared_ptr<Entity> get(const string &id);
    void add(const shared_ptr<Entity> &e);
    void close();

    private:
    shared_ptr<IInvertedIndex> inverted_index;
    shared_ptr<IForwardIndex> forward_index;
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
    shared_ptr<InputExpr> parse(string input);

    private:
    shared_ptr<InputExpr> parse_tokens(deque<string> &tokens);
    deque<string> tokenize(string input);
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
    Interpreter(DB &db, ostream &out);

    virtual void interpret(const InputExpr &e);
    virtual void interpret(const List &e) { };
    virtual void interpret(const Symbol &e) { };
    virtual void interpret(const String &e) { };

    private:
    void exit(const vector<shared_ptr<InputExpr>> &args);
    void add(const vector<shared_ptr<InputExpr>> &args);
    void query(const vector<shared_ptr<InputExpr>> &args);
    void get(const vector<shared_ptr<InputExpr>> &args);

    vector<shared_ptr<InputExpr>> getList(const InputExpr& e);
    string getSymbol(const InputExpr& e);
    string getString(const InputExpr& e);

    DB &db;
    ostream &iout;
};
