#include "stdafx.h"

#include "../../pervasives/pervasives.h"

#include "../../audioio/paudio.h"
#include "app_config.h"

using namespace modplug::audioio;

namespace modplug {
namespace gui {
namespace qt4 {

struct private_configs {
    paudio_settings audio;
};

class misc_globals {
public:
    misc_globals(portaudio::System &system) : pa_system(system) { };
    portaudio::System &pa_system;
};

app_config::app_config(portaudio::System &system) :
    store(new private_configs()),
    globals(new misc_globals(system))
{ }

app_config::~app_config()
{ }


Json::Value app_config::export_json() const {
    Json::Value root;
    root["audio"] = json_of_paudio_settings(store->audio, globals->pa_system);
    return root;
}

void app_config::import_json(Json::Value &root) {
    store->audio = paudio_settings_of_json(root["audio"], globals->pa_system);
}


const paudio_settings & app_config::audio_settings() const {
    return store->audio;
}

void app_config::change_audio_settings(const paudio_settings &settings) {
    store->audio = settings;
    emit audio_settings_changed();
}

portaudio::System & app_config::audio_handle() {
    return globals->pa_system;
}


}
}
}