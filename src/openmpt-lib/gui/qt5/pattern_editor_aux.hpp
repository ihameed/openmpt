#pragma once

#include <QtGui>
#include "pattern_editor.hpp"
#include "order_editor.hpp"

#include "keymap.hpp"

class module_renderer;

namespace modplug {
namespace gui {
namespace qt5 {

class app_config;


class pattern_editor_tab : public QWidget {
    Q_OBJECT
public:
    pattern_editor_tab(module_renderer &renderer, app_config &config);

    QVBoxLayout main_layout;

    QVBoxLayout top_layout;
    QHBoxLayout order_layout;
    QHBoxLayout pattern_info_layout;

    QToolBar pattern_tool_bar;
    QToolButton new_pattern;
    QToolButton play_pattern_from_cursor;
    QToolButton play_pattern_from_start;
    QToolButton stop;
    QToolButton play_row;
    QToolButton record;

    module_renderer &renderer;
    app_config &config;

    pattern_editor editor;
    order_editor orderedit;
};

}
}
}
