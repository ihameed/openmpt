#pragma once

#include "option.h"

#include <algorithm>

namespace modplug {
namespace pervasives {

template <typename Ty>
bool
none_is_supremum(const boost::optional<Ty> &x, const boost::optional<Ty> &y) {
    return !x ? false : (!y ? true : x.get() < y.get());
}

template <typename Ty>
bool
none_is_infimum(const boost::optional<Ty> &x, const boost::optional<Ty> &y) {
    return !x ? true : (!y ? false : x.get() < y.get());
}

template <typename ColTy>
auto
minimum(const ColTy &col) -> boost::optional<typename ColTy::value_type> {
    typedef typename ColTy::value_type ValTy;
    auto iter = min_element(col.cbegin(), col.cend(), none_is_supremum<ValTy>);
    if (iter == col.cend()) return boost::none;
    return *iter;
}

template <typename Ty>
boost::optional<Ty>
min_(const boost::optional<Ty> &x, const boost::optional<Ty> &y) {
    return std::min(x, y, none_is_supremum<Ty>);
}

template <typename Ty>
boost::optional<Ty>
max_(const boost::optional<Ty> &x, const boost::optional<Ty> &y) {
    return std::max(x, y, none_is_infimum<Ty>);
}

}
}
