#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class module_renderer;

namespace modplug {

namespace pervasives { namespace binaryparse { struct context; } }

namespace serializers {
namespace xm {

bool
read(module_renderer &song, modplug::pervasives::binaryparse::context &ctx);


}
}
}
