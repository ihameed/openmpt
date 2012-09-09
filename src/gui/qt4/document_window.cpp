#include "stdafx.h"

#include "app_config.h"
#include "document_window.h"
#include "comment_view.h"
#include "pattern_editor.h"
#include "..\MainFrm.h"

namespace modplug {
namespace gui {
namespace qt4 {

document_window::document_window(module_renderer *renderer,
                                 app_config &config,
                                 QWidget *parent
) : QDialog(parent), global_config(config)
{
    auto layout = new QHBoxLayout;
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setMargin(0);

    layout->addWidget(&tab_bar);

    editor = new pattern_editor(
        *renderer,
        global_config.pattern_keymap(),
        global_config.colors()
    );

    comments = new comment_view(renderer);

    tab_bar.addTab(editor, "Patterns");
    tab_bar.addTab(comments, "Comments");

    QObject::connect(
        &global_config, SIGNAL(colors_changed()),
        this, SLOT(config_colors_changed())
    );
}

void document_window::test_notification(MPTNOTIFICATION * pnotify) {
    /*
    DEBUG_FUNC(
        "latency = %d; pos = %d; order = %d, row = %d",
        pnotify->dwLatency,
        pnotify->dwPos,
        pnotify->nOrder,
        pnotify->nRow
    );
    */
    const player_position_t position(
        pnotify->nOrder,
        pnotify->nPattern,
        pnotify->nRow
    );
    editor->update_playback_position(position);
}

void document_window::config_colors_changed() {
    editor->update_colors(global_config.colors());
}


}
}
}