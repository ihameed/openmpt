#pragma once

#include <cstdint>

#include "types.h"

#include "../legacy_soundlib/Snd_defs.h"
#include "../legacy_soundlib/misc.h"

namespace modplug {
namespace tracker {

uint32_t frequency_of_transpose(int8_t, int8_t);

std::tuple<int8_t, int8_t> transpose_of_frequency(uint32_t);

struct modsample_t {
    sampleoffset_t length;

    sampleoffset_t loop_start;
    sampleoffset_t loop_end;
    sampleoffset_t sustain_start;
    sampleoffset_t sustain_end;

    char *sample_data;

    uint32_t c5_samplerate;
    uint16_t default_pan;
    uint16_t default_volume;
    uint16_t global_volume;

    uint16_t flags;

    signed char RelativeTone; // Relative note to middle c (for MOD/XM)
    signed char nFineTune;    // Finetune period (for MOD/XM)

    uint8_t vibrato_type;
    uint8_t vibrato_sweep;
    uint8_t vibrato_depth;
    uint8_t vibrato_rate;

    char name[MAX_SAMPLENAME];
    char legacy_filename[MAX_SAMPLEFILENAME];

    uint8_t GetElementarySampleSize() const {return (flags & CHN_16BIT) ? 2 : 1;}

    uint8_t GetNumChannels() const {return (flags & CHN_STEREO) ? 2 : 1;}

    uint8_t GetBytesPerSample() const {return GetElementarySampleSize() * GetNumChannels();}

    uint32_t GetSampleSizeInBytes() const {return length * GetBytesPerSample();}

    uint32_t GetSampleRate(const MODTYPE type) const;
};

inline uint32_t modplug::tracker::modsample_t::GetSampleRate(const MODTYPE type) const {
    uint32_t nRate;
    if(type & (MOD_TYPE_MOD|MOD_TYPE_XM)) {
        nRate = frequency_of_transpose(this->RelativeTone, this->nFineTune);
    } else {
        nRate = c5_samplerate;
    }
    return (nRate > 0) ? nRate : 8363;
}

}
}
