#include "stdafx.h"

#include "app_config.h"
#include "pattern_editor_aux.h"

namespace modplug {
namespace gui {
namespace qt4 {


order_editor::order_editor() {
    setLayout(&main_layout);

    main_layout.addWidget(&order_list);
    main_layout.setMargin(0);
    main_layout.setSpacing(0);
    order_list.setFlow(QListView::LeftToRight);
    order_list.addItem("0");
    order_list.addItem("2");
    order_list.addItem("1");
    order_list.addItem("6");


    order_list.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

pattern_editor_tab::pattern_editor_tab(
    module_renderer &renderer,
    app_config &config
) : renderer(renderer), config(config),
    editor(
        renderer,
        config.pattern_keymap(),
        config.it_pattern_keymap(),
        config.xm_pattern_keymap(),
        config.colors()
    )
{
    setLayout(&main_layout);
    main_layout.addWidget(&orderedit);
    main_layout.addWidget(&editor);
}


}
}
}