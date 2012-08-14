#include "stdafx.h"

#include "colors.h"
#include "..\pervasives\pervasives.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt4 {

Json::Value json_of_colors_t(const colors_t &colors) {
    Json::Value root;
    for_each(color_names, [&] (color_name_assoc_t names) {
        auto color = colors[names.key];
        root[names.value]["background"] = color.background.name()
                                               .toAscii().data();
        root[names.value]["foreground"] = color.foreground.name()
                                               .toAscii().data();
    });
    return root;
}

colors_t colors_t_of_json(const Json::Value &value) {
    colors_t colors;
    for (auto it = value.begin(); it != value.end(); ++it) {
        auto key = key_of_value(color_names, it.key().asCString(),
                                colors_t::Normal, strcmp);
        auto extract_color = [&](const char *type) -> QColor {
            QColor ret;
            auto value = *it;
            if (value.isMember(type) && value[type].isString()) {
                ret.setNamedColor(value[type].asCString());
            }
            return ret;
        };
        colors[key].background = extract_color("background");
        colors[key].foreground = extract_color("foreground");
    }
    return colors;
}

}
}
}