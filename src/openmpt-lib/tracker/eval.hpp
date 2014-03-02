#pragma once

#include <cstdint>

#include "tracker.hpp"

#include "../mixgraph/constants.hpp"
#include "../mixgraph/core.hpp"

#include "../legacy_soundlib/Snd_defs.h"

namespace modplug {
namespace tracker {

enum struct it_bidi_mode_ty : uint8_t {
    UsingItBidiMode,
    NotUsingItBidiMode
};

enum struct tempo_mode_ty : uint8_t {
    Modern,
    Classic,
    Alternative
};

struct pattern_ty {
};

struct tick_width_ty {
    uint32_t width;
    double error;
};

const tick_width_ty empty_tick_width = { 0, 0 };

struct playback_pos_ty {
    uint32_t current_order;
    uint32_t current_pattern;
    uint32_t current_row;
    uint32_t current_tick;
};

const playback_pos_ty default_pos = { 0, 0, 0, 0 };

struct eval_state_ty {
    playback_pos_ty pos;

    uint32_t tempo;
    uint32_t ticks_per_row;
    uint32_t rows_per_beat;

    uint32_t mixing_freq;

    uint32_t sample_pre_amp;

    tick_width_ty tick_width;

    tempo_mode_ty tempo_mode;
    bool processing_first_tick;

    std::array<voice_ty, MAX_VIRTUAL_CHANNELS> *channels;
    std::array<uint32_t, MAX_VIRTUAL_CHANNELS> *mix_channels;
    std::array<initial_voice_settings_ty, MAX_BASECHANNELS> *chan_settings;

    pattern_ty *patterns;

    modplug::mixgraph::core *mixgraph;
};

double inline
fp64_of_16_16(const int32_t val) {
    return static_cast<double>(val) / pow(2, 16);
}

void
advance_silently(voice_ty &, const int32_t);

uint32_t
eval_pattern(int16_t * const, const size_t, eval_state_ty &);

}
}
