#pragma once

#include <memory>

#include "../tracker/orderlist.hpp"
#include "../pervasives/binaryparse.hpp"

class CPattern;
class module_renderer;

namespace modplug { namespace tracker {
struct modsample_t;
struct modinstrument_t;
} }

namespace modplug {
namespace serializers {

modplug::tracker::orderlist_t
read_uint8_orderlist(modplug::pervasives::binaryparse::context &ctx, size_t length);

std::function<CPattern &(const size_t)>
mk_alloc_pattern(module_renderer &);

std::function<modplug::tracker::modsample_t &(void)>
mk_alloc_sample(module_renderer &);

std::function<modplug::tracker::modinstrument_t &(void)>
mk_alloc_instr(module_renderer &);

}
}
