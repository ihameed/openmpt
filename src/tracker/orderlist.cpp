#include "stdafx.h"

#include "orderlist.h"
#include "pervasives/pervasives.h"

#include <iterator>
#include <algorithm>

namespace modplug {
namespace tracker {

inline bool
is_entry_nil(orderlist_entry_t entry) {
    return entry.tag == orderlist_entry_tag_t::Nil;
}

size_t
resize_to_fit(orderlist_t &orders, orderindex_t idx) {
    const size_t size = orders.orders.size();
    const auto idx_sz = static_cast<size_t>(idx);
    if (idx_sz > size) {
        orders.orders.resize(idx_sz + 1, orderlist_nil_entry);
    }
    return idx_sz;
}

orderlist_entry_t
mkentry(patternindex_t val) {
    orderlist_entry_t ret = {
        orderlist_entry_tag_t::Pattern,
        val
    };
    return ret;
}

boost::optional<orderindex_t>
calculate_max_index(const orderlist_t &orders) {
    if (orders.max_index) {
        const auto idx_sz = static_cast<size_t>(orders.max_index.get());
        const auto sz = orders.orders.size();
        for (size_t idx = std::min(idx_sz, sz); idx >= 0; --idx) {
            if (!is_entry_nil(orders.orders[idx_sz])) {
                return static_cast<orderindex_t>(idx_sz);
            }
        }
        return boost::none;
    } else {
        return orders.max_index;
    }
}

boost::optional<orderindex_t>
orderlist_max_index(const orderlist_t &orders) {
    return orders.max_index;
}

orderlist_entry_t
orderlist_at(const orderlist_t &orders, orderindex_t idx) {
    const size_t size = orders.orders.size();
    const auto idx_sz = static_cast<size_t>(idx);
    return idx_sz >= size
        ? orderlist_nil_entry
        : orders.orders[idx_sz];
}

void
orderlist_insert(orderlist_t &orders, orderindex_t idx, orderlist_entry_t val) {
    auto idx_sz = resize_to_fit(orders, idx);
    auto offset = orders.orders.begin() + idx_sz;
    auto is_nil = is_entry_nil(val);
    auto max_index = orders.max_index;
    orders.orders.insert(offset, val);
    orders.max_index = max_index
        ? idx > max_index
            ? (is_nil ? max_index : idx)
            : max_index.get() + 1
        : is_nil
            ? max_index
            : idx
        ;
}

void
orderlist_replace(orderlist_t &orders, orderindex_t idx, orderlist_entry_t val) {
    auto idx_sz = resize_to_fit(orders, idx);
    orders.orders[idx_sz] = val;
    auto is_nil = is_entry_nil(val);
    auto max_index = orders.max_index;
    orders.max_index = max_index
        ? is_nil
            ? (idx == max_index ? calculate_max_index(orders) : max_index)
            : (idx > max_index ? idx : max_index)
        : is_nil
            ? max_index
            : idx
        ;
}

void
orderlist_remove(orderlist_t &orders, orderindex_t idx) {
    const size_t size = orders.orders.size();
    auto idx_sz = static_cast<size_t>(idx);
    if (idx_sz < size) {
        auto offset = orders.orders.begin() + idx_sz;
        orders.orders.erase(offset);
        orders.max_index = orders.max_index
            ? (idx <= orders.max_index ? calculate_max_index(orders) : idx)
            : orders.max_index;
    }
}

size_t
orderlist_size(orderlist_t &orders) {
    return orders.max_index
        ? static_cast<size_t>(orders.max_index.get()) + 1
        : 0;
}

}
}
