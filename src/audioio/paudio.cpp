#include "stdafx.h"

#include <cstdint>

#include <functional>
#include <string>

#include "paudio.h"
#include "../MainFrm.h"
#include "../pervasives/pervasives.h"

using namespace modplug::pervasives;

extern BOOL gbStopSent;

namespace modplug {
namespace audioio {

struct json_sample_format_assoc {
    portaudio::SampleDataFormat key;
    const char *value;
};

std::array<const char *, 14> paudio_api_names = {
    "none",
    "directsound",
    "mme",
    "asio",
    "soundmanager",
    "coreaudio",
    "oss",
    "alsa",
    "al",
    "beos",
    "wdmks",
    "jack",
    "wasapi",
    "audiosciencehpi"
};

static const json_sample_format_assoc paudio_sample_format_names[] = {
    { portaudio::FLOAT32, "float32" },
    { portaudio::INT32,   "int32" },
    { portaudio::INT24,   "int24" },
    { portaudio::INT16,   "int16" },
    { portaudio::INT8,    "int8" },
    { portaudio::UINT8,   "uint8" }
};

template <typename arr>
const char * lookup_by_index(arr &names, size_t idx) {
    return (idx >= 0 && idx < names.size()) ? (names[idx]) : (names[0]);
}

template <typename arr>
size_t lookup_by_name(arr &names, const char *name) {
    for (size_t idx = 0; idx < names.size(); ++idx) {
        if (strcmp(name, names[idx]) == 0) {
            return idx;
        }
    }
    return 0;
}



uint8_t bits_per_sample(portaudio::SampleDataFormat format) {
    switch (format) {
    case portaudio::FLOAT32 : return 32;
    case portaudio::INT32   : return 32;
    case portaudio::INT24   : return 24;
    case portaudio::INT16   : return 16;
    case portaudio::INT8    : return 8;
    case portaudio::UINT8   : return 8;
    default:
        DEBUG_FUNC("got unknown sample format %d", format);
        return 0;
    }
}


bool settings_equal(const paudio_settings_t &a, const paudio_settings_t &b) {
    return a.buffer_length == b.buffer_length
        && a.channels == b.channels
        && a.device == b.device
        && a.host_api == b.host_api
        && a.latency == b.latency
        && a.sample_rate == b.sample_rate;
}


PaHostApiTypeId hostapi_of_int(int apinum) {
    switch (apinum) {
    case paInDevelopment   : return paInDevelopment;
    case paDirectSound     : return paDirectSound;
    case paMME             : return paMME;
    case paASIO            : return paASIO;
    case paSoundManager    : return paSoundManager;
    case paCoreAudio       : return paCoreAudio;
    case paOSS             : return paOSS;
    case paALSA            : return paALSA;
    case paAL              : return paAL;
    case paBeOS            : return paBeOS;
    case paWDMKS           : return paWDMKS;
    case paJACK            : return paJACK;
    case paWASAPI          : return paWASAPI;
    case paAudioScienceHPI : return paAudioScienceHPI;
    default:
        DEBUG_FUNC("got unknown hostapi value %d", apinum);
        return paInDevelopment;
    }
};


Json::Value json_of_paudio_settings(
    const paudio_settings_t &settings,
    portaudio::System &pa_system
) {
    Json::Value root;
    root["sample_rate"]   = settings.sample_rate;

    root["host_api"] = lookup_by_index(paudio_api_names, settings.host_api);
    root["device"]   = pa_system.deviceByIndex(settings.device).name();
    root["latency"]  = settings.latency;

    root["buffer_length"] = settings.buffer_length;

    root["channels"] = settings.channels;
    return root;
}

paudio_settings_t paudio_settings_of_json(Json::Value &root, portaudio::System &pa_system) {
    paudio_settings_t ret;

    try {
        ret.sample_rate   = root["sample_rate"].asDouble();

        ret.host_api = static_cast<PaHostApiTypeId>(
            lookup_by_name(paudio_api_names, root["host_api"].asCString())
        );

        ret.device = 0;
        auto &host_api = pa_system.hostApiByTypeId(ret.host_api);
        const char *device_name = root["device"].asCString();
        for (auto idx = host_api.devicesBegin(); idx != host_api.devicesEnd(); ++idx) {
            if (strcmp(device_name, idx->name()) == 0) {
                ret.device = idx->index();
            }
        }

        ret.latency       = root["latency"].asDouble();
        ret.buffer_length = root["buffer_length"].asUInt();
        ret.channels      = root["channels"].asUInt();
    } catch (std::runtime_error e) {
        ret.sample_rate        = 0;
        ret.host_api           = paInDevelopment;
        ret.device             = 0;
        ret.latency            = 0;
        ret.buffer_length      = 0;
        ret.channels           = 0;
    }
    return ret;
}

paudio_callback::paudio_callback(CMainFrame &main_frame, paudio_settings_t &settings) :
    main_frame(main_frame),
    settings(settings)
{ }

int paudio_callback::invoke(const void *input, void *output, unsigned long frames,
                            const PaStreamCallbackTimeInfo *time_info,
                            PaStreamCallbackFlags status_flags)
{

    auto ret = 0;

    const unsigned long width = sizeof(int16_t);
    size_t buf_len = frames * settings.channels * width;

    /*
    DEBUG_FUNC("thread id = %x, buf_len = %d, frames = %d, width = %d",
        GetCurrentThreadId(),
        buf_len,
        frames,
        width
    );
    */

    if (main_frame.IsPlaying() && main_frame.renderer && !gbStopSent) {
        ret = main_frame.renderer->ReadPattern(output, buf_len);
        main_frame.DoNotification(ret, settings.buffer_length);
    }

    if (ret == 0) {
        if (!gbStopSent) {
            gbStopSent = TRUE;
            main_frame.PostMessage(WM_COMMAND, ID_PLAYER_STOP);
        }
        auto out = reinterpret_cast<char *>(output);
        for (unsigned long i = 0; i < buf_len; ++i) {
            out[i] = 0;
        }
    }

    return paContinue;
}

paudio::paudio(const paudio_settings_t &settings, portaudio::System &system, CMainFrame &main_frame) :
    interleaved(true),
    _settings(settings),
    callback(main_frame, _settings),
    stream(
        portaudio::StreamParameters(
            portaudio::DirectionSpecificStreamParameters::null(),
            portaudio::DirectionSpecificStreamParameters(
                system.deviceByIndex(_settings.device),
                _settings.channels,
                portaudio::INT16,
                interleaved,
                _settings.latency,
                nullptr
            ),
            _settings.sample_rate,
            _settings.buffer_length,
            false
        ),
        callback,
        &paudio_callback::invoke
    )
{
    _settings.sample_rate = stream.sampleRate();
    _settings.latency = stream.outputLatency();
}

paudio::~paudio() {
}

void paudio::start() {
    stream.start();
}

void paudio::stop() {
    stream.stop();
}

void paudio::close() {
    stream.close();
}

const paudio_settings_t& paudio::settings() const {
    return _settings;
}

}
}
