#include "stdafx.h"

#include "../../pervasives/pervasives.h"
#include "config_dialog.h"
#include "config_audioio.h"
#include "../../audioio/paudio.h"

namespace modplug {
namespace gui {
namespace qt4 {


using namespace modplug::pervasives;

using namespace modplug::audioio;

struct sample_rate_assoc {
    int key;
    const char *value;
};

struct sample_format_assoc {
    portaudio::SampleDataFormat key;
    const char *value;
};

static const sample_rate_assoc supported_sample_rates[] = {
    { 22050.0,  "22050" },
    { 44100.0,  "44100" },
    { 48000.0,  "48000" },
    { 96000.0,  "96000" },
    { 192000.0, "192000" }
};

static const sample_format_assoc supported_sample_formats[] = {
    { portaudio::UINT8,   "8-bit unsigned integer" },
    { portaudio::INT8,    "8-bit signed integer" },
    { portaudio::INT16,   "16-bit signed integer" },
    { portaudio::INT24,   "24-bit signed integer" },
    { portaudio::INT32,   "32-bit signed integer" },
    { portaudio::FLOAT32, "32-bit floating point" }
};



config_audioio_main::config_audioio_main(
    config_context &context,
    paudio_settings &settings,
    QWidget *parent
) : config_page(), settings(settings), context(context)
{
    auto layout = new QGridLayout(this);

    auto hoot = new QComboBox;
    devices = new QComboBox;
    rates = new QComboBox;
    formats = new QComboBox;
    latency = new QLabel;

    buflen = new QSpinBox;
    buflen->setSuffix(" samples");
    buflen->setMinimum(0);
    buflen->setMaximum(5678);
    buflen->setSingleStep(32);

    channels = new QSpinBox;
    channels->setMinimum(0);

    int row = 0;
    auto add_widget = [&](char *name, QWidget *widget) {
        layout->addWidget(new QLabel(name), row, 0);
        layout->addWidget(widget, row, 1);
        ++row;
    };

    add_widget("Output Device:", devices);
    add_widget("Sample Rate:", rates);
    add_widget("Sample Format:", formats);
    add_widget("Buffer Length:", buflen);
    add_widget("Output Channels:", channels);
    add_widget("Output Latency:", latency);

    layout->addItem(
        new QSpacerItem(
            0, 0,
            QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding
        ),
        row, 1
    );

    for_each(supported_sample_rates, [&](sample_rate_assoc t) {
        rates->addItem(t.value, QVariant(t.key));
    });

    for_each(supported_sample_formats, [&](sample_format_assoc t) {
        formats->addItem(t.value);
    });

    auto &pa_system = context.pa_system;

    for (auto i = pa_system.devicesBegin(); i != pa_system.devicesEnd(); ++i) {
        devices->addItem(
            QString("%1: %2").arg(i->hostApi().name()) .arg(i->name())
        );
    }

    QObject::connect(
        rates, SIGNAL(currentIndexChanged(int)),
        this, SLOT(latency_event_with_int(int))
    );

    QObject::connect(
        buflen, SIGNAL(valueChanged(int)),
        this, SLOT(latency_event_with_int(int))
    );

    refresh();
};

void config_audioio_main::refresh() {
    DEBUG_FUNC("%s",
        debug_json_dump(json_of_paudio_settings(
            settings,
            context.pa_system
        )).c_str()
    );
    auto current_sample_rate = static_cast<int>(settings.sample_rate);

    rates->setCurrentIndex(idx_of_key(
        supported_sample_rates,
        current_sample_rate
    ));
    formats->setCurrentIndex(idx_of_key(
        supported_sample_formats,
        settings.sample_format
    ));

    devices->setCurrentIndex(settings.device);

    buflen->setValue(settings.buffer_length);

    channels->setValue(settings.channels);
}

void config_audioio_main::latency_event_with_int(int) {
    update_latency_indicator();
}

void config_audioio_main::update_latency_indicator() {
    double rate = current_sample_rate();
    int len = buflen->value();
    double latency_ms = rate == 0
        ? 0
        : (1000 * len) / rate;
    latency->setText(QString("%1 ms").arg(latency_ms));
}

double config_audioio_main::current_sample_rate() const {
    int idx = rates->currentIndex();
    auto data = rates->itemData(idx);
    return data == QVariant::Invalid ? 0 : data.toDouble();
}


}
}
}