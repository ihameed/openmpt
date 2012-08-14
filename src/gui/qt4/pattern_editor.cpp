#include "stdafx.h"

#include "../../Resource.h"
#include "../../pervasives/pervasives.h"

#include "pattern_editor.h"
#include "pattern_editor_column.h"
#include "util.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt4 {

pattern_editor::pattern_editor(module_renderer &renderer,
                               const colors_t &colors) :
    renderer(renderer),
    colors(colors),
    font_metrics(small_pattern_font),
    font_loaded(false)
{ }

void pattern_editor::update_colors(const colors_t &colors) {
    this->colors = colors;
    repaint();
}

void pattern_editor::update_playback_row(modplug::tracker::rowindex_t playback_row) {
    this->playback_row = playback_row;
    repaint();
}

void pattern_editor::paintEvent(QPaintEvent *evt) {
    if (!font_loaded) {
        auto resource = MAKEINTRESOURCE(IDB_PATTERNVIEW);
        auto instance = GetModuleHandle(nullptr);
        auto hdc      = GetDC(nullptr);

        font = load_bmp_resource(resource, instance, hdc)
              .convertToFormat(QImage::Format_MonoLSB);

        ReleaseDC(nullptr, hdc);

        font_loaded = true;
    }

    QPainter painter(this);
    painter.setRenderHints( QPainter::Antialiasing
                          | QPainter::TextAntialiasing
                          | QPainter::SmoothPixmapTransform
                          | QPainter::HighQualityAntialiasing
                          , false);
    painter.setViewTransformEnabled(false);

    const QRect &clipping_rect = evt->rect();

    CHANNELINDEX channel_count = renderer.GetNumChannels();
    note_column notehomie;

    draw_state state = {
        renderer,
        painter,
        clipping_rect,
        font_metrics.width,
        font_metrics.height,
        font,
        font_metrics,
        0,
        playback_row,
        colors
    };

    for (CHANNELINDEX idx = 0; idx < channel_count; ++idx) {
        notehomie.draw_header(state, idx);
        notehomie.draw_column(state, idx);
    }
}


}
}
}