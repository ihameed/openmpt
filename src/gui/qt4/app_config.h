#pragma once

#include <QtGui>
#include <json/json.h>

#include "../../pervasives/pervasives.h"

namespace portaudio { class System; }

namespace modplug { namespace audioio { struct paudio_settings; } }

namespace modplug {
namespace gui {
namespace qt4 {

struct colors_t;
struct private_configs;
class misc_globals;

class app_config : public QObject, private modplug::pervasives::noncopyable {
    Q_OBJECT

public:
    app_config(portaudio::System &);
    ~app_config();

    Json::Value export_json() const;
    void import_json(Json::Value &);


    const modplug::audioio::paudio_settings &audio_settings() const;
    void change_audio_settings(const modplug::audioio::paudio_settings &);
    portaudio::System & audio_handle();

    const colors_t &colors() const;
    void change_colors(const colors_t &);

signals:
    void audio_settings_changed();
    void colors_changed();

private:
    std::unique_ptr<private_configs> store;
    std::unique_ptr<misc_globals> globals;

};


}
}
}