#include "stdafx.h"

#include "legacy_util.h"
#include "binaryprint.h"

using namespace modplug::tracker;

namespace modplug {
namespace serializers {

orderlist_t
read_uint8_orderlist(binaryparse::context &ctx, size_t length) {
    using namespace modplug::serializers::binaryparse;
    orderlist_t ret;
    auto rawlist = read_bytestring(ctx, length);
    for (size_t i = 0; i < length; ++i) {
        uint8_t item = rawlist[i];
        switch (item) {
        case 0xff:
            orderlist_insert(ret, i, orderlist_nil_entry);
            break;
        case 0xfe:
            orderlist_insert(ret, i, orderlist_spacer_entry);
            break;
        default:
            orderlist_insert(ret, i, mkentry(item));
        };
    }
    return ret;
}

}
}
