#include "stdafx.h"

#include "pervasives/pervasives.hpp"
#include "main_window.hpp"
#include "document_window.hpp"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt5 {

main_window::main_window() {
    QIcon window_icon(":/openmpt/icons/nobu-icons/mptgloss/128.png");
    setWindowIcon(window_icon);
    setWindowTitle("OpenMPT");
    setCentralWidget(&mdi_area);

    file = menuBar()->addMenu("File");
}

void main_window::add_document_window(document_window *docwnd) {
    auto subwindow = mdi_area.addSubWindow(docwnd);
    internal_windows[docwnd] = subwindow;
    docwnd->show();
}

void main_window::remove_document_window(document_window *docwnd) {
    auto subwindow = internal_windows.find(docwnd);
    if (subwindow == internal_windows.end()) { return; }
    mdi_area.removeSubWindow(subwindow->second);
    internal_windows.erase(docwnd);
}

}
}
}
