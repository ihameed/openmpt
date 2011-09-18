#pragma once

#include "constants.h"
#include <vector>

namespace modplug {
namespace graph {

class node;

class graph {
public:
    graph();
    ~graph();

    node *channel_inputs[MAX_CHANNELS];

    node *mixed_output;
};

}
}