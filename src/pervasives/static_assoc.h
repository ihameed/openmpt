#pragma once

#include <functional>

namespace modplug {
namespace pervasives {

//ghetto bijective association arrays

template <typename assoc_t, typename key_t>
size_t idx_of_key(assoc_t assocs[], key_t key) {
    size_t i = 0;
    for (; i < sizeof(assocs) && assocs[i].key != key; ++i) { }
    return i;
}

template <typename assoc_t, typename val_t>
size_t idx_of_value(assoc_t assocs[], val_t value, int (*cmp) (val_t, val_t)) {
    size_t i = 0;
    for (; i < sizeof(assocs) && cmp(assocs[i].value, value) == 0; ++i) { }
    return i;
}

template <typename assoc_t, typename val_t>
auto key_of_value(assoc_t assocs[], val_t value, int (*cmp) (val_t, val_t))
    -> decltype(assocs[0].key)
{
    return assocs[idx_of_value(assocs, value, cmp)].key;
}

template <typename assoc_t, typename key_t>
auto value_of_key(assoc_t assocs[], key_t value) -> decltype(assocs[0].value) {
    return assocs[idx_of_key(assocs, value)].value;
}

}
}