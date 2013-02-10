#pragma once

#include <cstdint>

#ifdef USE_GHETTO_NEWTYPES
#define NUMERIC_NEWTYPE(alias, type) \
struct alias { \
    explicit alias(type val) { \
        this->val = val; \
    } \
    bool operator < (const alias & other) const { \
        return val < other.val; \
    } \
    bool operator > (const alias & other) const { \
        return val > other.val; \
    } \
    bool operator == (const alias & other) const { \
        return val == other.val; \
    } \
    type val; \
};
#else
#define NUMERIC_NEWTYPE(alias, type) typedef type alias;
#endif


namespace modplug {
namespace pervasives {

#ifdef USE_GHETTO_NEWTYPES
template <typename Ty>
auto unwrap(Ty &wrapped) -> decltype(wrapped.val) {
    //XXX: remove once msvc has explicit conversion operators
    return wrapped.val;
}
#else
template <typename Ty>
Ty unwrap(Ty &wrapped) { return wrapped; }
#endif


}
}
