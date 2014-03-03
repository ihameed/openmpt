#pragma once

#include "mixer_internal_filter.hpp"
#include "mixer_internal_resample.hpp"

namespace modplug {
namespace tracker {
namespace internal {

using namespace modplug::pervasives;
using namespace modplug::mixgraph;

void __forceinline
voice_scrub_backwards(sampleoffset_t &pos, fixedpt_t &frac, fixedpt_t &delta, voice_ty &src) {
    frac = 0x10000 - frac;

    const auto looplen = src.length - src.loop_start - 1;
    const auto full = 2 * looplen;
    const auto mod = (looplen + pos - (src.length - 1)) % full;
    pos = src.loop_start + (mod <= looplen ? mod : (full - mod));

    frac &= 0xffff;
    delta = -delta;
    bitset_add(src.flags, vflag_ty::ScrubBackwards);
}

void __forceinline
voice_scrub_forwards(sampleoffset_t &pos, fixedpt_t &frac, fixedpt_t &delta, voice_ty &src) {
    frac = 0x10000 - frac;

    const auto looplen = src.length - src.loop_start - 1;
    const auto full = 2 * looplen;
    const auto mod = (src.loop_start - pos) % full;
    pos = src.loop_start + (mod <= looplen ? mod : (full - mod));

    frac &= 0xffff;
    delta = -delta;
    bitset_remove(src.flags, vflag_ty::ScrubBackwards);
}

void __forceinline
voice_forward_loop(sampleoffset_t &pos, fixedpt_t &frac, fixedpt_t &delta, voice_ty &src) {
    //pos = src.loop_start + (src.length - pos);
    const auto rel = pos - src.loop_start;
    pos = src.loop_start + (rel % (src.length - src.loop_start));
}

const sample_t VolScale = 1.0 / 4096.0;

template <typename Conv, template <class, int> class Resampler, int chans>
size_t
mix_and_advance(sample_t *left, sample_t *right, size_t out_frames, voice_ty &src) {
    Resampler<Conv, chans> resample(src);
    filter::basic filter(src);
    const auto oldleft = left;
    const auto oldright = right;
    const auto buf = fetch_buf<Conv>(src.active_sample_data);
    const auto left_vol = static_cast<sample_t>(src.left_volume);
    const auto right_vol = static_cast<sample_t>(src.right_volume);
    auto pos = src.sample_position;
    auto frac = src.fractional_sample_position;
    auto delta = src.position_delta;

    while (out_frames--) {
        sample_t left_smp = 0;
        sample_t right_smp = 0;
        resample(left_smp, right_smp, src, pos, frac, buf);
        filter(left_smp, right_smp);
        *left += left_vol * VolScale * left_smp;
        *right += right_vol * VolScale * right_smp;
        ++left;
        ++right;
        {
            const auto newfrac = frac + delta;
            pos += newfrac >> 16;
            frac = newfrac & 0xffff;
        }
        if (pos >= src.length) {
            if (bitset_is_set(src.flags, vflag_ty::BidiLoop)) {
                voice_scrub_backwards(pos, frac, delta, src);
                continue;
            } else if (bitset_is_set(src.flags, vflag_ty::Loop)) {
                voice_forward_loop(pos, frac, delta, src);
                continue;
            } else {
                break;
            }
        } else if (pos < src.loop_start) {
            if (bitset_is_set(src.flags, vflag_ty::ScrubBackwards) && (delta < 0)) {
                voice_scrub_forwards(pos, frac, delta, src);
                continue;
            } else if (delta > 0) {
                continue;
            } else {
                break;
            }
        }
    }
    /*
    invariant: src.fractional_sample_position \in [0, 0xffff]
    */
    src.sample_position = pos;
    src.fractional_sample_position = frac;
    src.position_delta = delta;
    return out_frames + 1;
}

}
}
}
