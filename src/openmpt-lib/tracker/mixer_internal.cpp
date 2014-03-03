#include "stdafx.h"

#include "mixer.hpp"
#include "mixer_internal.hpp"
#include "sample.hpp"

namespace modplug {
namespace tracker {

template <typename Conv, template <class, int> class Resampler>
size_t static __forceinline
by_chan(sample_t *l, sample_t *r, size_t n, voice_ty *s) {
    using internal::mix_and_advance;
    if (bitset_is_set(s->flags, vflag_ty::Stereo)) {
        return mix_and_advance<Conv, Resampler, 2>(l, r, n, *s);
    } else {
        return mix_and_advance<Conv, Resampler, 1>(l, r, n, *s);
    }
}

template <typename Conv>
size_t static __forceinline
by_resampler(sample_t *l, sample_t *r, size_t n, voice_ty *s) {
    if (bitset_is_set(s->flags, vflag_ty::DisableInterpolation)) {
        return by_chan<Conv, resamplers::zero_order_hold>(l, r, n, s);
    } else {
        //return by_chan<Conv, resamplers::zero_order_hold>(l, r, n, s);
        //return by_chan<Conv, resamplers::linear>(l, r, n, s);
        return by_chan<Conv, resamplers::windowed_sinc>(l, r, n, s);
    }
}

size_t
mix_and_advance(sample_t *l, sample_t *r, size_t n, voice_ty *s) {
    switch (s->sample_tag) {
    case stag_ty::Int8 : return by_resampler<int8_f >(l, r, n, s);
    case stag_ty::Int16: return by_resampler<int16_f>(l, r, n, s);
    case stag_ty::Int24: return by_resampler<int24_f>(l, r, n, s);
    case stag_ty::Int32: return by_resampler<int32_f>(l, r, n, s);
    case stag_ty::Fp32 : return by_resampler<fp32_f >(l, r, n, s);
    case stag_ty::Fp64 : return by_resampler<fp64_f >(l, r, n, s);
    default            : return 0;
    }
}

}
}
