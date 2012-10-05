#pragma once

#include <QtGui>
#include "pattern_editor.h"

#include "keymap.h"

class module_renderer;

namespace modplug {
namespace gui {
namespace qt4 {

class app_config;

class order_editor : public QWidget {
    Q_OBJECT
public:
    order_editor();

    QVBoxLayout main_layout;

    QListWidget order_list;
};

class pattern_editor_tab : public QWidget {
    Q_OBJECT
public:
    pattern_editor_tab(module_renderer &renderer, app_config &config);

    QVBoxLayout main_layout;

    module_renderer &renderer;
    app_config &config;

    pattern_editor editor;
    order_editor orderedit;
    //pattern_editor_panel panel;
};

}
}
}