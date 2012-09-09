#pragma once

#include <QtGui>

struct MPTNOTIFICATION;
class module_renderer;

namespace modplug {
namespace gui {
namespace qt4 {

class pattern_editor;
class app_config;
class comment_editor;

class document_window : public QDialog {
    Q_OBJECT
public:
    document_window(module_renderer *, app_config &, QWidget *);
    void test_notification(MPTNOTIFICATION *pnotify);

public slots:
    void config_colors_changed();

private:
    pattern_editor *editor;
    comment_editor *comments;
    app_config &global_config;

    QTabWidget tab_bar;
};

}
}
}