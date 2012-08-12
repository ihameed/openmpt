#pragma once
#include <QtGui>

#include "../../audioio/paudio.h"
#include "../../pervasives/pervasives.h"

namespace modplug {
namespace gui {
namespace qt4 {


class config_page : public QWidget, private modplug::pervasives::noncopyable {
    Q_OBJECT
public:
    virtual void refresh() = 0;
    virtual void apply_changes() = 0;
};

class config_treeview;
class app_config;

class config_dialog : public QDialog, private modplug::pervasives::noncopyable {
    Q_OBJECT
public:
    config_dialog(app_config &, QWidget *);

public slots:
    void change_page();
    void button_clicked(QAbstractButton *);

    virtual void setVisible(bool);

private:
    config_treeview *category_list;
    QStackedWidget *category_pager;

    QDialogButtonBox buttons;
    std::vector<config_page *> pages;
};


}
}
}