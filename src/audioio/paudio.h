#pragma once

#include "portaudiocpp/PortAudioCpp.hxx"

#include <array>
#include <json/json.h>

class CMainFrame;

namespace modplug {
namespace audioio {

struct paudio_settings {
    double sample_rate;
    portaudio::SampleDataFormat sample_format;

    PaHostApiTypeId host_api;
    PaDeviceIndex device;

    PaTime latency;
    long asio_buffer_length;
};
Json::Value json_of_paudio_settings(const paudio_settings &);
paudio_settings paudio_settings_of_json(Json::Value &);




class paudio_callback {
public:
    paudio_callback(CMainFrame &main_frame);

    int invoke(const void *, void *, unsigned long, const PaStreamCallbackTimeInfo *, PaStreamCallbackFlags);


private:
    CMainFrame &main_frame;
    int debug_counter;
    const int debug_wrap;
};



class paudio {
public:
    paudio(paudio_settings &settings, portaudio::System &system, CMainFrame &);
    ~paudio();

    void start();
    void stop();
    void close();

private:
    const bool interleaved;
    const int channels;
    const int buffer_length;
    paudio_settings settings;
    paudio_callback callback;

    portaudio::MemFunCallbackStream<paudio_callback> stream;
};

}
}