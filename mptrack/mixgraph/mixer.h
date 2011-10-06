#pragma once

#include "constants.h"
#include "../tracker/tracker.h"
#include <cmath>
#include <cstdint>

namespace modplug {
namespace mixgraph {


static const double PI = 3.14159265358979323846;
static const size_t FIR_TAPS = 8;
static const size_t FIR_CENTER = (FIR_TAPS / 2) - 1;
static const size_t TAP_PHASES = 4096;
static const size_t SINC_SIZE = FIR_TAPS * TAP_PHASES;
static const sample_t NORM_INT16 = 1.0 / INT16_MAX;
static const sample_t NORM_INT8  = 1.0 / INT8_MAX;

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

inline void init_filterstate(filterstate_t &butt_abs, modplug::tracker::modchannel_t *source) {
    if (!(source->flags & CHN_FILTER)) return;
    butt_abs.left.y1 = source->nFilter_Y1;
    butt_abs.left.y2 = source->nFilter_Y2;
    butt_abs.right.y1 = source->nFilter_Y3;
    butt_abs.right.y2 = source->nFilter_Y4;
}

inline void save_filterstate(filterstate_t &butt_abs, modplug::tracker::modchannel_t *source) {
    if (!(source->flags & CHN_FILTER)) return;
    source->nFilter_Y1 = butt_abs.left.y1;
    source->nFilter_Y2 = butt_abs.left.y2;
    source->nFilter_Y3 = butt_abs.right.y1;
    source->nFilter_Y4 = butt_abs.right.y2;
}

inline sample_t convert_sample(const int16_t *derp, int idx) {
    //if (idx < 0) return 0;
    return derp[idx] * NORM_INT16;
}

inline sample_t convert_sample(const int8_t *derp, int idx) {
    //if (idx < 0) return 0;
    return derp[idx] * NORM_INT8;
}

template <typename buf_t>
inline void resample_and_mix_no_interpolation(sample_t &left, sample_t &right, modplug::tracker::modchannel_t *source, buf_t in_sample, int smp_pos, int fixedpt_pos) {
    int pos = smp_pos + (fixedpt_pos >> 16);
    if (source->flags & CHN_STEREO) {
        left  = convert_sample(in_sample, pos * 2);
        right = convert_sample(in_sample, (pos * 2) + 1);
    } else {
        left  = convert_sample(in_sample, pos);
        right = left;
    }
}

template <typename buf_t>
inline void resample_and_mix_windowed_sinc(sample_t &left, sample_t &right, modplug::tracker::modchannel_t *source, buf_t in_sample, int smp_pos, int fixedpt_pos) {
    int pos = smp_pos + (fixedpt_pos >> 16);
    int fir_idx = (fixedpt_pos & 0xfff0) >> 1;
    //fir_idx = fir_idx << 3;
    //fir_idx = fir_idx << 1; //log_2(FIR_TAPS / 8);

    const double *fir_table =
        ((source->position_delta > 0x13000 || source->position_delta < -0x13000) ?
            (downsample_sinc_table) :
            (upsample_sinc_table))
        + fir_idx;
    //const double *fir_table = downsample_sinc_table + fir_idx;

    if (source->flags & CHN_STEREO) {
        for (size_t tap = 0; tap < FIR_TAPS; ++tap) {
            left  += fir_table[tap] * convert_sample(in_sample, (pos + tap - FIR_CENTER) * 2);
            right += fir_table[tap] * convert_sample(in_sample, (pos + tap - FIR_CENTER) * 2 + 1);
        }
    } else {
        for (size_t tap = 0; tap < FIR_TAPS; ++tap) {
            left += fir_table[tap] * convert_sample(in_sample, pos + tap - FIR_CENTER);
        }
        right = left;
    }
}

inline void apply_filter(sample_t &left, sample_t &right, modplug::tracker::modchannel_t *source, filterstate_t &fst) {
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
inline void hurfa(sample_t *left, sample_t *right, modplug::tracker::modchannel_t *source, size_t length/*, uint32_t flags*/) {
    const buf_t *in_sample = reinterpret_cast<buf_t *>(source->active_sample_data);
    int pos = source->sample_position;
    if (source->flags & CHN_STEREO) in_sample += source->sample_position;
    filterstate_t filterstate;

    init_filterstate(filterstate, source);

    int fixedpt_pos = source->fractional_sample_position;
    for (size_t idx = 0; idx < length; ++idx) {
        double left_smp = 0;
        double right_smp = 0;
        if (fir_resampler) {
            resample_and_mix_windowed_sinc(left_smp, right_smp, source, in_sample, pos, fixedpt_pos);
        } else {
            resample_and_mix_no_interpolation(left_smp, right_smp, source, in_sample, pos, fixedpt_pos);
        }

        apply_filter(left_smp, right_smp, source, filterstate);

        *left  += static_cast<double>(source->left_volume)  / 4096.0 * left_smp;
        *right += static_cast<double>(source->right_volume) / 4096.0 * right_smp;
        ++left;
        ++right;
        fixedpt_pos += source->position_delta;
    }
    source->sample_position += fixedpt_pos >> 16;
    source->fractional_sample_position = fixedpt_pos & 0xffff;

    save_filterstate(filterstate, source);
}

inline void resample_and_mix(sample_t *left, sample_t *right, modplug::tracker::modchannel_t *source, size_t length) {
    if (source->flags & CHN_16BIT) {
        hurfa<int16_t, true>(left, right, source, length);
    } else {
        hurfa<int8_t, true>(left, right, source, length);
    }
}


}
}
