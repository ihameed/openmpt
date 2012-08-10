#pragma once
#include <QtGui>

#include "../../audioio/paudio.h"

namespace modplug {
namespace gui {
namespace qt4 {


class config_context {
public:
    config_context(portaudio::System &pa_system) :
        pa_system(pa_system)
    { };

    portaudio::System &pa_system;
};

class config_page : public QWidget {
    Q_OBJECT
public:
    virtual void refresh() { };
};

class config_treeview;

class config_dialog : public QDialog {
    Q_OBJECT
public:
    config_dialog(config_context &, CMainFrame &, QWidget * = 0);

public slots:
    void change_page();

private:
    config_treeview *category_list;
    QStackedWidget *category_pager;

    QPushButton *button_ok;
    QPushButton *button_cancel;
    QPushButton *button_apply;
};


}
}
}