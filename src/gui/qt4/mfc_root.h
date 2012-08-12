#include <QtGui>
#include "qwinwidget.h"

class CMainFrame;

namespace modplug {
namespace gui {
namespace qt4 {

class app_config;

class mfc_root : public QWinWidget {
    Q_OBJECT

public:
    mfc_root(app_config &settings, CMainFrame &mfc_parent);

public slots:
    void update_audio_settings();

private:
    app_config &settings;
    CMainFrame &mainwnd;
};

}
}
}