#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class module_renderer;

namespace modplug { namespace serializers { namespace binaryparse {
struct context;
} } }

namespace modplug {
namespace serializers {
namespace xm {

bool
read(module_renderer &song, modplug::serializers::binaryparse::context &ctx);


}
}
}
