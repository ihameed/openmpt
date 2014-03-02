#include "stdafx.h"

#include <cassert>
#include <cstdint>
#include <array>

#include "eval.hpp"

#include "../pervasives/bitset.hpp"
#include "../pervasives/pervasives.hpp"

#include "../mixgraph/core.hpp"
#include "../mixgraph/mixer.hpp"

#include "legacy_constants.hpp"
#include "../legacy_soundlib/Snd_defs.h"

using modplug::mixgraph::sample_t;
using namespace modplug::pervasives;

namespace modplug {
namespace tracker {

int
pattern_num_rows(const pattern_ty &pat) { return 0; }

tick_width_ty
calc_tick_width_modern(const eval_state_ty &state) {
    const auto tempo = static_cast<double>(state.tempo);
    const auto ticks_per_row = static_cast<double>(state.ticks_per_row);
    const auto rows_per_beat = static_cast<double>(state.rows_per_beat);
    const auto mixing_freq = static_cast<double>(state.mixing_freq);
    const auto width = state.mixing_freq * ((60.0 / tempo) / (ticks_per_row * rows_per_beat));

    auto trunc_width = static_cast<uint32_t>(width);
    auto trunc_error = state.tick_width.error + (width - trunc_width);
    if (trunc_error >= 1) {
        ++trunc_width; --trunc_error;
    } else if (trunc_error <= -1) {
        --trunc_width; ++trunc_error;
    }
    const tick_width_ty ret = { trunc_width, trunc_error };
    return ret;
}

tick_width_ty
calc_tick_width_classic(const eval_state_ty &state) {
    const tick_width_ty ret = { (state.mixing_freq * 5) / (state.tempo * 2), 0 };
    return ret;
}

tick_width_ty
calc_tick_width_alternative(const eval_state_ty &state) {
    const tick_width_ty ret = { state.mixing_freq / state.tempo, 0 };
    return ret;
}

tick_width_ty
calc_tick_width(const eval_state_ty &state) {
    switch (state.tempo_mode) {
        case tempo_mode_ty::Modern: return calc_tick_width_modern(state);
        case tempo_mode_ty::Classic: return calc_tick_width_classic(state);
        case tempo_mode_ty::Alternative: return calc_tick_width_alternative(state);
        default: assert(false); return empty_tick_width;
    }
}

void
eval_row(eval_state_ty &state) {
    if (state.pos.current_tick >= state.ticks_per_row) {
        // do new-row stuff
    }

    if (state.ticks_per_row == 0) state.ticks_per_row = 1;
    state.processing_first_tick = true;

    if (state.pos.current_tick > 0) {
        state.processing_first_tick = false;
    }
}

void inline
apply_pan_swing(modchannel_t &channel) {
    channel.nPan += channel.nPanSwing;
    channel.nPan = clamp(channel.nPan, 0, 256);
    channel.nPanSwing = 0;
    channel.nRealPan = channel.nPan;
}

void inline
apply_vol_swing(modchannel_t &channel) {
    channel.nVolume += channel.nVolSwing;
    channel.nVolume = clamp(channel.nVolume, 0, 256);
    channel.nVolSwing = 0;
}

void inline
apply_tremolo(modchannel_t &channel) {
}

void inline
apply_tremor(modchannel_t &channel) {
}

void inline
apply_arpeggio(modchannel_t &channel) {
}

void inline
clamp_freq_to_amiga_limits(modchannel_t &channel) {
}

int inline
env_value_linear_interp(modenvelope_t &env, modenvstate_t &pos) {
    return -200;
}

int inline
apply_vol_env(int vol, modchannel_t &channel, const modinstrument_t &instr) {
    const bool should_process =
        bitset_is_set(channel.flags, vflag_ty::VolEnvOn) ||
        (channel.instrument->volume_envelope.flags & ENV_ENABLED) ||
        (channel.instrument->volume_envelope.num_nodes > 0);
        // IsCompatibleMode(TRK_IMPULSETRACKER)
    if (should_process) {
        auto scale = env_value_linear_interp(
            channel.instrument->volume_envelope, channel.volume_envelope);
        return vol * clamp(scale, 0, 512);
    }
    return vol;
}

int inline
apply_instrument(int vol, modchannel_t &channel) {
    const auto instr = channel.instrument;
    if (instr) {
        apply_vol_env(vol, channel, *instr);
    }
    return -100;
}

void inline
eval_channel(eval_state_ty &state, modchannel_t &channel) {
    channel.position_delta = 0;
    channel.nRealVolume = 0;

    apply_pan_swing(channel);
    if (channel.nPeriod != 0 && channel.length > 0) {
        apply_vol_swing(channel);
        auto vol = channel.nVolume;
        apply_tremolo(channel);
        apply_tremor(channel);
        channel.nRealVolume = vol;
        apply_arpeggio(channel);
        clamp_freq_to_amiga_limits(channel);
        vol = apply_instrument(vol, channel);
    }
}

void
eval_channels(eval_state_ty &state) {
    for (auto &channel : *state.channels) {
        if (channel.inactive()) continue;
        eval_channel(state, channel);
    }
}

void
init_chan_from_settings(modchannel_t &dest, const MODCHANNELSETTINGS &src) {
    dest.nGlobalVol = src.nVolume;
    dest.nVolume = src.nVolume;
    dest.nPan = src.nPan;
    dest.nPanSwing = dest.nVolSwing = 0;
    dest.nCutSwing = dest.nResSwing = 0;
    dest.nOldVolParam = 0;
    dest.nOldOffset = 0;
    dest.nOldHiOffset = 0;
    dest.nPortamentoDest = 0;
    if (dest.length == 0) {
        dest.flags = voicef_from_oldchn(src.dwFlags);
        dest.loop_start = 0;
        dest.loop_end = 0;
        dest.instrument = nullptr;
        dest.sample_data = nullptr;
        dest.sample = nullptr;
    }
}

void
eval_note(eval_state_ty &state) {
    eval_row(state);
    state.tick_width = calc_tick_width(state);
    const auto master_vol = state.sample_pre_amp;
}

const double IntToFloat = 1.0 / MIXING_CLIPMAX;
const double FloatToInt = MIXING_CLIPMAX;

void
truncate_copy(int16_t *dest, int32_t *src, size_t num_samples) {
    for (size_t i = 0; i < num_samples; ++i) {
        *dest = *src >> (16 - modplug::mixgraph::MIXING_ATTENUATION);
        ++dest;
        ++src;
    }
}

struct sample_cursor_ty {
    sampleoffset_t position;
    fixedpt_t fractional_sample_position;
    fixedpt_t position_delta;
    bitset<vflag_ty> flags;
};

int32_t inline
calc_max_samples(const fixedpt_t pos_dt) {
    return 16384 / ((pos_dt >> 16) + 1);
}

std::tuple<int32_t, int32_t, int32_t> inline
calc_safe_frames(const fixedpt_t pos_dt, const int32_t frame_count) {
    const auto maxsamples_ = std::max(2, calc_max_samples(pos_dt));
    const auto maxsamples = std::min(frame_count, maxsamples_);
    return std::make_tuple(
        maxsamples,
        (pos_dt >> 16) * (maxsamples - 1),
        (pos_dt & 0xffff) * (maxsamples - 1)
    );
}

void
advance_silently(modchannel_t &channel, const int32_t frame_count) {
    const auto delta = channel.position_delta * frame_count +
        static_cast<int32_t>(channel.fractional_sample_position);
    channel.fractional_sample_position = static_cast<uint32_t>(delta) & 0xffff;
    //XXXih: type errors ahoy!
    channel.sample_position += delta >> 16;
    channel.nROfs = 0;
    channel.nLOfs = 0;
}

uint32_t
generate_frames(
    const size_t out_frames, const size_t mix_channels,
    const eval_state_ty &state)
{
    using modplug::mixgraph::resample_and_mix;
    uint32_t channels_used = 0;
    for (size_t idx = 0; idx < mix_channels; ++idx) {
        const auto channel = &(*state.channels)[(*state.mix_channels)[idx]];
        if (!channel->active_sample_data) continue;
        ++channels_used;

        const auto parent_idx = channel->parent_channel
            ? channel->parent_channel - 1
            : (*state.mix_channels)[idx];
        const auto mix_vtx = parent_idx > modplug::mixgraph::MAX_PHYSICAL_CHANNELS
            ? throw new std::runtime_error("holy moly!")
            : state.mixgraph->channel_vertices[parent_idx];
        const size_t num_frames = 0;

        auto remaining_frames = static_cast<int>(out_frames);
        auto left_buf = mix_vtx->channels[0];
        auto right_buf = mix_vtx->channels[1];
        while (remaining_frames > 0) {
            const bool silently_advance =
                (channel->nRampLength == 0) &&
                (channel->left_volume == 0) &&
                (channel->right_volume == 0);
            if (silently_advance) {
                const auto delta =
                    (channel->position_delta * num_frames) +
                    (channel->fractional_sample_position);
                channel->fractional_sample_position = delta & 0xffff;
                channel->sample_position += delta >> 16;
            } else {
                resample_and_mix(left_buf, right_buf, channel, num_frames);
            }
            left_buf += num_frames;
            right_buf += num_frames;
            remaining_frames -= num_frames;
        }
    }
    return channels_used;
}

uint32_t
eval_pattern(int16_t * const out, const size_t out_frames, eval_state_ty &state) {
    auto uncomputed_frames = out_frames;
    auto outptr = out;
    int32_t temp_buffer[2048];
    uint32_t frames_needed = 0;
    while (uncomputed_frames > 0) {
        if (frames_needed == 0) {
            eval_note(state);
            frames_needed = state.tick_width.width;
        }
        auto computed_frames = frames_needed;
        if (computed_frames > modplug::mixgraph::MIX_BUFFER_SIZE) assert(0);
        if (computed_frames > uncomputed_frames) computed_frames = uncomputed_frames;
        if (computed_frames == 0) break;

        state.mixgraph->pre_process(computed_frames);
        //CreateStereoMix(computed_samples);
        state.mixgraph->process(temp_buffer, computed_frames, FloatToInt, IntToFloat);

        const auto num_samples = computed_frames * 2;
        truncate_copy(outptr, temp_buffer, num_samples);
        outptr += num_samples;

        assert(computed_frames <= uncomputed_frames);
        frames_needed -= computed_frames;
        uncomputed_frames -= computed_frames;
    }
    return out_frames - uncomputed_frames;
}

}
}
