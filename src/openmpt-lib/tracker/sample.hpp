#pragma once

#include <cstdint>

#include "types.hpp"

#include "../pervasives/bitset.hpp"

#include "../legacy_soundlib/Snd_defs.h"
#include "../legacy_soundlib/misc.h"

namespace modplug {
namespace tracker {

uint32_t frequency_of_transpose(int8_t, int8_t);

std::tuple<int8_t, int8_t> transpose_of_frequency(uint32_t);

enum struct sflag_ty : uint32_t {
    SixteenBit      = 1u << 0,
    Loop            = 1u << 1,
    BidiLoop        = 1u << 2,
    SustainLoop     = 1u << 3,
    BidiSustainLoop = 1u << 4,
    ForcedPanning   = 1u << 5,
    Stereo          = 1u << 6
};

typedef modplug::pervasives::bitset<sflag_ty> sflags_ty;

union sample_data_ty {
    char *generic;
    int8_t *int8;
    int16_t *int16;
};

struct modsample_t {
    sampleoffset_t length;

    sampleoffset_t loop_start;
    sampleoffset_t loop_end;
    sampleoffset_t sustain_start;
    sampleoffset_t sustain_end;

    sample_data_ty sample_data;

    uint32_t c5_samplerate;
    uint16_t default_pan;
    uint16_t default_volume;
    uint16_t global_volume;

    sflags_ty flags;

    signed char RelativeTone; // Relative note to middle c (for MOD/XM)
    signed char nFineTune;    // Finetune period (for MOD/XM)

    uint8_t vibrato_type;
    uint8_t vibrato_sweep;
    uint8_t vibrato_depth;
    uint8_t vibrato_rate;

    char name[MAX_SAMPLENAME];
    char legacy_filename[MAX_SAMPLEFILENAME];
};

size_t
sample_channels(const modsample_t &);

size_t
sample_bytes_per_frame(const modsample_t &);

size_t
sample_len_bytes(const modsample_t &);

}
}
