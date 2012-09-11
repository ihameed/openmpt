#include "stdafx.h"

#include "../../Resource.h"
#include "../../pervasives/pervasives.h"

#include "pattern_editor.h"
#include "pattern_editor_column.h"
#include "util.h"

#include <GL/gl.h>
#include <GL/glu.h>

#include "keymap.h"

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt4 {

const int hugeor = 999999;

QImage load_font() {
    auto resource = MAKEINTRESOURCE(IDB_PATTERNVIEW);
    auto instance = GetModuleHandle(nullptr);
    auto hdc      = GetDC(nullptr);

    auto font_bitmap = load_bmp_resource(resource, instance, hdc)
                      .convertToFormat(QImage::Format_MonoLSB);

    font_bitmap.setColor(0, qRgba(0xff, 0xff, 0xff, 0x00));
    font_bitmap.setColor(1, qRgba(0xff, 0xff, 0xff, 0xff));

    ReleaseDC(nullptr, hdc);

    return font_bitmap;
};

pattern_editor::pattern_editor(
    module_renderer &renderer,
    const pattern_keymap_t &keymap,
    const pattern_keymap_t &it_keymap,
    const pattern_keymap_t &xm_keymap,
    const colors_t &colors
) :
    renderer(renderer),
    keymap(keymap),
    it_keymap(it_keymap),
    xm_keymap(xm_keymap),
    font_metrics(small_pattern_font),
    follow_playback(true)
{
    setFocusPolicy(Qt::ClickFocus);
    font_bitmap = load_font();
    update_colors(colors);
    set_base_octave(4);
}

void pattern_editor::update_colors(const colors_t &newcolors) {
    colors = newcolors;
    /*
    auto &bgc = colors[colors_t::Normal].background;
    makeCurrent();
    glClearColor(bgc.redF() * 0.75, bgc.greenF() * 0.75, bgc.blueF() * 0.75, 0.5);
    */
    updateGL();
}

void pattern_editor::update_playback_position(
    const player_position_t &position)
{
    playback_pos = position;
    if (follow_playback) {
        active_pos = playback_pos;
    }
    repaint();
}

void pattern_editor::initializeGL() {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY_EXT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    font_texture = bindTexture(font_bitmap);

    resizeGL(100, 100);

    glBindTexture(GL_TEXTURE_2D, font_texture);
}

void pattern_editor::resizeGL(int width, int height) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, width, height, 0);
    glViewport(0, 0, width, height);
    glMatrixMode(GL_MODELVIEW);

    clipping_rect = QRect(0, 0, width, height);

    this->width = width;
    this->height = height;
}

void pattern_editor::paintGL() {
    ghettotimer homesled(__FUNCTION__);

    chnindex_t channel_count = renderer.GetNumChannels();

    draw_state state = {
        renderer,

        renderer.GetNumPatterns(),
        clipping_rect,

        playback_pos,
        active_pos,

        colors,
        corners,
        pos()
    };

    /*
    glClear(GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT |
            GL_ACCUM_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT);
            */

    int painted_width = 0;

    for (chnindex_t idx = 0;
         idx < channel_count && painted_width < width;
         ++idx)
    {
        note_column notehomie(painted_width, idx, font_metrics);
        notehomie.draw_column(state);
        painted_width += notehomie.width();
    }
}

editor_position_t pattern_editor::pos_move_by_row(
    const editor_position_t &in, int amount) const
{
    auto newpos = in;
    bool down = amount < 0;
    uint32_t absamount = down ? -amount : amount;

    if (down) {
        newpos.row = absamount > newpos.row
            ? 0
            : newpos.row - absamount;
    } else {
        //XXXih: :[
        newpos.row += absamount;
        auto numrows = renderer.Patterns[active_pos.pattern].GetNumRows();
        if (newpos.row >= numrows) {
            newpos.row = numrows - 1; //XXXih: :[[[
        }
    }

    return newpos;
}

editor_position_t pattern_editor::pos_move_by_subcol(
    const editor_position_t &in, int amount) const
{
    auto newpos = in;

    bool left = amount < 0;
    uint32_t absamount = left ? -amount : amount;

    //TODO: multiple column types
    if (left) {
        for (size_t i = 0; i < absamount; ++i) {
            if (newpos.subcolumn == ElemMin) {
                if (newpos.column > 0) {
                    newpos.subcolumn = (elem_t) (ElemMax - 1);
                    newpos.column -= 1;
                } else {
                    break;
                }
            } else {
                --newpos.subcolumn;
            }
        }
    } else {
        uint32_t max = renderer.GetNumChannels();
        for (size_t i = 0; i < absamount; ++i) {
            if (newpos.subcolumn == ElemMax - 1) {
                if (newpos.column < max - 1) {
                    newpos.subcolumn = ElemMin;
                    newpos.column += 1;
                } else {
                    break;
                }
            } else {
                ++newpos.subcolumn;
            }
        }
    }

    return newpos;

}

bool pattern_editor::position_from_point(const QPoint &point,
                                         editor_position_t &pos)
{
    chnindex_t channel_count = renderer.GetNumChannels();

    int left = 0;

    bool success = false;

    for (chnindex_t idx = 0; idx < channel_count; ++idx) {
        note_column notehomie(left, idx, font_metrics);
        success = notehomie.position_from_point(point, pos);
        if (success) {
            break;
        }
        left += notehomie.width();
    }

    return success;
}

void pattern_editor::recalc_corners() {
    corners = normalize_selection(selection);
}

bool pattern_editor::set_pos_from_point(const QPoint &point,
                                        editor_position_t &pos)
{
    editor_position_t newpos;
    if (position_from_point(point, newpos)) {
        pos = newpos;
        return true;
    }
    return false;
}

void pattern_editor::set_selection_start(const QPoint &point) {
    if (set_pos_from_point(point, selection.start)) {
        recalc_corners();
    }
}

void pattern_editor::set_selection_start(const editor_position_t &pos) {
    selection.start = pos;
    recalc_corners();
}

void pattern_editor::set_selection_end(const QPoint &point) {
    if (set_pos_from_point(point, selection.end)) {
        recalc_corners();
    }
}

void pattern_editor::set_selection_end(const editor_position_t &pos) {
    selection.end = pos;
    recalc_corners();
}

void pattern_editor::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        is_dragging = true;
        set_selection_start(event->pos());
        set_selection_end(event->pos());
        update();
    }
}

void pattern_editor::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton && is_dragging) {
        set_selection_end(event->pos());
        update();
    }
}

void pattern_editor::mouseReleaseEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        is_dragging = false;
        set_selection_end(event->pos());
        update();
    }
}

bool pattern_editor::invoke_key(const pattern_keymap_t &km, key_t key) {
    auto action = action_of_key(km, pattern_actionmap, key);
    if (action) {
        action(*this);
        return true;
    } else {
        return false;
    }
}

void pattern_editor::keyPressEvent(QKeyEvent *event) {
    Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    auto context_key = key_t(modifiers, event->key(), keycontext());
    auto pattern_key = key_t(modifiers, event->key());

    if (invoke_key(keymap,    context_key)) return;
    if (invoke_key(it_keymap, context_key)) return;
    if (invoke_key(keymap,    pattern_key)) return;

    QGLWidget::keyPressEvent(event);
}

void pattern_editor::move_to(const editor_position_t &target) {
    selection.start = target;
    selection.end   = target;
    recalc_corners();
    update();
}

const editor_position_t &pattern_editor::pos() const {
    return selection.end;
}

keycontext_t pattern_editor::keycontext() const{
    switch (pos().subcolumn) {
    case ElemNote:  return ContextNoteCol;
    case ElemInstr: return ContextInstrCol;
    case ElemVol:   return ContextVolCol;
    case ElemCmd:   return ContextCmdCol;
    case ElemParam: return ContextParamCol;
    default:        return ContextGlobal;
    }
}

void pattern_editor::set_base_octave(uint8_t octave) {
    base_octave = octave;
}

void pattern_editor::collapse_selection() {
    auto &newpos = selection.start = selection.end = pos();
    recalc_corners();
    move_to(newpos);
}

modevent_t *pattern_editor::active_event() {
    auto &modspec = renderer.GetModSpecifications();
    auto patternidx = active_pos.pattern;
    modevent_t *evt = renderer.Patterns[patternidx]
                              .GetpModCommand(pos().row, pos().column);
    return evt;
}






void pattern_editor::move_up(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_row(editor.pos(), -1);
    editor.move_to(newpos);
}

void pattern_editor::move_down(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_row(editor.pos(), 1);
    editor.move_to(newpos);
}

void pattern_editor::move_left(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_subcol(editor.pos(), -1);
    editor.move_to(newpos);
}

void pattern_editor::move_right(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_subcol(editor.pos(), 1);
    editor.move_to(newpos);
}


void pattern_editor::move_first_row(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_row(editor.pos(), -hugeor);
    editor.move_to(newpos);
}

void pattern_editor::move_last_row(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_row(editor.pos(), hugeor);
    editor.move_to(newpos);
}

void pattern_editor::move_first_col(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_subcol(editor.pos(), -hugeor);
    editor.move_to(newpos);
}

void pattern_editor::move_last_col(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_subcol(editor.pos(), hugeor);
    editor.move_to(newpos);
}



void pattern_editor::select_up(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_row(editor.pos(), -1);
    editor.set_selection_end(newpos);
    editor.update();
}

void pattern_editor::select_down(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_row(editor.pos(), 1);
    editor.set_selection_end(newpos);
    editor.update();
}

void pattern_editor::select_left(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_subcol(editor.pos(), -1);
    editor.set_selection_end(newpos);
    editor.update();
}

void pattern_editor::select_right(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_subcol(editor.pos(), 1);
    editor.set_selection_end(newpos);
    editor.update();
}


void pattern_editor::select_first_row(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_row(editor.pos(), -hugeor);
    editor.set_selection_end(newpos);
    editor.update();
}

void pattern_editor::select_last_row(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_row(editor.pos(), hugeor);
    editor.set_selection_end(newpos);
    editor.update();
}

void pattern_editor::select_first_col(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_subcol(editor.pos(), -hugeor);
    editor.set_selection_end(newpos);
    editor.update();
}

void pattern_editor::select_last_col(pattern_editor &editor) {
    auto newpos = editor.pos_move_by_subcol(editor.pos(), hugeor);
    editor.set_selection_end(newpos);
    editor.update();
}




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

void pattern_editor::clear_selected_cells(pattern_editor &editor) {
    auto &corners = editor.corners;
    auto &upper_left   = corners.topleft;
    auto &bottom_right = corners.bottomright;
    auto pos = upper_left;

    auto &pattern = editor.renderer.Patterns[editor.active_pos.pattern];

    for (; pos.row <= bottom_right.row; ++pos.row) {
        for (pos.column = upper_left.column;
             pos.column <= bottom_right.column; ++pos.column)
        {
            for (elem_t elem = ElemNote; elem < ElemMax; ++elem) {
                pos.subcolumn = elem;
                if (pos_in_rect(corners, pos)) {
                    modevent_t *evt = pattern.GetpModCommand(pos.row,
                                                             pos.column);
                    clear_at(evt, elem);
                }
            }
        }
    }

    editor.update();
}

void pattern_editor::delete_row(pattern_editor &editor) {
    auto &pattern = editor.renderer.Patterns[editor.active_pos.pattern];
    auto &pos     = editor.pos();
    auto numrows  = pattern.GetNumRows();

    if (numrows > 2) {
        for (size_t row = pos.row; row <= numrows - 2; ++row) {
            auto evt = pattern.GetpModCommand(row, pos.column);
            auto nextevt = pattern.GetpModCommand(row + 1, pos.column);
        }
    }
    if (numrows > 1) {
        pattern.GetpModCommand(numrows - 1, pos.column)->Clear();
    }

    editor.update();
}

void pattern_editor::insert_row(pattern_editor &editor) {
    //TODO
}

void pattern_editor::insert_note(pattern_editor &editor, uint8_t octave,
    uint8_t tone_number)
{
    editor.collapse_selection();
    auto evt = editor.active_event();
    auto newcmd = *evt;

    newcmd.note = 1 + tone_number + 12 * (octave + editor.base_octave);

    *evt = newcmd;
    editor.update();
}

void pattern_editor::insert_instr(pattern_editor &editor, uint8_t digit) {
    editor.collapse_selection();
    auto evt = editor.active_event();
    auto newcmd = *evt;

    instr_t instr = (newcmd.instr % 10) * 10 + digit;
    newcmd.instr = instr;

    *evt = newcmd;
    editor.update();
}

void pattern_editor::insert_volparam(pattern_editor &editor, uint8_t digit) {
    editor.collapse_selection();
    auto evt = editor.active_event();
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
    editor.update();
}

void pattern_editor::insert_volcmd(pattern_editor &editor, volcmd_t cmd) {
    editor.collapse_selection();
    auto evt = editor.active_event();
    auto newcmd = *evt;

    newcmd.volcmd = cmd;

    *evt = newcmd;
    editor.update();
}

void pattern_editor::insert_cmd(pattern_editor &editor, cmd_t cmd) {
    editor.collapse_selection();
    auto evt = editor.active_event();
    auto newcmd = *evt;

    newcmd.command = cmd;

    *evt = newcmd;
    editor.update();
}

void pattern_editor::insert_cmdparam(pattern_editor &editor, uint8_t digit) {
    editor.collapse_selection();
    auto evt = editor.active_event();
    auto newcmd = *evt;

    param_t param = (newcmd.param % 0x10) * 0x10 + digit;
    newcmd.param = param;

    *evt = newcmd;
    editor.update();
}












}
}
}