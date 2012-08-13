#pragma once

#include <QtGui>
#include <json/json.h>

namespace modplug {
namespace gui {
namespace qt4 {

struct colorpair_t {
    QColor background;
    QColor foreground;
};

struct colors_t {
    enum colortype_t {
        Normal = 0,
        Selected,
        PlayCursor,
        CurrentRow,
        HighlightPrimary,
        HighlightSecondary,
        Note,
        Instrument,
        Volume,
        Panning,
        Pitch,
        Globals,
        MAX_COLORS
    };

    colorpair_t colors[MAX_COLORS];

    colorpair_t &operator [] (const colortype_t type) {
        return colors[type];
    };

    const colorpair_t &operator [] (const colortype_t type) const {
        return colors[type];
    };
};

struct color_name_assoc_t {
    colors_t::colortype_t key;
    const char *value;
};

static const color_name_assoc_t color_names[] = {
    { colors_t::Normal,             "normal" },
    { colors_t::Selected,           "selected" },
    { colors_t::PlayCursor,         "playcursor" },
    { colors_t::CurrentRow,         "currentrow" },
    { colors_t::HighlightPrimary,   "highlightprimary" },
    { colors_t::HighlightSecondary, "highlightsecondary" },
    { colors_t::Note,               "note" },
    { colors_t::Instrument,         "note" },
    { colors_t::Volume,             "volume" },
    { colors_t::Panning,            "panning" },
    { colors_t::Pitch,              "pitch" },
    { colors_t::Globals,            "globals" },
};

colors_t default_colors();
colors_t preset_it();
colors_t preset_ft2();
colors_t preset_old_mpt();
colors_t preset_buzz();

Json::Value json_of_colors_t(const colors_t &);
colors_t colors_t_of_json(Json::Value &);


}
}
}