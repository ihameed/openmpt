#pragma once

#include <QtGui>

class module_renderer;

namespace modplug {
namespace gui {
namespace qt4 {

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