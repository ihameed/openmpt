#pragma once

#include <type_traits>

namespace modplug {
namespace pervasives {

template <typename Ty>
struct bitset {
    typedef Ty flag_type;
    typedef typename std::underlying_type<Ty>::type underlying_type;
    Ty _internal_val_;

    bitset() : _internal_val_(static_cast<Ty>(0)) { };
    explicit bitset(underlying_type other) : _internal_val_(static_cast<Ty>(other)) { };
    explicit bitset(const Ty val) : _internal_val_(val) { };
};

template <typename Ty>
bool __forceinline
operator == (const bitset<Ty> &x, const bitset<Ty> &y) {
    return x._internal_val_ == y._internal_val_;
}

template <typename Ty>
void __forceinline
bitset_add(bitset<Ty> &set, const Ty val) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    set._internal_val_ = static_cast<Ty>(
        static_cast<wrapped>(set._internal_val_) |
        static_cast<wrapped>(val));
}

template <typename Ty>
void __forceinline
bitset_remove(bitset<Ty> set, const Ty val) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    set._internal_val_ = static_cast<Ty>(
        static_cast<wrapped>(set._internal_val_) &
        ~static_cast<wrapped>(val));
}

template <typename Ty>
bitset<Ty> __forceinline
bitset_intersect(const bitset<Ty> x, const bitset<Ty> y) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    return bitset<Ty>(
        static_cast<wrapped>(x._internal_val_) &
        static_cast<wrapped>(y._internal_val_)
    );
}

template <typename Ty>
bool __forceinline
bitset_intersects(const bitset<Ty> x, const bitset<Ty> y) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    return (
        static_cast<wrapped>(x._internal_val_) &
        static_cast<wrapped>(y._internal_val_)
    ) != 0;
}

template <typename Ty>
bitset<Ty> __forceinline
bitset_merge(const bitset<Ty> x, const bitset<Ty> y) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    return bitset<Ty>(
        static_cast<wrapped>(x._internal_val_) |
        static_cast<wrapped>(y._internal_val_)
    );
}

template <typename Ty>
bitset<Ty> __forceinline
bitset_set(const bitset<Ty> set, const Ty val) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    return bitset<Ty>(
        static_cast<wrapped>(set._internal_val_) |
        static_cast<wrapped>(val));
}

template <typename Ty>
bitset<Ty> __forceinline
bitset_unset(const bitset<Ty> set, const Ty val) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    return bitset<Ty>(
        static_cast<wrapped>(set._internal_val_) &
        ~static_cast<wrapped>(val));
}

template <typename Ty>
bool __forceinline
bitset_is_set(const bitset<Ty> set, const Ty val) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    return (
        static_cast<wrapped>(set._internal_val_) &
        static_cast<wrapped>(val)) != 0;
}

template <typename Ty>
bool __forceinline
bitset_is_set(const bitset<Ty> set, const Ty val, const Ty val2) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    return (
        static_cast<wrapped>(set._internal_val_) &
        (static_cast<wrapped>(val) | static_cast<wrapped>(val2))) != 0;
}

template <typename Ty>
bool __forceinline
bitset_is_set(const bitset<Ty> set, const Ty val, const Ty val2, const Ty val3) {
    typedef typename bitset<Ty>::underlying_type wrapped;
    return (
        static_cast<wrapped>(set._internal_val_) &
        (static_cast<wrapped>(val) |
         static_cast<wrapped>(val2) |
         static_cast<wrapped>(val3) )) != 0;
}


}
}
