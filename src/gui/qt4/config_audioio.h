#pragma once
#include <QtGui>

namespace modplug {
namespace gui {
namespace qt4 {

class config_context;

class config_audioio_main : public QWidget {
public:
    config_audioio_main(config_context &, QWidget * = 0);
};

}
}
}