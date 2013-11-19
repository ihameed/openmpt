#pragma once

class module_renderer;

namespace modplug {

namespace pervasives { namespace binaryparse { struct context; } }

namespace serializers {
namespace it {

bool
read(module_renderer &song, modplug::pervasives::binaryparse::context &ctx);

}
}
}
