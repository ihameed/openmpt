#pragma once

class CPatternContainer;
class CSoundFile;

namespace modplug {
namespace serializers {

bool write_wao(CPatternContainer &, CSoundFile &);
bool read_wao();


}
}
