#pragma once

#define VST_FORCE_DEPRECATED 0
#include <aeffectx.h>
#include <vstfxstore.h>

#include "../../soundlib/Sndfile.h"
#include "../../soundlib/Snd_defs.h"
#include "../vstplug.h"


namespace modplug {
namespace graph {

class node;

class vst_node : public node {
public:
    vst_node();
    ~vst_node();

private:
    CVstPlugin *_vst;
};

}
}
