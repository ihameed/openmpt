#pragma once

#include <cstdint>

#include "modevent.hpp"
#include "types.hpp"
#include "sample.hpp"
#include "voice.hpp"

#include "../pervasives/bitset.hpp"

#include "../legacy_soundlib/Snd_defs.h"
#include "../mixgraph/constants.hpp"

class CTuningBase;
typedef CTuningBase CTuning;

namespace modplug {
namespace tracker {

struct resampler_config_t {
    //rewbs.resamplerConf
    long ramp_in_length;
    long ramp_out_length;
    double fir_cutoff;
    uint8_t fir_type; //WFIR_KAISER4T;
    //end rewbs.resamplerConf
};

struct modenvelope_t {
    uint32_t flags;

    uint16_t Ticks[MAX_ENVPOINTS]; // envelope point position (x axis)
    uint8_t Values[MAX_ENVPOINTS]; // envelope point value (y axis)

    uint32_t num_nodes;

    uint8_t loop_start;
    uint8_t loop_end;
    uint8_t sustain_start;
    uint8_t sustain_end;

    uint8_t release_node;
};

struct modinstrument_t {
    uint32_t fadeout;
    uint32_t flags;
    uint32_t global_volume;
    uint32_t default_pan;

    modenvelope_t volume_envelope;
    modenvelope_t panning_envelope;
    modenvelope_t pitch_envelope;

    uint8_t NoteMap[128];    // Note mapping, f.e. C-5 => D-5.
    uint16_t Keyboard[128];  // Sample mapping, f.e. C-5 => Sample 1

    uint8_t new_note_action;
    uint8_t duplicate_check_type;
    uint8_t duplicate_note_action;

    uint8_t random_pan_weight;
    uint8_t random_volume_weight;

    uint8_t default_filter_cutoff;
    uint8_t default_filter_resonance;

    uint16_t midi_bank;
    uint8_t midi_program;
    uint8_t midi_channel;
    uint8_t midi_drum_set;

    signed char pitch_pan_separation;
    unsigned char pitch_pan_center;

    char name[32];            // Note: not guaranteed to be null-terminated.
    char legacy_filename[32];

    PLUGINDEX nMixPlug;                            // Plugin assigned to this instrument

    modplug::mixgraph::id_t graph_insert;

    uint16_t volume_ramp_up;
    uint16_t volume_ramp_down;

    uint32_t resampling_mode;                            // Resampling mode

    uint8_t random_cutoff_weight;
    uint8_t random_resonance_weight;
    uint8_t default_filter_mode;

    uint16_t pitch_to_tempo_lock;

    uint8_t nPluginVelocityHandling;    // How to deal with plugin velocity
    uint8_t nPluginVolumeHandling;            // How to deal with plugin volume
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// WHEN adding new members here, ALSO update Sndfile.cpp (instructions near the top of this file)!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    CTuning* pTuning;                            // sample tuning assigned to this instrument
    static CTuning* s_DefaultTuning;

    void SetTuning(CTuning* pT)
    {
        pTuning = pT;
    }
};

#define CHNRESET_SETPOS_BASIC   2 // 10 b
#define CHNRESET_SETPOS_FULL    7 //111 b
#define CHNRESET_TOTAL        255 //11111111b

struct initial_voice_settings_ty {
    uint32_t nPan;
    uint32_t nVolume;
    uint32_t dwFlags;
    PLUGINDEX nMixPlugin;
    char szName[MAX_CHANNELNAME];
};


}
}
