#pragma once

#include <type_traits>

namespace modplug {
namespace pervasives {

template <typename Ty>
struct bitset {
    typedef Ty flag_type;
    typedef typename std::underlying_type<Ty>::type underlying_type;
    underlying_type internal_val;

    bitset() : internal_val(0) { };
    bitset(underlying_type other) : internal_val(other) { };
};

template <typename Ty>
bitset<Ty>
bitset_set(const bitset<Ty> set, const Ty val) {
    auto rawval = static_cast<typename bitset<Ty>::underlying_type>(val);
    return bitset<Ty>(set.internal_val | rawval);
}

template <typename Ty>
bitset<Ty>
bitset_unset(const bitset<Ty> set, const Ty val) {
    auto rawval = static_cast<typename bitset<Ty>::underlying_type>(val);
    return bitset<Ty>(set.internal_val & ~rawval);
}

template <typename Ty>
bool
bitset_is_set(const bitset<Ty> set, const Ty val) {
    auto rawval = static_cast<typename bitset<Ty>::underlying_type>(val);
    return (set.internal_val & rawval) != 0;
}


}
}
