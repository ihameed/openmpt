#pragma once

#include <QtGui>
#include <QtWidgets/QtWidgets>
#include "../../tracker/types.h"

class module_renderer;

namespace modplug {
namespace gui {
namespace qt5 {

class app_config;

namespace order_internal {

class model: public QAbstractListModel {
    Q_OBJECT
public:
    model(module_renderer &renderer);

    int rowCount(const QModelIndex &) const override;
    QVariant data(const QModelIndex &, int) const override;

private:
    module_renderer &renderer;
};

}

class order_editor : public QWidget {
    Q_OBJECT
public:
    order_editor(module_renderer &renderer);
    QSize sizeHint() const override;

public slots:
    void display_context_menu(const QPoint &);
    void _set_active_pattern(const QModelIndex &, const QModelIndex &);

signals:
    void active_pattern_changed(modplug::tracker::patternindex_t);

private:

    QVBoxLayout main_layout;
    QListView order_list;

    QMenu context_menu;
    /*
    QAction insert_pattern;
    QAction remove_pattern;
    QAction create_new_pattern;
    QAction duplicate_pattern;
    QAction copy_pattern;
    QAction paste_pattern;
    QAction paste_orders;
    QAction pattern_properties;
    QAction render_to_wave;
    */

    module_renderer &renderer;
    order_internal::model modelhooty;
};

}
}
}