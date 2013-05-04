#pragma once

#include <QMainWindow>
#include <QMdiArea>
#include <unordered_map>

namespace modplug {
namespace gui {
namespace qt5 {

class document_window;

class main_window : public QMainWindow {
    Q_OBJECT
public:
    main_window();

    void add_document_window(document_window *);
    void remove_document_window(document_window *);

    QMdiArea mdi_area;

    QMenu *file;

    std::unordered_map<document_window *, QMdiSubWindow *> internal_windows;
};

}
}
}
