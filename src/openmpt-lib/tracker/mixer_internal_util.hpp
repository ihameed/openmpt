#pragma once

#include "types.hpp"
#include "voice.hpp"
#include "../pervasives/bitset.hpp"

#include "../mixgraph/mixer.hpp"

namespace modplug {
namespace tracker {
namespace internal {

using modplug::pervasives::bitset_is_set;
using modplug::mixgraph::sample_t;

template <typename Ty>
Ty __forceinline
lookup_(const voice_ty &chan, const int32_t idx, const Ty * const buf) {
    if (idx >= chan.length) {
        if (bitset_is_set(chan.flags, vflag_ty::BidiLoop)) {
            const auto looplen = chan.length - chan.loop_start - 1;
            const auto full = 2 * looplen;
            const auto mod = (looplen + idx - (chan.length - 1)) % full;
            return buf[chan.loop_start + (mod <= looplen ? mod : (full - mod))];
        } else if (bitset_is_set(chan.flags, vflag_ty::BidiLoop)) {
            return buf[(idx - chan.length) + chan.loop_start];
        } else {
            return 0;
        }
    } else if (idx < chan.loop_start) {
        if (chan.position_delta >= 0 && idx >= 0) {
            return buf[idx];
        } else if (bitset_is_set(chan.flags, vflag_ty::BidiLoop)) {
            const auto looplen = chan.length - chan.loop_start - 1;
            const auto full = 2 * looplen;
            const auto mod = (chan.loop_start - idx) % full;
            return buf[chan.loop_start + (mod <= looplen ? mod : (full - mod))];
        } else {
            return 0;
        }
    }
    return buf[idx];
}

template <typename ConvTy>
struct lookup {
    sample_t __forceinline
    operator () (const voice_ty &chan, const int32_t idx,
        typename const ConvTy::FieldTy * const buf)
    {
        return modplug::tracker::normalize_single<ConvTy>(lookup_(chan, idx, buf));
    }
};

template <typename ConvTy>
struct lookup_spill {
    sample_t __forceinline
    operator () (const voice_ty &chan, const int32_t idx,
        typename const ConvTy::FieldTy * const buf)
    {
        const auto length = chan.length;
        const auto loop_start = chan.loop_start;
        if (idx >= length) {
            if (bitset_is_set(chan.flags, vflag_ty::Loop) && ((idx - length) < SpillMax)) {
                const auto i = idx - length;
                return chan.spill_fwd[i];
            } else {
                return 0;
            }
        } else if (idx < loop_start) {
            if (bitset_is_set(chan.flags, vflag_ty::Loop) && ((loop_start - idx) < SpillMax)) {
                const auto i = loop_start - idx;
                return chan.spill_back[i];
            } else {
                return 0;
            }
        }
        return modplug::tracker::normalize_single<ConvTy>(buf[idx]);
    }
};


}
}
}
