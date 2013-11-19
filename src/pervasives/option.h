#pragma once

#include "boost/optional.hpp"

#include <functional>

namespace modplug {
namespace pervasives {

template <typename Ty, typename SomeTy, typename NoneTy>
auto
match(const boost::optional<Ty> &x, SomeTy match_some, NoneTy match_none)
    -> decltype(match_some(x.get()))
{
    return x ? match_some(x.get()) : match_none();
}

template <typename Ty>
boost::optional<Ty>
some(const Ty & val) {
    return boost::optional<Ty>(val);
}

template <typename Ty>
boost::optional<Ty>
some(Ty && val) {
    return boost::optional<Ty>(std::forward<Ty>(val));
}

}
}
