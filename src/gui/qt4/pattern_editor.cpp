#include "stdafx.h"

#include "../../Resource.h"
#include "../../pervasives/pervasives.h"

#include "pattern_editor.h"
#include "pattern_editor_column.h"
#include "util.h"

#include <GL/gl.h>
#include <GL/glu.h>

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt4 {

pattern_editor::pattern_editor(module_renderer &renderer,
                               const colors_t &colors) :
    renderer(renderer),
    font_metrics(small_pattern_font),
    follow_playback(true)
{
    auto resource = MAKEINTRESOURCE(IDB_PATTERNVIEW);
    auto instance = GetModuleHandle(nullptr);
    auto hdc      = GetDC(nullptr);

    font_bitmap = load_bmp_resource(resource, instance, hdc)
                 .convertToFormat(QImage::Format_MonoLSB);

    font_mask.convertFromImage(font_bitmap);

    ReleaseDC(nullptr, hdc);

    update_colors(colors);
}

void pattern_editor::update_colors(const colors_t &newcolors) {
    colors = newcolors;

    for (size_t i = 0; i < colors_t::MAX_COLORS; ++i) {
        auto &color = colors[(colors_t::colortype_t) i];
        auto &colorized_font = font[i];
        font_bitmap.setColor(1, color.foreground.rgba());
        font_bitmap.setColor(0, color.background.rgba());
        font_bitmap.setColor(0, qRgba(0xff, 0xff, 0xff, 0x00));
        font_bitmap.setColor(1, qRgba(0xff, 0xff, 0xff, 0xff));
        colorized_font.convertFromImage(font_bitmap);
        colorized_font.setMask(font_mask);
    };

    update();
}

void pattern_editor::update_playback_position(
    const editor_position_t &position)
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

    resizeGL(100, 100);
    for (size_t i = 0; i < colors_t::MAX_COLORS; ++i) {
        font_textures[i] = bindTexture(font[i]);
    }

    glBindTexture(GL_TEXTURE_2D, font_textures[0]);
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

        font_metrics.width,
        font_metrics.height,

        font_textures,
        font_metrics,
        colors_t::Normal,

        playback_pos,
        active_pos,

        colors
    };

    chnindex_t idx = 0;
    int painted_width = 0;

    for (; idx < channel_count && painted_width < width;
         ++idx, painted_width += font_metrics.width)
    {
        note_column notehomie(state);
        notehomie.draw_column(state, idx);
    }
}


}
}
}