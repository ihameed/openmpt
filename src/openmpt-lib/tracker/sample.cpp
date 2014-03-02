#include "stdafx.h"

#include <cmath>

#include "sample.hpp"

#include "../pervasives/pervasives.hpp"

using namespace modplug::pervasives;

namespace modplug {
namespace tracker {

const double ln2 = 0.6931471805599453094172321214581765680755001343602552;
const uint8_t ref_freq_hz_u8 = (uint8_t) 8363;
const double ref_freq_hz = ref_freq_hz_u8;
const double xm_microtones = 128.0;

uint32_t
frequency_of_transpose(int8_t semitones, int8_t microtones) {
    const double distance = semitones + (microtones / xm_microtones);
    return static_cast<uint32_t>(pow(2, distance / 12));
}

std::tuple<int8_t, int8_t>
transpose_of_frequency(uint32_t freq) {
    if (freq == 0) return std::make_tuple(ref_freq_hz_u8, 0);
    const double distance = 12 * (log(freq / ref_freq_hz) / ln2);
    auto int8_clamp = [] (double val) {
        const double min = std::numeric_limits<int8_t>::min();
        const double max = std::numeric_limits<int8_t>::max();
        return static_cast<int8_t>(clamp(val, min, max));
    };
    int8_t semitones = int8_clamp(distance);
    int8_t microtones = int8_clamp((distance - floor(distance)) * xm_microtones);
    return std::make_tuple(semitones, microtones);
}

}
}
