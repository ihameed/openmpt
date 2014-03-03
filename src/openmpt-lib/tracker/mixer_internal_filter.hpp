#pragma once

#include "voice.hpp"

#include "../mixgraph/constants.hpp"

namespace modplug {
namespace tracker {
namespace filter {

using modplug::mixgraph::sample_t;

void __forceinline
apply_filter(sample_t &out, flt_hist_ty &hist, const flt_coefs_ty &coef)  {
    auto fy = out * coef.a0 + hist.y1 * coef.b0 + hist.y2 * coef.b1;
    hist.y2 = hist.y1;
    hist.y1 = fy - (out * coef.hp);
    out = fy;
}

struct basic {
    const bool enabled;
    flt_lr_ty hist;
    voice_ty &src;

    __forceinline
    basic(voice_ty &src)
        : enabled(bitset_is_set(src.flags, vflag_ty::Filter))
        , src(src)
    { if (enabled) { hist = src.flt_hist; } }

    __forceinline
    ~basic() { if (enabled) { src.flt_hist = hist; } }

    void __forceinline
    operator () (sample_t &left, sample_t &right) {
        using namespace modplug::pervasives;
        using namespace modplug::tracker;
        if (!enabled) return;
        apply_filter(left, hist.left, src.flt_coefs);
        apply_filter(right, hist.right, src.flt_coefs);
    }
};

}
}
}
