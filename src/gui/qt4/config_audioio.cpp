#include "stdafx.h"

#include "../../pervasives/pervasives.h"
#include "app_config.h"
#include "config_dialog.h"
#include "config_audioio.h"
#include "../../audioio/paudio.h"

namespace modplug {
namespace gui {
namespace qt4 {


using namespace modplug::pervasives;

using namespace modplug::audioio;

struct sample_rate_assoc {
    double key;
    const char *value;
};

struct sample_format_assoc {
    portaudio::SampleDataFormat key;
    const char *value;
};

static const sample_rate_assoc supported_sample_rates[] = {
    { 44100.0,  "44100" },
    { 48000.0,  "48000" },
    { 96000.0,  "96000" },
    { 192000.0, "192000" }
};

union devid_hostapi_pack {
    struct {
        uint16_t devid;
        uint16_t hostapi;
    };
    uint32_t packed;
};

class config_audioio_asio : public config_page {

};

config_audioio_main::config_audioio_main(app_config &context) : context(context)
{
    auto layout = new QGridLayout(this);

    buflen.setSuffix(" samples");
    buflen.setMinimum(0);
    buflen.setMaximum(5678);
    buflen.setSingleStep(32);

    channels.setMinimum(0);

    int row = 0;
    auto add_widget = [&](char *name, QWidget &widget) {
        layout->addWidget(new QLabel(name), row, 0);
        layout->addWidget(&widget, row, 1);
        ++row;
    };

    add_widget("Output Device:", devices);
    add_widget("Sample Rate:", rates);
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
        rates.addItem(t.value, QVariant(t.key));
    });

    auto &pa_system = context.audio_handle();

    for (auto i = pa_system.devicesBegin(); i != pa_system.devicesEnd(); ++i) {
        if (i->maxOutputChannels() > 0) {
            devid_hostapi_pack pack;
            pack.devid = i->index();
            pack.hostapi = i->hostApi().typeId();
            devices.addItem(
                QString("%1: %2").arg(i->hostApi().name()).arg(i->name()),
                QVariant(pack.packed)
            );
        }
    }

    QObject::connect(
        &rates, SIGNAL(currentIndexChanged(int)),
        this, SLOT(latency_event_with_int(int))
    );

    QObject::connect(
        &buflen, SIGNAL(valueChanged(int)),
        this, SLOT(latency_event_with_int(int))
    );

    refresh();
};

void config_audioio_main::refresh() {
    auto settings = context.audio_settings();

    DEBUG_FUNC("%s",
        debug_json_dump(json_of_paudio_settings(
            settings,
            context.audio_handle()
        )).c_str()
    );

    auto current_sample_rate = static_cast<int>(settings.sample_rate);

    rates.setCurrentIndex(idx_of_key(
        supported_sample_rates,
        current_sample_rate
    ));

    devid_hostapi_pack pack;
    pack.devid = settings.device;
    pack.hostapi = settings.host_api;

    devices.setCurrentIndex(devices.findData(pack.packed));

    buflen.setValue(settings.buffer_length);

    channels.setValue(settings.channels);
}

void config_audioio_main::apply_changes() {
    auto current_sample_rate = [&] () -> double {
        auto idx = rates.currentIndex();
        return idx >= 0 ? rates.itemData(idx).toDouble() : 0.0;
    };

    paudio_settings settings;
    settings.buffer_length = buflen.value();
    settings.channels = channels.value();

    settings.host_api = paInDevelopment;
    settings.device = 0;
    auto idx = devices.currentIndex();
    if (idx >= 0) {
        devid_hostapi_pack pack;
        pack.packed = devices.itemData(idx).toUInt();
        settings.host_api = hostapi_of_int(pack.hostapi);
        settings.device = pack.devid;
    }

    settings.latency = context.audio_handle().deviceByIndex(settings.device)
                                             .defaultLowOutputLatency();
    if (settings.host_api == paASIO) {
        settings.latency = 0;
    }
    settings.sample_rate = current_sample_rate();

    context.change_audio_settings(settings);
}

void config_audioio_main::latency_event_with_int(int) {
    update_latency_indicator();
}

void config_audioio_main::update_latency_indicator() {
    double rate = current_sample_rate();
    int len = buflen.value();
    double latency_ms = rate == 0
        ? 0
        : (1000 * len) / rate;
    latency.setText(QString("%1 ms").arg(latency_ms));
}

double config_audioio_main::current_sample_rate() const {
    int idx = rates.currentIndex();
    auto data = rates.itemData(idx);
    return data == QVariant::Invalid ? 0 : data.toDouble();
}


}
}
}