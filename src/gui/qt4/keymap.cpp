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

template <typename f_t>
struct actionmap_assoc {
    const char *name;
    f_t action;
};


void maptest() {
    DEBUG_FUNC("waoaoao!");
}



static actionmap_assoc<global_action_t> global_actions[] = {
    { "maptest", &maptest },
    { "new", &maptest },
    { "open", &maptest },
    { "save", &maptest },
    { "save_as", &maptest },
    { "export_as_wave", &maptest },
    { "export_as_midi", &maptest },
    { "export_compatible", &maptest },

    { "play_pause", &maptest },
    { "stop", &maptest },
    { "play_song_from_start", &maptest },
    { "play_song_from_cursor", &maptest },
    { "play_pattern_from_start", &maptest },
    { "play_pattern_from_cursor", &maptest },

    { "undo", &maptest },
    { "cut", &maptest },
    { "copy", &maptest },
    { "paste", &maptest },
    { "mix_paste", &maptest },
    { "mix_paste_it", &maptest },

    { "view_general", &maptest },
    { "view_pattern", &maptest },
    { "view_samples", &maptest },
    { "view_instruments", &maptest },
    { "view_graph", &maptest },
    { "view_comments", &maptest },
};

static actionmap_assoc<pattern_action_t> pattern_actions[] = {
    { "move_up",    &pattern_editor::move_up },
    { "move_down",  &pattern_editor::move_down },
    { "move_left",  &pattern_editor::move_left },
    { "move_right", &pattern_editor::move_right },

    { "select_up",    &pattern_editor::select_up },
    { "select_down",  &pattern_editor::select_down },
    { "select_left",  &pattern_editor::select_left },
    { "select_right", &pattern_editor::select_right },
};

template <typename amap_t, typename f_t, size_t size>
void init_actions(amap_t &actionmap, const actionmap_assoc<f_t> (&assocs)[size]) {
    for (size_t idx = 0; idx < size; ++idx) {
        actionmap[assocs[idx].name] = assocs[idx].action;
    }
}

const size_t MaxTones = 12;

const char *note_names[MaxTones] = {
    "c", "cs", "d", "ds", "e", "f", "fs", "g", "gs", "a", "as", "b"
};

void init_note_maps() {
    auto &m = pattern_actionmap;

    for (char i = '0'; i < '3'; ++i) {
        int octave = i - '0';

        std::string prefix("note_oct");
        prefix += i;
        prefix += "_";

        for (size_t tone = 0; tone < MaxTones; ++tone) {
            auto fname = prefix + note_names[tone];
            m[fname] = [octave, tone] (pattern_editor &editor) {
                pattern_editor::insert_note(editor, octave, tone);
            };
        }
    }

    std::string vol_prefix("vol_");
    for (char i = '0'; i <= '9'; ++i) {
        auto fname = vol_prefix + i;
        uint8_t digit = i - '0';
        m[fname] = [digit] (pattern_editor &editor) {
            pattern_editor::insert_volparam(editor, digit);
        };
    }
}

void init_action_maps() {
    init_actions(global_actionmap, global_actions);

    init_actions(pattern_actionmap, pattern_actions);

    init_note_maps();
}

global_keymap_t default_global_keymap() {
    global_keymap_t keymap;
    keymap[key_t(Qt::NoModifier, Qt::Key_A)] = "maptest";

    keymap[key_t(Qt::ControlModifier, Qt::Key_N)] = "new";
    keymap[key_t(Qt::ControlModifier, Qt::Key_S)] = "save";

    keymap[key_t(Qt::NoModifier, Qt::Key_F5)] = "play_pause";
    keymap[key_t(Qt::NoModifier, Qt::Key_F6)] = "play_song_from_start";
    keymap[key_t(Qt::NoModifier, Qt::Key_F7)] = "play_pattern_from_start";
    keymap[key_t(Qt::NoModifier, Qt::Key_F8)] = "stop";
    keymap[key_t(Qt::NoModifier, Qt::Key_F9)] = "play_song_from_cursor";

    return keymap;
}

pattern_keymap_t default_pattern_keymap() {
    pattern_keymap_t m;

    m[key_t(Qt::NoModifier, Qt::Key_Up)]    = "move_up";
    m[key_t(Qt::NoModifier, Qt::Key_Down)]  = "move_down";
    m[key_t(Qt::NoModifier, Qt::Key_Left)]  = "move_left";
    m[key_t(Qt::NoModifier, Qt::Key_Right)] = "move_right";

    m[key_t(Qt::ShiftModifier, Qt::Key_Up)]    = "select_up";
    m[key_t(Qt::ShiftModifier, Qt::Key_Down)]  = "select_down";
    m[key_t(Qt::ShiftModifier, Qt::Key_Left)]  = "select_left";
    m[key_t(Qt::ShiftModifier, Qt::Key_Right)] = "select_right";

    auto notekey = [&m] (int key, const char *act) {
        return m[key_t(Qt::NoModifier, key, ContextNoteCol)] = act;
    };

    notekey(Qt::Key_Q,            "note_oct0_c");
    notekey(Qt::Key_W,            "note_oct0_cs");
    notekey(Qt::Key_E,            "note_oct0_d");
    notekey(Qt::Key_R,            "note_oct0_ds");
    notekey(Qt::Key_T,            "note_oct0_e");
    notekey(Qt::Key_Y,            "note_oct0_f");
    notekey(Qt::Key_U,            "note_oct0_fs");
    notekey(Qt::Key_I,            "note_oct0_g");
    notekey(Qt::Key_O,            "note_oct0_gs");
    notekey(Qt::Key_P,            "note_oct0_a");
    notekey(Qt::Key_BracketLeft,  "note_oct0_as");
    notekey(Qt::Key_BracketRight, "note_oct0_b");

    notekey(Qt::Key_A,          "note_oct1_c");
    notekey(Qt::Key_S,          "note_oct1_cs");
    notekey(Qt::Key_D,          "note_oct1_d");
    notekey(Qt::Key_F,          "note_oct1_ds");
    notekey(Qt::Key_G,          "note_oct1_e");
    notekey(Qt::Key_H,          "note_oct1_f");
    notekey(Qt::Key_J,          "note_oct1_fs");
    notekey(Qt::Key_K,          "note_oct1_g");
    notekey(Qt::Key_L,          "note_oct1_gs");
    notekey(Qt::Key_Semicolon,  "note_oct1_a");
    notekey(Qt::Key_Apostrophe, "note_oct1_as");
    notekey(Qt::Key_Backslash,  "note_oct1_b");

    notekey(Qt::Key_Z,      "note_oct2_c");
    notekey(Qt::Key_X,      "note_oct2_cs");
    notekey(Qt::Key_C,      "note_oct2_d");
    notekey(Qt::Key_V,      "note_oct2_ds");
    notekey(Qt::Key_B,      "note_oct2_e");
    notekey(Qt::Key_N,      "note_oct2_f");
    notekey(Qt::Key_M,      "note_oct2_fs");
    notekey(Qt::Key_Comma,  "note_oct2_g");
    notekey(Qt::Key_Period, "note_oct2_gs");
    notekey(Qt::Key_Slash,  "note_oct2_a");

    auto volkey = [&m] (int key, const char *act) {
        return m[key_t(Qt::NoModifier, key, ContextVolCol)] = act;
    };

    volkey(Qt::Key_0, "vol_0");
    volkey(Qt::Key_1, "vol_1");
    volkey(Qt::Key_2, "vol_2");
    volkey(Qt::Key_3, "vol_3");
    volkey(Qt::Key_4, "vol_4");
    volkey(Qt::Key_5, "vol_5");
    volkey(Qt::Key_6, "vol_6");
    volkey(Qt::Key_7, "vol_7");
    volkey(Qt::Key_8, "vol_8");
    volkey(Qt::Key_9, "vol_9");

    volkey(Qt::Key_V, "vol_vol");
    volkey(Qt::Key_P, "vol_pan");
    volkey(Qt::Key_C, "vol_slide_up");
    volkey(Qt::Key_D, "vol_slide_down");
    volkey(Qt::Key_A, "vol_fine_slide_up");
    volkey(Qt::Key_B, "vol_fine_slide_down");
    volkey(Qt::Key_U, "vol_vibrato_speed");
    volkey(Qt::Key_H, "vol_vibrato");
    volkey(Qt::Key_U, "vol_vibrato_speed");
    volkey(Qt::Key_L, "vol_xm_pan_left");
    volkey(Qt::Key_R, "vol_xm_pan_right");
    volkey(Qt::Key_L, "vol_xm_pan_left");
    volkey(Qt::Key_G, "vol_portamento");
    volkey(Qt::Key_F, "vol_portamento_up");
    volkey(Qt::Key_E, "vol_portamento_down");
    volkey(Qt::Key_O, "vol_offset");


    return m;
}

}
}
}