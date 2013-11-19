#pragma once

class CPatternContainer;
class module_renderer;

namespace modplug {
namespace serializers {

bool write_wao(CPatternContainer &, module_renderer &);
bool read_wao();


}
}