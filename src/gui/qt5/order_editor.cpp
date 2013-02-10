#include "stdafx.h"

#include "order_editor.h"
#include "../../legacy_soundlib/Sndfile.h"

using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt5 {

order_editor::order_editor(module_renderer &renderer)
  : renderer(renderer),
    modelhooty(renderer)
{
    setLayout(&main_layout);

    order_list.setFlow(QListView::LeftToRight);
    order_list.setModel(&modelhooty);
    order_list.setContextMenuPolicy(Qt::CustomContextMenu);

    order_list.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    order_list.setUniformItemSizes(true);
    order_list.setMovement(QListView::Free);
    order_list.setFrameStyle(QFrame::Sunken | QFrame::Box);
    order_list.setStyleSheet("QListView::item { width: 2em; text-align: center; }");

    main_layout.addWidget(&order_list);
    main_layout.setMargin(0);
    main_layout.setSpacing(0);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    connect(
        &order_list, &QListView::customContextMenuRequested,
        this, &order_editor::display_context_menu
    );

    auto smodel = order_list.selectionModel();

    connect(
        smodel, &QItemSelectionModel::currentChanged,
        this, &order_editor::_set_active_pattern
    );
}

QSize order_editor::sizeHint() const {
    return QSize(10, 40);
}

void order_editor::display_context_menu(const QPoint &point) {
    DEBUG_FUNC("HUOA (%d, %d)", point.x(), point.y());
}

void order_editor::_set_active_pattern(const QModelIndex &index, const QModelIndex &) {
    patternindex_t idx = patternindex_t(index.row());
    emit active_pattern_changed(idx);
}




namespace order_internal {

model::model(
    module_renderer &renderer
) : renderer(renderer)
{ }

int model::rowCount(const QModelIndex &parent) const {
    //renderer.m_nMaxOrderPosition;
    return renderer.Order.GetLength();
}

QVariant pretty_pattern_index(const modsequence_t &seq, size_t idx) {
    const patternindex_t pidx = seq[idx];
    const auto ignore  = seq.GetIgnoreIndex();
    const auto invalid = seq.GetInvalidPatIndex();
    if (pidx == ignore) {
        return QVariant("++");
    } else if (pidx == invalid) {
        return QVariant("--");
    } else {
        return QVariant(pidx);
    }
}

QVariant model::data(const QModelIndex &index, int role) const {
    const auto idx = index.row();
    switch (role) {
    case Qt::DisplayRole:    return pretty_pattern_index(renderer.Order, idx);
    case Qt::BackgroundRole: return qApp->palette().base();
    default:                 return QVariant();
    }
}

} // namespace order_internal


}
}
}
