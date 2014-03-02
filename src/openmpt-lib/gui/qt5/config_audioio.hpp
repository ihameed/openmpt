#pragma once
#include <QtGui>
#include <QtWidgets/QtWidgets>

#include "config_dialog.hpp"
#include "../../pervasives/pervasives.hpp"

namespace modplug {

namespace gui {
namespace qt5 {

class app_config;

class config_audioio_main : public config_page
{
    Q_OBJECT
public:
    config_audioio_main(app_config &);

    virtual void refresh();
    virtual void apply_changes();

private slots:
    void latency_event_with_int(int);
    void modified(int);

private:
    void update_latency_indicator();
    double current_sample_rate() const;

    bool _modified;

    app_config &context;

    QComboBox devices;
    QComboBox rates;
    QSpinBox channels;
    QSpinBox buflen;
    QLabel latency;
};

}
}
}
