#pragma once

// #define USE_GHETTO_NEWTYPES

#ifdef USE_GHETTO_NEWTYPES

#define NEWTYPE(type, wrapped) \
#pragma pack(push) \
#pragma pack(1) \
struct type { \
    explicit type(wrapped val) { \
        this->val = val; \
    } \
    wrapped val; \
}; \
#pragma pack(pop)

#define ORDERED_TYPE(type) \
inline bool operator < (const type & x, const type & y) { \
    return x.val < y.val; \
} \
inline bool operator <= (const type & x, const type & y) { \
    return x.val <= y.val; \
} \
inline bool operator > (const type & x, const type & y) { \
    return x.val > y.val; \
} \
inline bool operator >= (const type & x, const type & y) { \
    return x.val >= y.val; \
} \
inline bool operator == (const type & x, const type & y) { \
    return x.val == y.val; \
} \
inline bool operator != (const type & x, const type & y) { \
    return x.val != y.val; \
}

#define ARITHMETIC_TYPE(type) \
inline type operator + (const type & x, const type & y) { \
    return type(x.val + y.val); \
} \
inline type operator - (const type & x, const type & y) { \
    return type(x.val - y.val); \
} \
inline type operator * (const type & x, const type & y) { \
    return type(x.val * y.val); \
} \
inline type operator / (const type & x, const type & y) { \
    return type(x.val / y.val); \
} \
inline type operator % (const type & x, const type & y) { \
    return type(x.val % y.val); \
} \
inline type operator - (const type & x) { \
    return type(-x.val); \
}

#define INCR_DECR_TYPE(type) \
inline type & operator ++ (type & x) { \
    ++x.val; \
    return x; \
} \
inline type & operator -- (type & x) { \
    --x.val; \
    return x; \
} \
inline type & operator ++ (type & x, int) { \
    type old = x; \
    ++x.val; \
    return old; \
} \
inline type & operator -- (type & x, int) { \
    type old = x; \
    --x.val; \
    return old; \
}

#define BITS_TYPE(type) \
inline type operator << (const type & x, int y) { \
    return type(x.val << y); \
} \
inline type operator >> (const type & x, int y) { \
    return type(x.val >> y); \
}

#else

#define NEWTYPE(alias, type) typedef type alias;
#define ORDERED_TYPE(type)
#define ARITHMETIC_TYPE(type)
#define INCR_DECR_TYPE(type)

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
Ty unwrap(Ty &&wrapped) { return wrapped; }
#endif


}
}
