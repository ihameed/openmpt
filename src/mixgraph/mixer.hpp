#pragma once

#include "constants.hpp"
#include "../tracker/tracker.hpp"
#include <cmath>
#include <cstdint>

namespace modplug {
namespace mixgraph {


static const double PI = 3.14159265358979323846;
static const size_t FIR_TAPS = 64;
static const size_t FIR_CENTER = (FIR_TAPS / 2) - 1;
static const size_t TAP_PHASES = 4096;
static const size_t SINC_SIZE = FIR_TAPS * TAP_PHASES;
static const sample_t NORM_INT16 = static_cast<sample_t>(1.0 / INT16_MAX);
static const sample_t NORM_INT8  = static_cast<sample_t>(1.0 / INT8_MAX);

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

inline void
init_filterstate(filterstate_t &state, modplug::tracker::modchannel_t *source) {
    if (!(source->flags & CHN_FILTER)) return;
    state.left.y1 = source->nFilter_Y1;
    state.left.y2 = source->nFilter_Y2;
    state.right.y1 = source->nFilter_Y3;
    state.right.y2 = source->nFilter_Y4;
}

inline void
save_filterstate(filterstate_t &state, modplug::tracker::modchannel_t *source) {
    if (!(source->flags & CHN_FILTER)) return;
    source->nFilter_Y1 = state.left.y1;
    source->nFilter_Y2 = state.left.y2;
    source->nFilter_Y3 = state.right.y1;
    source->nFilter_Y4 = state.right.y2;
}

inline sample_t
convert_sample(const int16_t *derp, int idx) {
    if (idx < 0) return 0;
    return derp[idx] * NORM_INT16;
}

inline sample_t
convert_sample(const int8_t *derp, int idx) {
    if (idx < 0) return 0;
    return derp[idx] * NORM_INT8;
}

template <typename buf_t>
inline void
resample_and_mix_no_interpolation(
    sample_t &left,
    sample_t &right,
    modplug::tracker::modchannel_t *source,
    buf_t in_sample,
    int smp_pos,
    int fixedpt_pos)
{
    int pos = smp_pos;
    if (source->flags & CHN_STEREO) {
        pos += (fixedpt_pos >> 16) * 2;
        left  = convert_sample(in_sample, pos);
        right = convert_sample(in_sample, pos + 1);
    } else {
        pos += (fixedpt_pos >> 16);
        left  = convert_sample(in_sample, pos);
        right = left;
    }
}

template <typename buf_t>
inline void
resample_and_mix_windowed_sinc(
    sample_t &left,
    sample_t &right,
    modplug::tracker::modchannel_t *source,
    buf_t in_sample,
    int smp_pos,
    int fixedpt_pos)
{
    int pos = smp_pos;
    int fir_idx = ((fixedpt_pos & 0xfff0) >> 4) * FIR_TAPS;

    const sample_t *fir_table =
        ((source->position_delta > 0x13000 || source->position_delta < -0x13000) ?
            (downsample_sinc_table) :
            (upsample_sinc_table))
        + fir_idx;

    if (source->flags & CHN_STEREO) {
        pos += (fixedpt_pos >> 16) * 2;
        for (size_t tap = 0; tap < FIR_TAPS; ++tap) {
            left  += fir_table[tap] *
                     convert_sample(in_sample, tap - FIR_CENTER + pos);
            right += fir_table[tap] *
                     convert_sample(in_sample, tap - FIR_CENTER + pos + 1);
        }
    } else {
        pos += (fixedpt_pos >> 16);
        for (size_t tap = 0; tap < FIR_TAPS; ++tap) {
            left += fir_table[tap] *
                    convert_sample(in_sample, tap - FIR_CENTER + pos);
        }
        right = left;
    }
}

inline void
apply_filter(sample_t &left, sample_t &right, modplug::tracker::modchannel_t *source, filterstate_t &fst) {
    if (!(source->flags & CHN_FILTER)) return;
    sample_t fy;

    fy = left * source->nFilter_A0 + fst.left.y1 * source->nFilter_B0 + fst.left.y2 * source->nFilter_B1;
    fst.left.y2 = fst.left.y1;
    fst.left.y1 = fy - (left * source->nFilter_HP);
    left = fy;

    fy = right * source->nFilter_A0 + fst.right.y1 * source->nFilter_B0 + fst.right.y2 * source->nFilter_B1;
    fst.right.y2 = fst.right.y1;
    fst.right.y1 = fy - (right * source->nFilter_HP);
    right = fy;
}

template <typename buf_t, bool fir_resampler>
inline void
resample_and_mix_inner(sample_t *left, sample_t *right, modplug::tracker::modchannel_t *source, size_t length/*, uint32_t flags*/) {
    const buf_t *in_sample = reinterpret_cast<buf_t *>(source->active_sample_data);
    int pos = source->sample_position;
    if (source->flags & CHN_STEREO) in_sample += source->sample_position;
    filterstate_t filterstate;

    init_filterstate(filterstate, source);

    int fixedpt_pos = source->fractional_sample_position;
    for (size_t idx = 0; idx < length; ++idx) {
        sample_t left_smp = 0;
        sample_t right_smp = 0;
        if (fir_resampler) {
            resample_and_mix_windowed_sinc(left_smp, right_smp, source, in_sample, pos, fixedpt_pos);
        } else {
            resample_and_mix_no_interpolation(left_smp, right_smp, source, in_sample, pos, fixedpt_pos);
        }

        apply_filter(left_smp, right_smp, source, filterstate);
        const auto vol_scale = static_cast<sample_t>(4096.0);

        *left  += static_cast<sample_t>(source->left_volume)  / vol_scale * left_smp;
        *right += static_cast<sample_t>(source->right_volume) / vol_scale * right_smp;
        ++left;
        ++right;
        fixedpt_pos += source->position_delta;
    }
    source->sample_position += fixedpt_pos >> 16;
    source->fractional_sample_position = fixedpt_pos & 0xffff;

    save_filterstate(filterstate, source);
}

inline void
resample_and_mix(sample_t *left, sample_t *right, modplug::tracker::modchannel_t *source, size_t length) {
    if (source->flags & CHN_16BIT) {
        resample_and_mix_inner<int16_t, true>(left, right, source, length);
    } else {
        resample_and_mix_inner<int8_t, true>(left, right, source, length);
    }
}


}
}
