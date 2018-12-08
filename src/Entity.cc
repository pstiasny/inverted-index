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

bool compare_entity_ptrs_by_id(const shared_ptr<Entity> &x, const shared_ptr<Entity> &y) {
    return x->id < y->id;
};
