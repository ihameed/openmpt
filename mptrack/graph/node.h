#pragma once

#include <sndfile.h>

#include "constants.h"

namespace modplug {
namespace graph {

class node {
public:
    node();
    ~node();

    size_t input_count()  const { return _input_count; };
    size_t output_count() const { return _output_count; };

    float *_inputs[MAX_IO];
    float *_outputs[MAX_IO];
    node *_output_nodes[MAX_IO];
    size_t _input_count;
    size_t _output_count;
};



}
}