#pragma once

#include <functional>

namespace modplug {
namespace pervasives {

//ghetto bijective association arrays

template <typename assoc_t, typename key_t>
int idx_of_key(assoc_t assocs[], key_t key) {
    for (size_t i = 0; i < sizeof(assocs); ++i) {
        if (assocs[i].key == key) {
            return (int) i;
        }
    }
    return -1;
}

template <typename assoc_t, typename val_t>
int idx_of_value(assoc_t assocs[], val_t value, int (*cmp) (val_t, val_t)) {
    for (size_t i = 0; i < sizeof(assocs); ++i) {
        if (cmp(assocs[i].value, value) == 0) {
            return (int) i;
        }
    }
    return -1;
}

template <typename assoc_t, typename val_t, typename key_t>
key_t key_of_value(assoc_t assocs[], val_t value,
                   key_t def, int (*cmp) (val_t, val_t))
{
    int idx = idx_of_value(assocs, value, cmp);
    return idx < 0 ? def : assocs[idx].key;
}

template <typename assoc_t, typename key_t, typename val_t>
val_t value_of_key(assoc_t assocs[], key_t key, val_t def) {
    int idx = idx_of_key(assocs, key);
    return idx < 0 ? def : assocs[idx].value;
}

}
}