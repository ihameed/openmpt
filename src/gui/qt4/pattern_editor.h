#pragma once

#include <QtGui>
#include <QGLWidget>
#include <cstdint>
#include "colors.h"
#include "pattern_bitmap_fonts.h"

#include "../../legacy_soundlib/Snd_defs.h"
#include "../../tracker/types.h"

class module_renderer;

namespace modplug {
namespace gui {
namespace qt4 {

enum sub_column_t {
    NOTE,
    INSTRUMENT,
    VOLUME,
    EFFECT,
    EFFECT_PARAMETER
};

struct editor_position_t {
    modplug::tracker::orderindex_t   order;
    modplug::tracker::patternindex_t pattern;
    modplug::tracker::rowindex_t     row;

    editor_position_t() : order(0), pattern(0),
                          row(modplug::tracker::RowIndexInvalid) { }

    editor_position_t(modplug::tracker::orderindex_t order,
                      modplug::tracker::patternindex_t pattern,
                      modplug::tracker::rowindex_t row)
                      : order(order), pattern(pattern), row(row) { }
};

struct pattern_selection_t {
    uint32_t row;
    uint32_t column;
    sub_column_t sub_column;
};

class pattern_editor : public QGLWidget {
    Q_OBJECT
public:
    pattern_editor(module_renderer &renderer, const colors_t &colors);

    void update_colors(const colors_t &colors);
    void update_playback_position(const editor_position_t &);

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int, int) override;

private:
    pattern_selection_t selection_start;
    pattern_selection_t selection_end;

    editor_position_t playback_pos;
    editor_position_t active_pos;
    bool follow_playback;

    module_renderer &renderer;

    const pattern_font_metrics_t &font_metrics;
    QImage font_bitmap;
    QPixmap font[colors_t::MAX_COLORS];
    QBitmap font_mask;
    colors_t colors;

    int width;
    int height;

    GLuint font_textures[colors_t::MAX_COLORS];
    QRect clipping_rect;
};


}
}
}