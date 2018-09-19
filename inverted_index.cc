#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <forward_list>

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

    void add(std::shared_ptr<Entity> &e) {
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
    }

    private:
    std::unordered_map<std::string, std::forward_list<std::shared_ptr<Entity>>> inverted_index;
};


int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "missing search term" << std::endl;
        return 1;
    }

    ++argv;
    std::shared_ptr<Query> q = std::make_shared<Term>(*argv);
    while (*++argv) {
        auto term = std::make_shared<Term>(*argv);
        q = std::make_shared<And>(term, q);
    }

    DB db;
    auto findThis1 = std::make_shared<Entity>("foo", "the quick brown fox jumps over the lazy dog");
    db.add(findThis1);
    auto findThis2 = std::make_shared<Entity>("bar", "the quick dog barks");
    db.add(findThis2);

    auto r = db.query(*q);
    for (auto &e: r)
        std::cout << e->id << std::endl;

    return 0;
}
