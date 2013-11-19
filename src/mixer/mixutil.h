/*
cmixer.h nabbed from schism
rudely sepple-ized and jammed into openmpt by xaimus  : - [
*/

#pragma once

#include <cstddef>
#include "../tracker/tracker.hpp"


namespace modplug {
namespace mixer {


void init_mix_buffer(int *, size_t);
void stereo_fill(int *, size_t, long *, long *);
void end_channel_ofs(modplug::tracker::modchannel_t *, int *, size_t);
void interleave_front_rear(int *, int *, size_t);
void mono_from_stereo(int *, size_t);

template <typename fp_t>
void stereo_mix_to_float(const int *src, fp_t *out1, fp_t *out2, size_t count, const fp_t i2fc) {
    for (size_t i = 0; i < count; i++) {
        *out1++ = *src * i2fc;
        src++;

        *out2++ = *src * i2fc;
        src++;
    }
}

template <typename fp_t>
void float_to_stereo_mix(const fp_t *in1, const fp_t *in2, int *out, size_t count, const fp_t f2ic) {
    for (size_t i = 0; i < count; i++) {
        *out++ = (int) (*in1 * f2ic);
        *out++ = (int) (*in2 * f2ic);
        in1++;
        in2++;
    }
}
void mono_mix_to_float(const int *, float *, size_t, const float);
void float_to_mono_mix(const float *, int *, size_t, const float);

size_t clip_32_to_8(void *, int *, size_t);
size_t clip_32_to_16(void *, int *, size_t);
size_t clip_32_to_24(void *, int *, size_t);
size_t clip_32_to_32(void *, int *, size_t);


}
}
