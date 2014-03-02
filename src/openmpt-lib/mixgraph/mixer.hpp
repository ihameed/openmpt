#pragma once

#include "constants.hpp"
#include "../tracker/tracker.hpp"
#include <cmath>
#include <cstdint>

namespace modplug {
namespace mixgraph {


const double PI = 3.14159265358979323846;
//const size_t FIR_TAPS = 64;
const size_t FIR_TAPS = 8;
const size_t FIR_CENTER = (FIR_TAPS / 2) - 1;
const size_t TAP_PHASES = 4096;
const size_t SINC_SIZE = FIR_TAPS * TAP_PHASES;

extern sample_t upsample_sinc_table[];
extern sample_t downsample_sinc_table[];

void init_tables();

struct filterchannelstate_t {
    sample_t y1;
    sample_t y2;
};

struct filterstate_t {
    filterchannelstate_t left;
    filterchannelstate_t right;
};

void __forceinline
init_filterstate(filterstate_t &state, modplug::tracker::voice_ty &source) {
    using namespace modplug::pervasives;
    using namespace modplug::tracker;
    if (!bitset_is_set(source.flags, vflag_ty::Filter)) return;
    state.left.y1 = source.nFilter_Y1;
    state.left.y2 = source.nFilter_Y2;
    state.right.y1 = source.nFilter_Y3;
    state.right.y2 = source.nFilter_Y4;
}

void __forceinline
save_filterstate(filterstate_t &state, modplug::tracker::voice_ty &source) {
    using namespace modplug::pervasives;
    using namespace modplug::tracker;
    if (!bitset_is_set(source.flags, vflag_ty::Filter)) return;
    source.nFilter_Y1 = state.left.y1;
    source.nFilter_Y2 = state.left.y2;
    source.nFilter_Y3 = state.right.y1;
    source.nFilter_Y4 = state.right.y2;
}

template <typename Field>
sample_t __forceinline
convert_sample(
    const typename Field::FieldTy * const derp,
    const modplug::tracker::voice_ty &source, int idx)
{
    if (idx < 0) return 0;
    if (idx >= source.length) return 0;
    return modplug::tracker::normalize_single<Field>(derp[idx]);
}

template <typename Field>
void __forceinline
resample_and_mix_no_interpolation(
    sample_t &left,
    sample_t &right,
    modplug::tracker::voice_ty &source,
    typename const Field::FieldTy * const in_sample,
    int smp_pos,
    int fixedpt_pos)
{
    using namespace modplug::pervasives;
    using namespace modplug::tracker;
    int pos = smp_pos;
    if (bitset_is_set(source.flags, vflag_ty::Stereo)) {
        pos += (fixedpt_pos >> 16) * 2;
        left  = convert_sample<Field>(in_sample, source, pos);
        right = convert_sample<Field>(in_sample, source, pos + 1);
    } else {
        pos += (fixedpt_pos >> 16);
        left  = convert_sample<Field>(in_sample, source, pos);
        right = left;
    }
}

template <typename Field>
void __forceinline
resample_and_mix_windowed_sinc(
    sample_t &left,
    sample_t &right,
    modplug::tracker::voice_ty &source,
    const typename Field::FieldTy * const in_sample,
    int smp_pos,
    int fixedpt_pos)
{
    using namespace modplug::pervasives;
    using namespace modplug::tracker;
    int pos = smp_pos;
    int fir_idx = ((fixedpt_pos & 0xfff0) >> 4) * FIR_TAPS;

    const sample_t *fir_table =
        ((source.position_delta > 0x13000 || source.position_delta < -0x13000) ?
            (downsample_sinc_table) :
            (upsample_sinc_table))
        + fir_idx;

    if (bitset_is_set(source.flags, vflag_ty::Stereo)) {
        pos += (fixedpt_pos >> 16) * 2;
        for (size_t tap = 0; tap < FIR_TAPS; ++tap) {
            left  += fir_table[tap] *
                     convert_sample<Field>(in_sample, source, tap - FIR_CENTER + pos);
            right += fir_table[tap] *
                     convert_sample<Field>(in_sample, source, tap - FIR_CENTER + pos + 1);
        }
    } else {
        pos += (fixedpt_pos >> 16);
        for (size_t tap = 0; tap < FIR_TAPS; ++tap) {
            left += fir_table[tap] *
                    convert_sample<Field>(in_sample, source, tap - FIR_CENTER + pos);
        }
        right = left;
    }
}

void __forceinline
apply_filter(sample_t &left, sample_t &right, modplug::tracker::voice_ty &source, filterstate_t &fst) {
    using namespace modplug::pervasives;
    using namespace modplug::tracker;
    if (!bitset_is_set(source.flags, vflag_ty::Filter)) return;
    sample_t fy;

    fy = left * source.nFilter_A0 + fst.left.y1 * source.nFilter_B0 + fst.left.y2 * source.nFilter_B1;
    fst.left.y2 = fst.left.y1;
    fst.left.y1 = fy - (left * source.nFilter_HP);
    left = fy;

    fy = right * source.nFilter_A0 + fst.right.y1 * source.nFilter_B0 + fst.right.y2 * source.nFilter_B1;
    fst.right.y2 = fst.right.y1;
    fst.right.y1 = fy - (right * source.nFilter_HP);
    right = fy;
}

template <typename Field, bool fir_resampler>
inline void
resample_and_mix_inner(sample_t *left, sample_t *right, modplug::tracker::voice_ty &source, size_t length/*, uint32_t flags*/) {
    using namespace modplug::pervasives;
    using namespace modplug::tracker;
    const typename Field::FieldTy * in_sample = modplug::tracker::fetch_buf<Field>(source.active_sample_data);
    int pos = source.sample_position;
    if (bitset_is_set(source.flags, vflag_ty::Stereo)) in_sample += source.sample_position;
    //if (bitset_is_set(source.flags, vflag_ty::Stereo)) pos += source.sample_position;
    //filterstate_t filterstate;

    //init_filterstate(filterstate, source);

    //printf("length = %d\n", length);
    int fixedpt_pos = source.fractional_sample_position;
    for (size_t idx = 0; idx < length; ++idx) {
        sample_t left_smp = 0;
        sample_t right_smp = 0;
        if (fir_resampler) {
            resample_and_mix_windowed_sinc<Field>(left_smp, right_smp, source, in_sample, pos, fixedpt_pos);
        } else {
            resample_and_mix_no_interpolation<Field>(left_smp, right_smp, source, in_sample, pos, fixedpt_pos);
        }

        //apply_filter(left_smp, right_smp, source, filterstate);
        const auto vol_scale = static_cast<sample_t>(4096.0);

        *left  += static_cast<sample_t>(source.left_volume)  / vol_scale * left_smp;
        *right += static_cast<sample_t>(source.right_volume) / vol_scale * right_smp;
        ++left;
        ++right;
        fixedpt_pos += source.position_delta;
    }
    source.sample_position += fixedpt_pos >> 16;
    source.fractional_sample_position = fixedpt_pos & 0xffff;

    //save_filterstate(filterstate, source);
}

inline void
resample_and_mix(sample_t *left, sample_t *right, modplug::tracker::voice_ty *source, size_t length) {
    using namespace modplug::pervasives;
    using namespace modplug::tracker;
    switch (source->sample_tag) {
    case stag_ty::Int8: return resample_and_mix_inner<int8_f, true>(left, right, *source, length);
    case stag_ty::Int16: return resample_and_mix_inner<int16_f, true>(left, right, *source, length);
    case stag_ty::Int24: return resample_and_mix_inner<int24_f, true>(left, right, *source, length);
    case stag_ty::Int32: return resample_and_mix_inner<int32_f, true>(left, right, *source, length);
    case stag_ty::Fp32: return resample_and_mix_inner<fp32_f, true>(left, right, *source, length);
    case stag_ty::Fp64: return resample_and_mix_inner<fp64_f, true>(left, right, *source, length);
    }
}


}
}
