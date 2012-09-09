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

pattern_editor::pattern_editor(module_renderer &renderer,
                               const colors_t &colors) :
    renderer(renderer),
    font_metrics(small_pattern_font),
    follow_playback(true)
{
    setFocusPolicy(Qt::ClickFocus);
    font_bitmap = load_font();
    update_colors(colors);
}

void pattern_editor::update_colors(const colors_t &newcolors) {
    colors = newcolors;
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

        selection_start,
        selection_end,
        false
    };

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

void pattern_editor::set_selection(const QPoint &point, editor_position_t &pos) {
    editor_position_t newpos;
    if (position_from_point(point, newpos)) {
        pos = newpos;
    }
}

void pattern_editor::set_selection_start(const QPoint &point) {
    set_selection(point, selection_start);
}

void pattern_editor::set_selection_end(const QPoint &point) {
    set_selection(point, selection_end);
}

void pattern_editor::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        is_dragging = true;
        set_selection_start(event->pos());
        set_selection_end(event->pos());
        repaint();
    }
}

void pattern_editor::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton && is_dragging) {
        set_selection_end(event->pos());
        repaint();
    }
}

void pattern_editor::mouseReleaseEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        is_dragging = false;
        set_selection_end(event->pos());
        repaint();
    }
}

void pattern_editor::keyPressEvent(QKeyEvent *event) {
    init_action_maps();
    pattern_keymap_t map = default_pattern_keymap();
    auto key = key_t(event->modifiers(), event->key());
    invoke(map, key, *this);
}

void pattern_editor::move_to(const editor_position_t &target) {
    selection_start = target;
    selection_end   = target;
    repaint();
}

const editor_position_t &pattern_editor::pos() const {
    return selection_end;
}



void pattern_editor::move_up(pattern_editor &editor) {
    auto pos = editor.pos();
    --pos.row;
    editor.move_to(pos);
}

void pattern_editor::move_down(pattern_editor &editor) {
    auto pos = editor.pos();
    ++pos.row;
    editor.move_to(pos);
}

void pattern_editor::move_right(pattern_editor &editor) {
    auto pos = editor.pos();
    ++pos.subcolumn;
    if (pos.subcolumn >= ElemMax) {
        ++pos.column;
        pos.subcolumn = ElemNote;
    }
    editor.move_to(pos);
}

void pattern_editor::move_left(pattern_editor &editor) {
    auto pos = editor.pos();
    if (pos.subcolumn == ElemNote) {
        --pos.column;
        pos.subcolumn = ElemMax;
    }
    --pos.subcolumn;
    editor.move_to(pos);
}


}
}
}