#include "stdafx.h"

#include "colors.h"

namespace modplug {
namespace gui {
namespace qt4 {

Json::Value json_of_colors_t(const colors_t &) {
    return Json::Value();
}

colors_t colors_t_of_json(Json::Value &) {
    return default_colors();
}

colors_t default_colors() {
    colors_t colors;

    colors.normal.background              = QColor("#000000");
    colors.normal.foreground              = QColor("#00e000");

    colors.selected.background            = QColor("#e0e0e0");
    colors.selected.foreground            = QColor("#000000");

    colors.playcursor.background          = QColor("#808000");
    colors.playcursor.foreground          = QColor("#00e000");

    colors.currentrow.background          = QColor("#707070");
    colors.currentrow.foreground          = QColor("#00e000");

    colors.primary_highlight.background   = QColor("#406840");
    colors.secondary_highlight.background = QColor("#2e482e");

    colors.note.foreground                = QColor("#00ff00");
    colors.instrument.foreground          = QColor("#ffff00");
    colors.volume.foreground              = QColor("#00ff00");
    colors.panning.foreground             = QColor("#00ffff");
    colors.pitch.foreground               = QColor("#ffff00");
    colors.globals.foreground             = QColor("#ff4040");

    return colors;
}


}
}
}