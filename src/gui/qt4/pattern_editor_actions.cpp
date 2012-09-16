#include "stdafx.h"

#include "../../pervasives/pervasives.h"
#include "pattern_editor_actions.h"
#include "pattern_editor.h"

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt4 {


const int hugeor = 999999;

void clear_at(modevent_t *evt, elem_t elem) {
    switch (elem) {
    case ElemNote:  evt->note    = 0; break;
    case ElemInstr: evt->instr   = 0; break;
    case ElemVol:   evt->vol     = 0;
                    evt->volcmd  = VolCmdNone; break;
    case ElemCmd:   evt->command = CmdNone; break;
    case ElemParam: evt->param   = 0; break;
    }
}

void pattern_editor_actions::clear_selected_cells(pattern_editor &editor) {
    auto &corners = editor.draw.selection_corners();
    auto &upper_left   = corners.topleft;
    auto &bottom_right = corners.bottomright;

    auto pattern = editor.draw.active_pattern();

    for (auto pos = upper_left; pos.row <= bottom_right.row; ++pos.row) {
        for (pos.column = upper_left.column;
             pos.column <= bottom_right.column; ++pos.column)
        {
            for (elem_t elem = ElemNote; elem < ElemMax; ++elem) {
                pos.subcolumn = elem;
                if (pos_in_rect(corners, pos)) {
                    modevent_t *evt = pattern->GetpModCommand(pos.row,
                                                             pos.column);
                    clear_at(evt, elem);
                }
            }
        }
    }

    editor.draw.update();
}

void pattern_editor_actions::delete_row(pattern_editor &editor) {
    auto pattern  = editor.draw.active_pattern();
    auto &pos     = editor.draw.pos();
    auto numrows  = pattern->GetNumRows();

    if (numrows > 2) {
        for (size_t row = pos.row; row <= numrows - 2; ++row) {
            auto evt     = pattern->GetpModCommand(row, pos.column);
            auto nextevt = pattern->GetpModCommand(row + 1, pos.column);
        }
    }
    if (numrows > 1) {
        pattern->GetpModCommand(numrows - 1, pos.column)->Clear();
    }

    editor.draw.update();
}

void pattern_editor_actions::insert_row(pattern_editor &editor) {
    //TODO
}

void pattern_editor_actions::insert_note(pattern_editor &editor, uint8_t octave,
    uint8_t tone_number)
{
    editor.draw.collapse_selection();
    auto evt = editor.draw.active_event();
    auto newcmd = *evt;

    newcmd.note = 1 + tone_number + 12 * (octave + editor.base_octave);

    *evt = newcmd;
    editor.draw.update();
}

void pattern_editor_actions::insert_instr(pattern_editor &editor, uint8_t digit) {
    editor.draw.collapse_selection();
    auto evt = editor.draw.active_event();
    auto newcmd = *evt;

    instr_t instr = (newcmd.instr % 10) * 10 + digit;
    newcmd.instr = instr;

    *evt = newcmd;
    editor.draw.update();
}

void pattern_editor_actions::insert_volparam(pattern_editor &editor, uint8_t digit) {
    editor.draw.collapse_selection();
    auto evt = editor.draw.active_event();
    auto newcmd = *evt;

    if (newcmd.volcmd == VolCmdNone) {
        newcmd.volcmd = VolCmdVol;
    }

    vol_t vol = (newcmd.vol % 10) * 10 + digit;
    if (vol > 64) {
        vol = digit;
    }
    newcmd.vol = vol;

    *evt = newcmd;
    editor.draw.update();
}

void pattern_editor_actions::insert_volcmd(pattern_editor &editor, volcmd_t cmd) {
    editor.draw.collapse_selection();
    auto evt = editor.draw.active_event();
    auto newcmd = *evt;

    newcmd.volcmd = cmd;

    *evt = newcmd;
    editor.draw.update();
}

void pattern_editor_actions::insert_cmd(pattern_editor &editor, cmd_t cmd) {
    editor.draw.collapse_selection();
    auto evt = editor.draw.active_event();
    auto newcmd = *evt;

    newcmd.command = cmd;

    *evt = newcmd;
    editor.draw.update();
}

void pattern_editor_actions::insert_cmdparam(pattern_editor &editor, uint8_t digit) {
    editor.draw.collapse_selection();
    auto evt = editor.draw.active_event();
    auto newcmd = *evt;

    param_t param = (newcmd.param % 0x10) * 0x10 + digit;
    newcmd.param = param;

    *evt = newcmd;
    editor.draw.update();
}




void pattern_editor_actions::move_up(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_row(editor.draw.pos(), -1);
    editor.draw.move_to(newpos);
}

void pattern_editor_actions::move_down(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_row(editor.draw.pos(), 1);
    editor.draw.move_to(newpos);
}

void pattern_editor_actions::move_left(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_subcol(editor.draw.pos(), -1);
    editor.draw.move_to(newpos);
}

void pattern_editor_actions::move_right(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_subcol(editor.draw.pos(), 1);
    editor.draw.move_to(newpos);
}


void pattern_editor_actions::move_first_row(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_row(editor.draw.pos(), -hugeor);
    editor.draw.move_to(newpos);
}

void pattern_editor_actions::move_last_row(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_row(editor.draw.pos(), hugeor);
    editor.draw.move_to(newpos);
}

void pattern_editor_actions::move_first_col(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_subcol(editor.draw.pos(), -hugeor);
    editor.draw.move_to(newpos);
}

void pattern_editor_actions::move_last_col(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_subcol(editor.draw.pos(), hugeor);
    editor.draw.move_to(newpos);
}



void pattern_editor_actions::select_up(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_row(editor.draw.pos(), -1);
    editor.draw.set_selection_end(newpos);
    editor.update();
}

void pattern_editor_actions::select_down(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_row(editor.draw.pos(), 1);
    editor.draw.set_selection_end(newpos);
    editor.update();
}

void pattern_editor_actions::select_left(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_subcol(editor.draw.pos(), -1);
    editor.draw.set_selection_end(newpos);
    editor.update();
}

void pattern_editor_actions::select_right(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_subcol(editor.draw.pos(), 1);
    editor.draw.set_selection_end(newpos);
    editor.update();
}


void pattern_editor_actions::select_first_row(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_row(editor.draw.pos(), -hugeor);
    editor.draw.set_selection_end(newpos);
    editor.update();
}

void pattern_editor_actions::select_last_row(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_row(editor.draw.pos(), hugeor);
    editor.draw.set_selection_end(newpos);
    editor.update();
}

void pattern_editor_actions::select_first_col(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_subcol(editor.draw.pos(), -hugeor);
    editor.draw.set_selection_end(newpos);
    editor.update();
}

void pattern_editor_actions::select_last_col(pattern_editor &editor) {
    auto newpos = editor.draw.pos_move_by_subcol(editor.draw.pos(), hugeor);
    editor.draw.set_selection_end(newpos);
    editor.update();
}


}
}
}