#pragma once

#include "constants.h"
#include <vector>
#include <map>

namespace modplug {
namespace graph {

class node;

class graph {
public:
    graph();
    ~graph();

    void process();

    node *channel_inputs[MAX_CHANNELS];
    node *mixed_output;

    //std::map<int, node>
};

}
}