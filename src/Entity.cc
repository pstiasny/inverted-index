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
