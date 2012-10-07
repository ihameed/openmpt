#include "stdafx.h"

#include "app_config.h"
#include "pattern_editor_aux.h"
#include "../../legacy_soundlib/Sndfile.h"

namespace modplug {
namespace gui {
namespace qt4 {

order_editor_hibbety_jibbety::order_editor_hibbety_jibbety(
    module_renderer &renderer
) : renderer(renderer)
{ }

int order_editor_hibbety_jibbety::rowCount(const QModelIndex &parent) const {
    //renderer.m_nMaxOrderPosition;
    return renderer.Order.GetLength();
}

QVariant order_editor_hibbety_jibbety::data(
    const QModelIndex &index, int role) const
{
    //DEBUG_FUNC("huoaoho (%d, %d); role %d", index.row(), index.column(), role);
    switch (role) {
    case Qt::DisplayRole: return QVariant("hooby");
    case Qt::BackgroundRole: return qApp->palette().base();
    default: return QVariant();
    }
}

order_editor::order_editor(module_renderer &renderer)
  : renderer(renderer),
    modelhooty(renderer)
{
    setLayout(&main_layout);

    main_layout.addWidget(&order_list);
    main_layout.setMargin(0);
    main_layout.setSpacing(0);
    order_list.setFlow(QListView::LeftToRight);
    order_list.setModel(&modelhooty);


    order_list.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

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
    main_layout.addWidget(&orderedit);
    main_layout.addWidget(&editor);
}


}
}
}