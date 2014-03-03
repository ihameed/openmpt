#pragma once

#include "constants.hpp"
#include "../tracker/tracker.hpp"
#include "../tracker/voice.hpp"
#include "../tracker/mixer_internal_filter.hpp"
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

namespace internal {

using namespace modplug::tracker;
using namespace modplug::pervasives;

void __forceinline
init_filterstate(flt_lr_ty &state, voice_ty &source) {
    if (!bitset_is_set(source.flags, vflag_ty::Filter)) return;
    state = source.flt_hist;
}

void __forceinline
save_filterstate(flt_lr_ty &state, voice_ty &source) {
    if (!bitset_is_set(source.flags, vflag_ty::Filter)) return;
    source.flt_hist = state;
}

template <typename Field>
sample_t __forceinline
convert_sample(
    const typename Field::FieldTy * const derp,
    const voice_ty &source, int idx)
{
    if (idx < 0) return 0;
    if (idx >= source.length) return 0;
    return normalize_single<Field>(derp[idx]);
}

template <typename Field>
void __forceinline
resample_and_mix_no_interpolation(
    sample_t &left,
    sample_t &right,
    voice_ty &source,
    typename const Field::FieldTy * const in_sample,
    int smp_pos,
    int fixedpt_pos)
{
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
    voice_ty &source,
    const typename Field::FieldTy * const in_sample,
    int smp_pos,
    int fixedpt_pos)
{
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
apply_filter(sample_t &left, sample_t &right, voice_ty &source, flt_lr_ty &fst) {
    using modplug::tracker::filter::apply_filter;
    if (!bitset_is_set(source.flags, vflag_ty::Filter)) return;
    apply_filter(left, fst.left, source.flt_coefs);
    apply_filter(right, fst.right, source.flt_coefs);
}

template <typename Field, bool fir_resampler>
inline void
resample_and_mix_inner(sample_t *left, sample_t *right, voice_ty &source, size_t length/*, uint32_t flags*/) {
    const typename Field::FieldTy * in_sample = fetch_buf<Field>(source.active_sample_data);
    int pos = source.sample_position;
    if (bitset_is_set(source.flags, vflag_ty::Stereo)) in_sample += source.sample_position;
    flt_lr_ty filterstate;

    init_filterstate(filterstate, source);

    int fixedpt_pos = source.fractional_sample_position;
    for (size_t idx = 0; idx < length; ++idx) {
        sample_t left_smp = 0;
        sample_t right_smp = 0;
        if (fir_resampler) {
            resample_and_mix_windowed_sinc<Field>(left_smp, right_smp, source, in_sample, pos, fixedpt_pos);
        } else {
            resample_and_mix_no_interpolation<Field>(left_smp, right_smp, source, in_sample, pos, fixedpt_pos);
        }

        apply_filter(left_smp, right_smp, source, filterstate);
        const auto vol_scale = static_cast<sample_t>(4096.0);

        *left  += static_cast<sample_t>(source.left_volume)  / vol_scale * left_smp;
        *right += static_cast<sample_t>(source.right_volume) / vol_scale * right_smp;
        ++left;
        ++right;
        fixedpt_pos += source.position_delta;
    }
    source.sample_position += fixedpt_pos >> 16;
    source.fractional_sample_position = fixedpt_pos & 0xffff;

    save_filterstate(filterstate, source);
}

}

inline void
resample_and_mix(sample_t *left, sample_t *right, modplug::tracker::voice_ty *source, size_t length) {
    using namespace modplug::tracker;
    using modplug::mixgraph::internal::resample_and_mix_inner;
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
