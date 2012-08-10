#pragma once
#include <QtGui>

#include "config_dialog.h"

namespace modplug {

namespace audioio { struct paudio_settings; }

namespace gui {
namespace qt4 {

class config_context;

class config_audioio_main : public config_page {
    Q_OBJECT
public:
    config_audioio_main(
        config_context &,
        modplug::audioio::paudio_settings &,
        QWidget * = 0
    );

    virtual void refresh();

private slots:
    void latency_event_with_int(int);

private:
    void update_latency_indicator();
    double current_sample_rate() const;

    modplug::audioio::paudio_settings &settings;
    config_context &context;

    QComboBox *devices;
    QComboBox *rates;
    QComboBox *formats;
    QSpinBox *channels;
    QSpinBox *buflen;
    QLabel *latency;
};

}
}
}