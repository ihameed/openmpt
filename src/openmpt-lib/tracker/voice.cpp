#include "stdafx.h"

#include "voice.hpp"

namespace modplug {
namespace tracker {

vflags_ty
voicef_from_legacy(const uint32_t old) {
    using namespace modplug::pervasives;
    bitset<vflag_ty> ret;
    const auto set = [&] (const uint32_t sf, const vflag_ty vf) {
        if (old & sf) bitset_add(ret, vf);
    };
    set(CHN_STEREO, vflag_ty::Stereo);
    return ret;
}

vflags_ty
voicef_from_smpf(const sflags_ty smpflags) {
    using namespace modplug::pervasives;
    bitset<vflag_ty> ret;
    const auto set = [&] (const sflag_ty sf, const vflag_ty vf) {
        if (bitset_is_set(smpflags, sf)) bitset_add(ret, vf);
    };
    set(sflag_ty::SixteenBit, vflag_ty::SixteenBit);
    set(sflag_ty::Loop, vflag_ty::Loop);
    set(sflag_ty::BidiLoop, vflag_ty::BidiLoop);
    set(sflag_ty::SustainLoop, vflag_ty::SustainLoop);
    set(sflag_ty::BidiSustainLoop, vflag_ty::BidiSustainLoop);
    set(sflag_ty::ForcedPanning, vflag_ty::ForcedPanning);
    set(sflag_ty::Stereo, vflag_ty::Stereo);
    return ret;
}

vflags_ty
voicef_unset_smpf(const vflags_ty vf) {
    auto ret = bitset_unset(vf, vflag_ty::SixteenBit);
    bitset_remove(ret, vflag_ty::SixteenBit);
    bitset_remove(ret, vflag_ty::Loop);
    bitset_remove(ret, vflag_ty::BidiLoop);
    bitset_remove(ret, vflag_ty::SustainLoop);
    bitset_remove(ret, vflag_ty::BidiSustainLoop);
    bitset_remove(ret, vflag_ty::ForcedPanning);
    bitset_remove(ret, vflag_ty::Stereo);
    return ret;
}

vflags_ty
voicef_from_oldchn(const uint32_t old) {
    auto ret = voicef_from_legacy(old);
    const auto set = [&] (const uint32_t sf, const vflag_ty vf) {
        if (old & sf) bitset_add(ret, vf);
    };
    set(CHN_MUTE, vflag_ty::Mute);
    set(CHN_KEYOFF, vflag_ty::KeyOff);
    set(CHN_NOTEFADE, vflag_ty::NoteFade);
    set(CHN_SURROUND, vflag_ty::Surround);
    set(CHN_NOIDO, vflag_ty::DisableInterpolation);
    set(CHN_FILTER, vflag_ty::Filter);
    set(CHN_VOLUMERAMP, vflag_ty::VolumeRamp);
    set(CHN_VIBRATO, vflag_ty::Vibrato);
    set(CHN_TREMOLO, vflag_ty::Tremolo);
    set(CHN_PANBRELLO, vflag_ty::Panbrello);
    set(CHN_PORTAMENTO, vflag_ty::Portamento);
    set(CHN_GLISSANDO, vflag_ty::Glissando);
    set(CHN_VOLENV, vflag_ty::VolEnvOn);
    set(CHN_PANENV, vflag_ty::PanEnvOn);
    set(CHN_PITCHENV, vflag_ty::PitchEnvOn);
    set(CHN_FASTVOLRAMP, vflag_ty::FastVolRamp);
    set(CHN_EXTRALOUD, vflag_ty::ExtraLoud);
    set(CHN_SOLO, vflag_ty::Solo);
    set(CHN_NOFX, vflag_ty::DisableDsp);
    set(CHN_SYNCMUTE, vflag_ty::SyncMute);
    set(CHN_FILTERENV, vflag_ty::PitchEnvAsFilter);
    return ret;
}

}
}
