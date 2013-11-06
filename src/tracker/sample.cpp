#include "stdafx.h"

#include <cmath>

#include "sample.h"

#include "../pervasives/pervasives.h"

using namespace modplug::pervasives;

namespace modplug {
namespace tracker {

const double ln2 = 0.6931471805599453094172321214581765680755001343602552;
const double ref_freq_hz = 8363.0;
const double xm_microtones = 128.0;

uint32_t
frequency_of_transpose(int8_t semitones, int8_t microtones) {
    const double distance = semitones + (microtones / xm_microtones);
    return static_cast<uint32_t>(pow(2, distance / 12));
}

std::tuple<int8_t, int8_t>
transpose_of_frequency(uint32_t freq) {
    if (freq == 0) return std::make_tuple(ref_freq_hz, 0);
    const double distance = 12 * (log(freq / ref_freq_hz) / ln2);
    auto int8_clamp = [] (double val) {
        return static_cast<int8_t>(clamp(val, INT8_MIN, INT8_MAX));
    };
    int8_t semitones = int8_clamp(distance);
    int8_t microtones = int8_clamp((distance - floor(distance)) * xm_microtones);
    return std::make_tuple(semitones, microtones);
}

}
}
