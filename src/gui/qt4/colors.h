#pragma once

#include <QtGui>
#include <json/json.h>

namespace modplug {
namespace gui {
namespace qt4 {

struct colorpair_t {
    QColor foreground;
    QColor background;
};

struct colors_t {
    colorpair_t normal;
    colorpair_t selected;
    colorpair_t playcursor;
    colorpair_t currentrow;

    colorpair_t primary_highlight;
    colorpair_t secondary_highlight;

    colorpair_t note;
    colorpair_t instrument;
    colorpair_t volume;
    colorpair_t panning;
    colorpair_t pitch;
    colorpair_t globals;
};

colors_t default_colors();

Json::Value json_of_colors_t(const colors_t &);
colors_t colors_t_of_json(Json::Value &);


}
}
}