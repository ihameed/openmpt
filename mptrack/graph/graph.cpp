#include "stdafx.h"

#include "graph.h"

namespace modplug {
namespace graph {


graph::graph() {
    Log("created graph\n");
}

graph::~graph() {
    Log("destroyed graph\n");
}

void graph::process() {
    Log("sup   i should process something\n");
}


}
}