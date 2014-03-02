#pragma once

#include <cstdint>

#include "types.hpp"
#include "../mixgraph/constants.hpp"

#include "../pervasives/bitset.hpp"

#include "../legacy_soundlib/Snd_defs.h"
#include "../legacy_soundlib/misc.h"

namespace modplug {
namespace tracker {

typedef modplug::mixgraph::sample_t sample_t;

const auto NormInt32 = static_cast<sample_t>(-1.0 / INT32_MIN);
const auto NormInt24 = static_cast<sample_t>(-1.0 / -8388608.0);
const auto NormInt16 = static_cast<sample_t>(-1.0 / INT16_MIN);
const auto NormInt8  = static_cast<sample_t>(-1.0 / INT8_MIN);

uint32_t frequency_of_transpose(int8_t, int8_t);

std::tuple<int8_t, int8_t> transpose_of_frequency(uint32_t);

enum struct sflag_ty : uint8_t {
    Loop            = 1u << 1,
    BidiLoop        = 1u << 2,
    SustainLoop     = 1u << 3,
    BidiSustainLoop = 1u << 4,
    ForcedPanning   = 1u << 5,
    Stereo          = 1u << 6
};

typedef modplug::pervasives::bitset<sflag_ty> sflags_ty;

union sample_data_ty {
    char    *generic;
    int8_t  *int8;
    int16_t *int16;
    int32_t *int32;
    float   *fp32;
    double  *fp64;
};

static_assert(sizeof(sample_data_ty) == sizeof(void *),
    "sample_data_ty must only contain pointers!");

struct int8_f {
    typedef int8_t FieldTy;

    static __forceinline FieldTy *
    fetch(const sample_data_ty val) { return val.int8; };

    static __forceinline sample_t
    normalize(const int8_t val) { return NormInt8 * val; }
};

struct int16_f {
    typedef int16_t FieldTy;

    static __forceinline FieldTy *
    fetch(const sample_data_ty val) { return val.int16; };

    static __forceinline sample_t
    normalize(const int16_t val) { return NormInt16 * val; }
};

struct int24_f {
    typedef int32_t FieldTy;

    static __forceinline FieldTy *
    fetch(const sample_data_ty val) { return val.int32; };

    static __forceinline sample_t
    normalize(const int32_t val) { return NormInt24 * val; }
};

struct int32_f {
    typedef int32_t FieldTy;

    static __forceinline FieldTy *
    fetch(const sample_data_ty val) { return val.int32; };

    static __forceinline sample_t
    normalize(const int32_t val) { return NormInt32 * val; }
};

struct fp32_f {
    typedef float FieldTy;

    static __forceinline FieldTy *
    fetch(const sample_data_ty val) { return val.fp32; };

    static __forceinline sample_t
    normalize(const float val) { return val; }
};

struct fp64_f {
    typedef double FieldTy;

    static __forceinline FieldTy *
    fetch(const sample_data_ty val) { return val.fp64; };

    static __forceinline sample_t
    normalize(const double val) { return val; }
};

template <typename Ty>
typename Ty::FieldTy *
fetch_buf(const sample_data_ty val) {
    return Ty::fetch(val);
}

template <typename Ty>
sample_t __forceinline
normalize_single(const typename Ty::FieldTy val) {
    return Ty::normalize(val);
}

enum struct stag_ty : uint8_t {
    Int8,
    Int16,
    Int24,
    Int32,
    Fp32,
    Fp64
};

struct modsample_t {
    sampleoffset_t length;

    sampleoffset_t loop_start;
    sampleoffset_t loop_end;
    sampleoffset_t sustain_start;
    sampleoffset_t sustain_end;

    sample_data_ty sample_data;
    stag_ty sample_tag;
    sflags_ty flags;

    uint32_t c5_samplerate;
    uint16_t default_pan;
    uint16_t default_volume;
    uint16_t global_volume;

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
sample_width(const modsample_t &);

size_t
sample_bytes_per_frame(const modsample_t &);

size_t
sample_len_bytes(const modsample_t &);

}
}
