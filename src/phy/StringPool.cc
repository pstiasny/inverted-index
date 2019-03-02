#include "phy.h"


StringPool::StringPool() {
    data = (char*)calloc(1, size);
}

StringPool::~StringPool() {
    free(data);
}

void StringPool::resize(int fit) {
    // doubling the size will be too much in many cases
    size = max(2 * size, fit);
    data = (char*)realloc(data, size);
    if (!data) throw Error("Could not resize string pool");
}

StringIndex StringPool::append(const char *s) {
    auto insert_at = free_idx;
    auto l = strlen(s) + 1;

    if (l >= size - free_idx) {
        resize(free_idx + l);
    }
    strcpy(&data[insert_at], s);
    free_idx += l;

    return insert_at;
}

const char * StringPool::get(StringIndex i) const {
    assert(i >= 0 && i < size);
    return &data[i];
}
