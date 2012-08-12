#pragma once

#include "portaudiocpp/PortAudioCpp.hxx"

#include <array>
#include <json/json.h>

class CMainFrame;

namespace modplug {
namespace audioio {

struct paudio_settings {
    double sample_rate;

    PaHostApiTypeId host_api;
    PaDeviceIndex device;

    PaTime latency;
    unsigned int buffer_length;
    unsigned int channels;
};

bool settings_equal(const paudio_settings &, const paudio_settings &);

Json::Value json_of_paudio_settings(
    const paudio_settings &, portaudio::System &
);
paudio_settings paudio_settings_of_json(Json::Value &, portaudio::System &);
PaHostApiTypeId hostapi_of_int(int);


class paudio_callback {
public:
    paudio_callback(CMainFrame &main_frame, paudio_settings &settings);

    int invoke(const void *,
               void *,
               unsigned long,
               const PaStreamCallbackTimeInfo *,
               PaStreamCallbackFlags);


private:
    CMainFrame &main_frame;
    paudio_settings &settings;
};



class paudio {
public:
    paudio(const paudio_settings &settings, portaudio::System &system, CMainFrame &);
    ~paudio();

    void start();
    void stop();
    void close();
    const paudio_settings& settings() const;

private:
    const bool interleaved;
    paudio_settings _settings;
    paudio_callback callback;

    portaudio::MemFunCallbackStream<paudio_callback> stream;
};

}
}