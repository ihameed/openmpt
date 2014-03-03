#pragma once

#include "mixer_internal_util.hpp"

namespace modplug {
namespace tracker {
namespace resamplers {

using modplug::mixgraph::sample_t;
using modplug::tracker::internal::lookup;

template <typename Conv, int chans>
struct zero_order_hold {
    zero_order_hold(const voice_ty &) { };

    void __forceinline
    operator () (
        sample_t & left, sample_t & right,
        const voice_ty &src,
        const sampleoffset_t pos,
        const fixedpt_t _frac,
        const typename Conv::FieldTy * const buf)
    {
        static_assert(chans >= 1, "chans must be > 1!");
        static_assert(chans <= 2, "chans must be <= 2 with the interleaved mixer!");
        if (chans == 1) {
            left = right = normalize_single<Conv>(buf[pos * chans]);
        } else if (chans == 2) {
            left = normalize_single<Conv>(buf[pos * chans]);
            right = normalize_single<Conv>(buf[1 + pos * chans]);
        }
    }
};

const auto FracPos = static_cast<sample_t>(1.0 / 65536);

template <typename Conv, template <class> class Lookup, int chans>
sample_t __forceinline
linear_interp(
    const int chan,
    const sampleoffset_t pos,
    const fixedpt_t frac,
    const voice_ty &src,
    const typename Conv::FieldTy * const buf
    )
{
    Lookup<Conv> lookup;
    if (pos >= (src.length - 1)) {
        const auto a = normalize_single<Conv>(buf[pos * chans]);
        const auto b = lookup(src, 1 + pos * chans, buf);
        return a + (b - a) * (frac * FracPos);
    } else {
        const auto a = normalize_single<Conv>(buf[pos * chans]);
        const auto b = normalize_single<Conv>(buf[1 + pos * chans]);
        return a + (b - a) * (frac * FracPos);
    }
}


template <typename Conv, int chans>
struct linear {
    linear(const voice_ty &) { };

    void __forceinline
    operator () (
        sample_t & left, sample_t & right,
        const voice_ty &src,
        const sampleoffset_t pos,
        const fixedpt_t frac,
        const typename Conv::FieldTy * const buf)
    {
        static_assert(chans >= 1, "chans must be > 1!");
        static_assert(chans <= 2, "chans must be <= 2 with the interleaved mixer!");
        if (chans == 1) {
            left = right = linear_interp<Conv, lookup, chans>(0, pos, frac, src, buf);
        } else if (chans == 2) {
            left  = linear_interp<Conv, lookup, chans>(0, pos, frac, src, buf);
            right = linear_interp<Conv, lookup, chans>(1, pos, frac, src, buf);
        }
    }
};

template <typename Conv, template <class> class Lookup, int chans>
sample_t __forceinline
convolve_with_kernel(
    const int chan,
    const sampleoffset_t pos,
    const voice_ty &src,
    const sample_t * const fir_table,
    const typename Conv::FieldTy * const buf
    )
{
    Lookup<Conv> lookup;
    sample_t sample = 0;
    if ((pos < (src.loop_start + 3)) || (pos >= (src.length - 4))) {
        sample += fir_table[0] * (lookup(src, chan + (pos - 3) * chans, buf));
        sample += fir_table[1] * (lookup(src, chan + (pos - 2) * chans, buf));
        sample += fir_table[2] * (lookup(src, chan + (pos - 1) * chans, buf));
        sample += fir_table[3] * (normalize_single<Conv>(buf[chan + pos * chans]));
        sample += fir_table[4] * (lookup(src, chan + (pos + 1) * chans, buf));
        sample += fir_table[5] * (lookup(src, chan + (pos + 2) * chans, buf));
        sample += fir_table[6] * (lookup(src, chan + (pos + 3) * chans, buf));
        sample += fir_table[7] * (lookup(src, chan + (pos + 4) * chans, buf));
    } else {
        sample += fir_table[0] * (normalize_single<Conv>(buf[chan + (pos - 3) * chans]));
        sample += fir_table[1] * (normalize_single<Conv>(buf[chan + (pos - 2) * chans]));
        sample += fir_table[2] * (normalize_single<Conv>(buf[chan + (pos - 1) * chans]));
        sample += fir_table[3] * (normalize_single<Conv>(buf[chan + pos * chans]));
        sample += fir_table[4] * (normalize_single<Conv>(buf[chan + (pos + 1) * chans]));
        sample += fir_table[5] * (normalize_single<Conv>(buf[chan + (pos + 2) * chans]));
        sample += fir_table[6] * (normalize_single<Conv>(buf[chan + (pos + 3) * chans]));
        sample += fir_table[7] * (normalize_single<Conv>(buf[chan + (pos + 4) * chans]));
    }
    return sample;
}

template <typename Conv, int chans>
struct windowed_sinc {
    const sample_t * const table;
    windowed_sinc(const voice_ty &src)
        : table( (src.position_delta > 0x13000 || src.position_delta < -0x13000)
            ? downsample_sinc_table
            : upsample_sinc_table)
    { }

    void __forceinline
    operator () (
        sample_t & left, sample_t & right,
        const voice_ty &src,
        const sampleoffset_t pos,
        const fixedpt_t frac,
        const typename Conv::FieldTy * const buf)
    {
        using modplug::mixgraph::FIR_TAPS;
        const auto fir_idx = ((frac & 0xfff0) >> 4) * FIR_TAPS;
        const auto fir_table = table + fir_idx;
        static_assert(chans >= 1, "chans must be > 1!");
        static_assert(chans <= 2, "chans must be <= 2 with the interleaved mixer!");
        if (chans == 1) {
            left = convolve_with_kernel<Conv, lookup, chans>(0, pos, src, fir_table, buf);
            right = left;
        } else if (chans == 2) {
            left  = convolve_with_kernel<Conv, lookup, chans>(0, pos, src, fir_table, buf);
            right = convolve_with_kernel<Conv, lookup, chans>(1, pos, src, fir_table, buf);
        }
    }
};


}
}
}
