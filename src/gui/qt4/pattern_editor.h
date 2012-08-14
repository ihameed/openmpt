#pragma once

#include <QtGui>
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

struct pattern_selection_t {
    uint32_t row;
    uint32_t column;
    sub_column_t sub_column;
};

class pattern_editor : public QWidget {
    Q_OBJECT
    typedef modplug::tracker::rowindex_t rowindex_t;
public:
    pattern_editor(module_renderer &renderer, const colors_t &colors);

    void update_colors(const colors_t &colors);
    void update_playback_row(rowindex_t playback_row);

protected:
    virtual void paintEvent(QPaintEvent *) override;

private:
    pattern_selection_t selection_start;
    pattern_selection_t selection_end;

    rowindex_t playback_row;

    module_renderer &renderer;

    bool font_loaded;
    const pattern_font_metrics_t &font_metrics;
    QImage font;
    colors_t colors;
};


}
}
}