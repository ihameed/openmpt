#include "stdafx.h"

#include "app_config.h"
#include "pattern_editor_aux.h"
#include "../../legacy_soundlib/Sndfile.h"

using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt5 {

pattern_editor_tab::pattern_editor_tab(
    module_renderer &renderer,
    app_config &config
) : renderer(renderer)
  , config(config)
  , orderedit(renderer)
  , editor(
        renderer,
        config.pattern_keymap(),
        config.it_pattern_keymap(),
        config.xm_pattern_keymap(),
        config.colors()
    )
{
    setLayout(&main_layout);
    main_layout.addLayout(&top_layout);
    main_layout.addWidget(&editor);

    top_layout.addWidget(&pattern_tool_bar);
    top_layout.addLayout(&pattern_info_layout);
    top_layout.addLayout(&order_layout);
    order_layout.addWidget(&orderedit);

    QIcon icon_new(":/openmpt/icons/entypo/list-add.svg");
    QIcon icon_record(":/openmpt/icons/entypo/record.svg");
    QIcon icon_play_pattern(":/openmpt/icons/entypo/derived/play-cw90.svg");

    new_pattern.setIcon(icon_new);
    new_pattern.setText("New Pattern");

    play_pattern_from_cursor.setIcon(icon_play_pattern);
    play_pattern_from_cursor.setText("Play Pattern From Cursor");

    play_pattern_from_start.setText("B");
    stop.setText("C");
    play_row.setText("D");

    record.setIcon(icon_record);
    record.setText("Record");

    pattern_tool_bar.addWidget(&new_pattern);
    pattern_tool_bar.addWidget(&play_pattern_from_cursor);
    pattern_tool_bar.addWidget(&play_pattern_from_start);
    pattern_tool_bar.addWidget(&stop);
    pattern_tool_bar.addWidget(&play_row);
    pattern_tool_bar.addWidget(&record);
    pattern_tool_bar.setStyleSheet("QToolBar { border: 0px; }");

    pattern_info_layout.addWidget(new QComboBox);
    pattern_info_layout.addWidget(new QLabel("Row Spacing"));
    pattern_info_layout.addWidget(new QSpinBox());
    pattern_info_layout.addWidget(new QCheckBox("Loop Pattern"));
    pattern_info_layout.addWidget(new QCheckBox("Follow Song"));
    pattern_info_layout.addWidget(new QLabel("Pattern Name"));
    pattern_info_layout.addWidget(new QLineEdit());

    connect(
        &orderedit, &order_editor::active_pattern_changed,
        &editor, &pattern_editor::set_active_order
    );
}


}
}
}
