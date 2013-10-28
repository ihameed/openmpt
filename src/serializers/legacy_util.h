#pragma once

#include <memory>

#include "../tracker/orderlist.h"
#include "binaryparse.h"

namespace modplug {
namespace serializers {

modplug::tracker::orderlist_t
read_uint8_orderlist(binaryparse::context &ctx, size_t length);

}
}
