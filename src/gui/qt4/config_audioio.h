#pragma once
#include <QtGui>

namespace modplug {
namespace gui {
namespace qt4 {

class config_context;
struct modplug::audioio::paudio_settings;

class config_audioio_main : public config_page {
public:
    config_audioio_main(
        config_context &,
        modplug::audioio::paudio_settings &,
        QWidget * = 0
    );

    virtual void refresh();

private:
    modplug::audioio::paudio_settings &settings;
    config_context &context;

    QComboBox *devices;
    QComboBox *rates;
    QComboBox *formats;
    QSpinBox *channels;
    QSpinBox *buflen;
};

}
}
}