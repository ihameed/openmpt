#pragma once

#include <QtGui>
#include <json/json.h>

#include "../../pervasives/pervasives.h"

namespace portaudio { class System; }

namespace modplug { namespace audioio { struct paudio_settings_t; } }

#define CONFIG_KNOB(type, name) \
public: \
    const type & name () const; \
    void change_##name (const type &);

namespace modplug {
namespace gui {
namespace qt4 {

struct colors_t;
struct private_configs;
class misc_globals;

class app_config : public QObject, private modplug::pervasives::noncopyable {
    Q_OBJECT
    typedef modplug::audioio::paudio_settings_t paudio_settings_t;

public:
    app_config(portaudio::System &);
    ~app_config();

    Json::Value export_json() const;
    void import_json(Json::Value &);

    portaudio::System & audio_handle();

    CONFIG_KNOB(paudio_settings_t, audio_settings)
    CONFIG_KNOB(colors_t,          colors)

signals:
    void audio_settings_changed();
    void colors_changed();

private:
    std::unique_ptr<private_configs> store;
    std::unique_ptr<misc_globals> globals;
};

#undef CONFIG_KNOB


}
}
}