/*
cmixer.h nabbed from schism
rudely sepple-ized and jammed into openmpt by xaimus  : - [
*/

#pragma once

#include <cstddef>
#include "../../soundlib/sndfile.h"

struct modchannel;

namespace modplug {
namespace mixer {


void init_mix_buffer(int *, size_t);
void stereo_fill(int *, size_t, long *, long *);
void end_channel_ofs(MODCHANNEL *, int *, size_t);
void interleave_front_rear(int *, int *, size_t);
void mono_from_stereo(int *, size_t);

void stereo_mix_to_sample_t(const int *, modplug::graph::sample_t *, modplug::graph::sample_t *, size_t, const float);

void stereo_mix_to_float(const int *, float *, float *, size_t, const float);
void float_to_stereo_mix(const float *, const float *, int *, size_t, const float);
void mono_mix_to_float(const int *, float *, size_t, const float);
void float_to_mono_mix(const float *, int *, size_t, const float);

size_t clip_32_to_8(void *, int *, size_t);
size_t clip_32_to_16(void *, int *, size_t);
size_t clip_32_to_24(void *, int *, size_t);
size_t clip_32_to_32(void *, int *, size_t);


}
}