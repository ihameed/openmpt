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
) : renderer(renderer),
    config(config),
    orderedit(renderer),
    editor(
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

    QFontDatabase fontdb;
    fontdb.addApplicationFont(":/openmpt/icons/entypo/entypo.ttf");

    foreach(QString s, fontdb.families()) {
        DEBUG_FUNC("fontdb font: '%s'", s.toLatin1().constData());
    }

    QFont entypo("Entypo",16 );
    //QFont entypo("wingdings", 20);

    auto ico_new = QString::fromUtf8("\xf0\x9f\x93\x84");
    auto ico_rec = QString::fromUtf8("\xe2\x97\x8f");
    //auto huoa = "t";

    new_pattern.setFont(entypo);
    new_pattern.setText(ico_new);

    play_pattern_from_cursor.setText("A");
    play_pattern_from_start.setText("B");
    stop.setText("C");
    play_row.setText("D");

    record.setFont(entypo);
    record.setText(ico_rec);

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
