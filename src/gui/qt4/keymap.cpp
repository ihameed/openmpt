#include "stdafx.h"

#include <QtGui>

#include "keymap.h"
#include "../pervasives/pervasives.h"

#include "pattern_editor.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt4 {

global_actionmap_t global_actionmap;
pattern_actionmap_t pattern_actionmap;

void maptest() {
    DEBUG_FUNC("waoaoao!");
}

void init_action_maps() {
    global_actionmap["maptest"] = &maptest;

    pattern_actionmap["move_up"]    = &pattern_editor::move_up;
    pattern_actionmap["move_down"]  = &pattern_editor::move_down;
    pattern_actionmap["move_left"]  = &pattern_editor::move_left;
    pattern_actionmap["move_right"] = &pattern_editor::move_right;
}

global_keymap_t default_global_keymap() {
    global_keymap_t keymap;
    keymap[key_t(Qt::NoModifier, Qt::Key_A)] = "maptest";

    return keymap;
}

pattern_keymap_t default_pattern_keymap() {
    pattern_keymap_t keymap;

    keymap[key_t(Qt::NoModifier, Qt::Key_Up)]    = "move_up";
    keymap[key_t(Qt::NoModifier, Qt::Key_Down)]  = "move_down";
    keymap[key_t(Qt::NoModifier, Qt::Key_Left)]  = "move_left";
    keymap[key_t(Qt::NoModifier, Qt::Key_Right)] = "move_right";

    return keymap;
}

}
}
}