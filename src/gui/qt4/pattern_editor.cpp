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

pattern_editor_draw::pattern_editor_draw(
    module_renderer &renderer,
    const colors_t &colors
) :
    renderer(renderer),
    font_metrics(small_pattern_font)
{
    setFocusPolicy(Qt::ClickFocus);
    font_bitmap = load_font();
}

void pattern_editor_draw::initializeGL() {
    //DEBUG_FUNC("");
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY_EXT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    font_texture = bindTexture(font_bitmap);

    resizeGL(100, 100);

    glBindTexture(GL_TEXTURE_2D, font_texture);
}

void pattern_editor_draw::resizeGL(int width, int height) {
    //DEBUG_FUNC("width = %d, height = %d", width, height);
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

void pattern_editor_draw::paintGL() {
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

editor_position_t pattern_editor_draw::pos_move_by_row(
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

editor_position_t pattern_editor_draw::pos_move_by_subcol(
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

bool pattern_editor_draw::position_from_point(const QPoint &point,
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

void pattern_editor_draw::recalc_corners() {
    corners = normalize_selection(selection);
}

bool pattern_editor_draw::set_pos_from_point(const QPoint &point,
                                        editor_position_t &pos)
{
    editor_position_t newpos;
    if (position_from_point(point, newpos)) {
        pos = newpos;
        return true;
    }
    return false;
}

void pattern_editor_draw::set_selection_start(const QPoint &point) {
    if (set_pos_from_point(point, selection.start)) {
        recalc_corners();
    }
}

void pattern_editor_draw::set_selection_start(const editor_position_t &pos) {
    selection.start = pos;
    recalc_corners();
}

void pattern_editor_draw::set_selection_end(const QPoint &point) {
    if (set_pos_from_point(point, selection.end)) {
        recalc_corners();
    }
}

void pattern_editor_draw::set_selection_end(const editor_position_t &pos) {
    selection.end = pos;
    recalc_corners();
}

void pattern_editor_draw::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        is_dragging = true;
        set_selection_start(event->pos());
        set_selection_end(event->pos());
        update();
    }
}

void pattern_editor_draw::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton && is_dragging) {
        set_selection_end(event->pos());
        update();
    }
}

void pattern_editor_draw::mouseReleaseEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        is_dragging = false;
        set_selection_end(event->pos());
        update();
    }
}

void pattern_editor_draw::move_to(const editor_position_t &target) {
    selection.start = target;
    selection.end   = target;
    recalc_corners();
    update();
}

const editor_position_t &pattern_editor_draw::pos() const {
    return selection.end;
}

keycontext_t pattern_editor_draw::keycontext() const{
    switch (pos().subcolumn) {
    case ElemNote:  return ContextNoteCol;
    case ElemInstr: return ContextInstrCol;
    case ElemVol:   return ContextVolCol;
    case ElemCmd:   return ContextCmdCol;
    case ElemParam: return ContextParamCol;
    default:        return ContextGlobal;
    }
}

void pattern_editor_draw::collapse_selection() {
    auto &newpos = selection.start = selection.end = pos();
    recalc_corners();
    move_to(newpos);
}

CPattern *pattern_editor_draw::active_pattern() {
    auto patternidx = active_pos.pattern;
    return &renderer.Patterns[patternidx];
}

modevent_t *pattern_editor_draw::active_event() {
    return active_pattern()->GetpModCommand(pos().row, pos().column);
}










QSize pattern_editor_draw::pattern_size() {
    chnindex_t channel_count = renderer.GetNumChannels();

    int left = 0;

    for (chnindex_t idx = 0; idx < channel_count; ++idx) {
        note_column notehomie(left, idx, font_metrics);
        left += notehomie.width();
    }

    auto pattern = active_pattern();
    auto height = font_metrics.height * pattern->GetNumRows()
                + column_header_height + 1;

    QSize ret(left, height);
    return ret;
}

const normalized_selection_t & pattern_editor_draw::selection_corners() {
    return corners;
}















pattern_editor::pattern_editor(
    module_renderer &renderer,
    const pattern_keymap_t &keymap,
    const pattern_keymap_t &it_keymap,
    const pattern_keymap_t &xm_keymap,
    const colors_t &colors
) :
    keymap(keymap),
    it_keymap(it_keymap),
    xm_keymap(xm_keymap),
    follow_playback(true),
    draw(renderer, colors)
{
    draw.setParent(this->viewport());
    draw.move(0, 0);

    verticalScrollBar()->setSingleStep(1);
    horizontalScrollBar()->setSingleStep(1);

    set_base_octave(4);
    update_colors(colors);
}

void pattern_editor::update_colors(const colors_t &newcolors) {
    draw.colors = newcolors;
    /*
    auto &bgc = colors[colors_t::Normal].background;
    makeCurrent();
    glClearColor(bgc.redF() * 0.75, bgc.greenF() * 0.75, bgc.blueF() * 0.75, 0.5);
    */
    draw.updateGL();
}

void pattern_editor::update_playback_position(
    const player_position_t &position)
{
    draw.playback_pos = position;
    if (follow_playback) {
        draw.active_pos = draw.playback_pos;
    }
    draw.updateGL();
}

void pattern_editor::set_base_octave(uint8_t octave) {
    base_octave = octave;
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
    auto context_key = key_t(modifiers, event->key(), draw.keycontext());
    auto pattern_key = key_t(modifiers, event->key());

    if (invoke_key(keymap,    context_key)) return;
    if (invoke_key(it_keymap, context_key)) return;
    if (invoke_key(keymap,    pattern_key)) return;

    QAbstractScrollArea::keyPressEvent(event);
}

void pattern_editor::resizeEvent(QResizeEvent *event) {
    draw.resize(event->size());
    auto sz = draw.pattern_size();

    const auto pattern = draw.active_pattern();

    auto slider_height = sz.height() - draw.size().height();
    auto slider_width  = sz.width()  - draw.size().width();
    if (slider_height > 0) {
        verticalScrollBar()->setRange(0, slider_height);
        verticalScrollBar()->setPageStep(draw.size().height());
        verticalScrollBar()->show();
    } else {
        verticalScrollBar()->setRange(0, 0);
        verticalScrollBar()->setPageStep(draw.size().height());
    }
    if (slider_width > 0) {
        horizontalScrollBar()->setRange(0, slider_width);
        horizontalScrollBar()->setPageStep(draw.size().width());
        horizontalScrollBar()->show();
    } else {
        horizontalScrollBar()->setRange(0, 0);
        horizontalScrollBar()->setPageStep(draw.size().width());
    }
}


}
}
}