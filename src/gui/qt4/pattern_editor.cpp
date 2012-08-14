#include "stdafx.h"

#include "../../Resource.h"
#include "../../pervasives/pervasives.h"

#include "pattern_editor.h"
#include "pattern_editor_column.h"
#include "util.h"

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

    font = load_bmp_resource(resource, instance, hdc)
          .convertToFormat(QImage::Format_MonoLSB);

    ReleaseDC(nullptr, hdc);

    update_colors(colors);
}

void pattern_editor::update_colors(const colors_t &newcolors) {
    colors = newcolors;

    update();
}

void pattern_editor::update_playback_position(
    const editor_position_t &position)
{
    playback_pos = position;
    if (follow_playback) {
        active_pos = playback_pos;
    }
    update();
}

void pattern_editor::paintEvent(QPaintEvent *evt) {
    QPainter painter(this);
    painter.setRenderHints( QPainter::Antialiasing
                          | QPainter::TextAntialiasing
                          | QPainter::SmoothPixmapTransform
                          | QPainter::HighQualityAntialiasing
                          , false);
    painter.setViewTransformEnabled(false);

    const QRect &clipping_rect = evt->rect();

    chnindex_t channel_count = renderer.GetNumChannels();
    note_column notehomie;

    draw_state state = {
        renderer,

        painter,
        clipping_rect,

        font_metrics.width,
        font_metrics.height,

        font,
        font_metrics,

        playback_pos,
        active_pos,

        colors
    };

    for (chnindex_t idx = 0; idx < channel_count; ++idx) {
        notehomie.draw_header(state, idx);
        notehomie.draw_column(state, idx);
    }
}


}
}
}