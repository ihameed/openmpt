#pragma once

#include "types.h"

#include <vector>
#include "boost/optional.hpp"

namespace modplug {
namespace tracker {

enum struct orderlist_entry_tag_t : uint8_t {
    Nil = 0,
    Spacer,
    Pattern
};

struct orderlist_entry_t {
    orderlist_entry_tag_t tag;
    patternindex_t pattern;
};

const orderlist_entry_t orderlist_nil_entry = {
    orderlist_entry_tag_t::Nil,
    patternindex_t(0)
};

const orderlist_entry_t orderlist_spacer_entry = {
    orderlist_entry_tag_t::Spacer,
    patternindex_t(0)
};

struct orderlist_t {
    std::vector<orderlist_entry_t> orders;
    boost::optional<orderindex_t> max_index;
};

orderlist_entry_t mkentry(patternindex_t val);

orderlist_entry_t orderlist_at(orderlist_t &, orderindex_t);

boost::optional<orderindex_t> orderlist_max_index(orderlist_t &);

void orderlist_replace(orderlist_t &, orderindex_t, orderlist_entry_t);

void orderlist_insert(orderlist_t &, orderindex_t, orderlist_entry_t);

void orderlist_remove(orderlist_t &, orderindex_t);

size_t orderlist_size(orderlist_t &);

}
}
