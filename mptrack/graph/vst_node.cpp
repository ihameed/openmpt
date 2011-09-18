#include "stdafx.h"

#include "node.h"
#include "vst_node.h"
#include "../mptrack.h"


namespace modplug {
namespace graph {

vst_node::vst_node() {
    _input_count = _vst->m_nInputs;
    _output_count = _vst->m_nOutputs;
    Log("every anus haxed");
}

vst_node::~vst_node() {
    Log("dead anus");
}

}
}