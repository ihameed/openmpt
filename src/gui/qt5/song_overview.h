#pragma once

#include <QtGui>
#include <QtWidgets/QtWidgets>

class module_renderer;

namespace modplug {
namespace gui {
namespace qt5 {

class song_overview : public QWidget {
    Q_OBJECT
public:
    song_overview(module_renderer &);

private:
    module_renderer &renderer;
    QListWidget samples;
    QListWidget instruments;
};

}
}
}