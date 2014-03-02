#include <QtGui>
#include "qwinwidget.h"
#include "main_window.hpp"

class CMainFrame;

namespace modplug {
namespace gui {
namespace qt5 {

class app_config;

class mfc_root : public QWinWidget {
    Q_OBJECT

public:
    mfc_root(app_config &settings, CMainFrame &mfc_parent);

    app_config &settings;
    CMainFrame &mainwnd;
    main_window mainwindow;

public slots:
    void update_audio_settings();
};

}
}
}
