#include "stdafx.h"

#include <cstdint>

#include "paudio.h"
#include "../MainFrm.h"
#include "../pervasives/pervasives.h"

using namespace modplug::pervasives;

extern BOOL gbStopSent;

namespace modplug {
namespace audioio {

typedef char * huoh;

std::array<const char *, 14> paudio_api_names = {
    "none", "directsound", "mme", "asio", "soundmanager",
    "coreaudio", "oss", "alsa", "al", "beos", "wdmks", "jack",
    "wasapi", "audiosciencehpi"
};

std::array<const char *, 7> paudio_sample_format_names = {
    "invalid", "float32", "int32", "int24", "int16", "int8", "uint8"
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

Json::Value json_of_paudio_settings(const paudio_settings &settings, portaudio::System &pa_system) {
    Json::Value root;
    root["sample_rate"]   = settings.sample_rate;
    root["sample_format"] = lookup_by_index(paudio_sample_format_names, (size_t) settings.sample_format);

    root["host_api"] = lookup_by_index(paudio_api_names, settings.host_api);
    root["device"]   = pa_system.deviceByIndex(settings.device).name();
    root["latency"]  = settings.latency;

    root["asio_buffer_length"] = settings.asio_buffer_length;
    return root;
}

paudio_settings paudio_settings_of_json(Json::Value &root, portaudio::System &pa_system) {
    paudio_settings ret;

    try {
        ret.sample_rate   = root["sample_rate"].asDouble();
        ret.sample_format = static_cast<portaudio::SampleDataFormat>(
            lookup_by_name(paudio_sample_format_names, root["sample_format"].asCString())
        );

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

        ret.latency = root["latency"].asDouble();
        ret.asio_buffer_length = root["asio_buffer_length"].asUInt();
    } catch (std::runtime_error e) {
        ret.sample_rate        = 0;
        ret.sample_format      = portaudio::INVALID_FORMAT;
        ret.host_api           = paInDevelopment;
        ret.device             = 0;
        ret.latency            = 0;
        ret.asio_buffer_length = 0;
    }
    return ret;
}

paudio_callback::paudio_callback(CMainFrame &main_frame) :
    main_frame(main_frame),
    debug_wrap(1024),
    debug_counter(0)
{ }

int paudio_callback::invoke(const void *input, void *output, unsigned long frames,
                            const PaStreamCallbackTimeInfo *time_info,
                            PaStreamCallbackFlags status_flags)
{
    auto ret = 0;

    unsigned long channels = 2;
    const unsigned long width = sizeof(int16_t);
    size_t buf_len = frames * channels * width;

    if (main_frame.IsPlaying() && main_frame.m_pSndFile && !gbStopSent) {
        if (debug_counter % debug_wrap == 0) {
            debug_log("paudio_callback: main_frame.IsPlaying = '%d', main_frame.m_pSndFile = '%p'", main_frame.IsPlaying(), main_frame.m_pSndFile);
        }
        ret = main_frame.m_pSndFile->ReadPattern(output, buf_len);
        if (debug_counter % debug_wrap == 0) {
            debug_log("paudio_callback: ReadPattern = '%d'", ret);
        }
        main_frame.DoNotification(ret, 128);
    }

    if (ret == 0) {
        if (debug_counter % debug_wrap == 0) {
            debug_log("paudio_callback: ret == 0; clearing buffer");
        }
        if (!gbStopSent) {
            gbStopSent = TRUE;
            main_frame.PostMessage(WM_COMMAND, ID_PLAYER_STOP);
        }
        auto out = reinterpret_cast<char *>(output);
        for (unsigned long i = 0; i < buf_len; ++i) {
            out[i] = 0;
        }
    }

    ++debug_counter;

    return paContinue;
}

paudio::paudio(paudio_settings &settings, portaudio::System &system, CMainFrame &main_frame) :
    interleaved(true),
    channels(2),
    buffer_length(64),
    callback(main_frame),
    settings(settings),
    stream(
        portaudio::StreamParameters(
            portaudio::DirectionSpecificStreamParameters::null(),
            portaudio::DirectionSpecificStreamParameters(
                system.deviceByIndex(settings.device),
                channels,
                settings.sample_format,
                interleaved,
                settings.latency,
                nullptr
            ),
            settings.sample_rate,
            buffer_length,
            false
        ),
        callback,
        &paudio_callback::invoke
    )
{ }

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

}
}