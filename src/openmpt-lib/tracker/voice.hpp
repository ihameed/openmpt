#pragma once

#include "sample.hpp"

namespace modplug {
namespace tracker {

enum struct vflag_ty : uint32_t {
    SixteenBit           = 1u << 0,
    Loop                 = 1u << 1,
    BidiLoop             = 1u << 2,
    SustainLoop          = 1u << 3,
    BidiSustainLoop      = 1u << 4,
    Stereo               = 1u << 5,
    ScrubBackwards       = 1u << 6,
    Mute                 = 1u << 7,
    KeyOff               = 1u << 8,
    NoteFade             = 1u << 9,
    Surround             = 1u << 10,
    DisableInterpolation = 1u << 11,
    AlreadyLooping       = 1u << 12,
    Filter               = 1u << 13,
    VolumeRamp           = 1u << 14,
    Vibrato              = 1u << 15,
    Tremolo              = 1u << 16,
    Panbrello            = 1u << 17,
    Portamento           = 1u << 18,
    Glissando            = 1u << 19,
    VolEnvOn             = 1u << 20,
    PanEnvOn             = 1u << 21,
    PitchEnvOn           = 1u << 22,
    FastVolRamp          = 1u << 23,
    ExtraLoud            = 1u << 24,
    Solo                 = 1u << 27,
    DisableDsp           = 1u << 28,
    SyncMute             = 1u << 29,
    PitchEnvAsFilter     = 1u << 30,
    ForcedPanning        = 1u << 31
};

typedef modplug::pervasives::bitset<vflag_ty> vflags_ty;

vflags_ty
voicef_from_legacy(const uint32_t);

vflags_ty
voicef_from_smpf(const sflags_ty);

vflags_ty
voicef_unset_smpf(const vflags_ty);

vflags_ty
voicef_from_oldchn(const uint32_t);

}
}
