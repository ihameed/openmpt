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

struct color_spec_t {
    colors_t::colortype_t key;
    colorpair_t value;
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

static const color_spec_t it_colors[] = {
    { colors_t::Normal,             { QColor("#000000"), QColor("#00e000") } },
    { colors_t::Selected,           { QColor("#e0e0e0"), QColor("#000000") } },
    { colors_t::PlayCursor,         { QColor("#808000"), QColor("#00e000") } },
    { colors_t::CurrentRow,         { QColor("#707070"), QColor("#00e000") } },
    { colors_t::HighlightPrimary,   { QColor("#406840"), QColor("#ffffff") } },
    { colors_t::HighlightSecondary, { QColor("#2e482e"), QColor("#ffffff") } },
    { colors_t::Note,               { QColor("#ffffff"), QColor("#00ff00") } },
    { colors_t::Instrument,         { QColor("#ffffff"), QColor("#ffff00") } },
    { colors_t::Volume,             { QColor("#ffffff"), QColor("#00ff00") } },
    { colors_t::Panning,            { QColor("#ffffff"), QColor("#00ffff") } },
    { colors_t::Pitch,              { QColor("#ffffff"), QColor("#ffff00") } },
    { colors_t::Globals,            { QColor("#ffffff"), QColor("#ff4040") } },
};

static const color_spec_t ft2_colors[] = {
    { colors_t::Normal,             { QColor("#000000"), QColor("#e0e0e0") } },
    { colors_t::Selected,           { QColor("#4040a0"), QColor("#f0f050") } },
    { colors_t::PlayCursor,         { QColor("#505070"), QColor("#e0e040") } },
    { colors_t::CurrentRow,         { QColor("#707070"), QColor("#f0f0f0") } },
    { colors_t::HighlightPrimary,   { QColor("#404080"), QColor("#ffffff") } },
    { colors_t::HighlightSecondary, { QColor("#2e2e58"), QColor("#ffffff") } },
    { colors_t::Note,               { QColor("#ffffff"), QColor("#e0e040") } },
    { colors_t::Instrument,         { QColor("#ffffff"), QColor("#ffff00") } },
    { colors_t::Volume,             { QColor("#ffffff"), QColor("#00ff00") } },
    { colors_t::Panning,            { QColor("#ffffff"), QColor("#00ffff") } },
    { colors_t::Pitch,              { QColor("#ffffff"), QColor("#ffff00") } },
    { colors_t::Globals,            { QColor("#ffffff"), QColor("#ff4040") } },
};

static const color_spec_t old_mpt_colors[] = {
    { colors_t::Normal,             { QColor("#ffffff"), QColor("#000000") } },
    { colors_t::Selected,           { QColor("#000000"), QColor("#ffffff") } },
    { colors_t::PlayCursor,         { QColor("#ffff80"), QColor("#000000") } },
    { colors_t::CurrentRow,         { QColor("#c0c0c0"), QColor("#000000") } },
    { colors_t::HighlightPrimary,   { QColor("#e0e8e0"), QColor("#ffffff") } },
    { colors_t::HighlightSecondary, { QColor("#f2f6f2"), QColor("#ffffff") } },
    { colors_t::Note,               { QColor("#ffffff"), QColor("#000080") } },
    { colors_t::Instrument,         { QColor("#ffffff"), QColor("#000080") } },
    { colors_t::Volume,             { QColor("#ffffff"), QColor("#008000") } },
    { colors_t::Panning,            { QColor("#ffffff"), QColor("#008080") } },
    { colors_t::Pitch,              { QColor("#ffffff"), QColor("#808000") } },
    { colors_t::Globals,            { QColor("#ffffff"), QColor("#800000") } },
};

static const color_spec_t buzz_colors[] = {
    { colors_t::Normal,             { QColor("#e1dbd0"), QColor("#3a3427") } },
    { colors_t::Selected,           { QColor("#000000"), QColor("#ddd7cc") } },
    { colors_t::PlayCursor,         { QColor("#a9997a"), QColor("#000000") } },
    { colors_t::CurrentRow,         { QColor("#c0b49e"), QColor("#000000") } },
    { colors_t::HighlightPrimary,   { QColor("#cec5b5"), QColor("#ffffff") } },
    { colors_t::HighlightSecondary, { QColor("#d8d1c3"), QColor("#ffffff") } },
    { colors_t::Note,               { QColor("#ffffff"), QColor("#00005b") } },
    { colors_t::Instrument,         { QColor("#ffffff"), QColor("#005555") } },
    { colors_t::Volume,             { QColor("#ffffff"), QColor("#005e00") } },
    { colors_t::Panning,            { QColor("#ffffff"), QColor("#006868") } },
    { colors_t::Pitch,              { QColor("#ffffff"), QColor("#626200") } },
    { colors_t::Globals,            { QColor("#ffffff"), QColor("#660000") } },
};

template <size_t size>
colors_t generate_preset(const color_spec_t (&spec)[size]) {
    colors_t colors;
    for (size_t i = 0; i < size; ++i) {
        colors[spec[i].key].background = spec[i].value.background;
        colors[spec[i].key].foreground = spec[i].value.foreground;
    }
    return colors;
}

inline colors_t default_colors() {
    return generate_preset(it_colors);
}

Json::Value json_of_colors_t(const colors_t &);
colors_t colors_t_of_json(Json::Value &);


}
}
}