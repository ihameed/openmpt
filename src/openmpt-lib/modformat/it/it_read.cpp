#include "stdafx.h"

#include "it.hpp"
#include "../../pervasives/binaryparse.hpp"
#include "../../pervasives/option.hpp"
#include "../../pervasives/pervasives.hpp"

using namespace modplug::modformat::mptexts;
using namespace modplug::pervasives;
using namespace modplug::pervasives::binaryparse;

namespace modplug {
namespace modformat {
namespace it {

template <typename PredTy, typename FunTy>
void
repeat_until_failure_(PredTy pred, FunTy fun) {
    while (true) {
        const auto pred_result = pred();
        if (pred_result.is_initialized()) fun(pred_result.get());
        else break;
    }
}

bitset<flags_ty>
song_flags(uint16_t val) {
    bitset<flags_ty> ret;
    if (val & 0x1)  ret = bitset_set(ret, flags_ty::Stereo);
    if (val & 0x2)  ret = bitset_set(ret, flags_ty::Vol0MixOptimizations);
    if (val & 0x4)  ret = bitset_set(ret, flags_ty::UseInstruments);
    if (val & 0x8)  ret = bitset_set(ret, flags_ty::UseLinearSlides);
    if (val & 0x10) ret = bitset_set(ret, flags_ty::UseOldEffects);
    if (val & 0x20) ret = bitset_set(ret, flags_ty::LinkPortaWithSlide);
    if (val & 0x40) ret = bitset_set(ret, flags_ty::UseMidiPitchController);
    if (val & 0x80) ret = bitset_set(ret, flags_ty::RequestEmbeddedMidiCfg);
    return ret;
}

bitset<special_flags_ty>
special_flags(uint16_t val) {
    bitset<special_flags_ty> ret;
    if (val & 0x1) ret = bitset_set(ret, special_flags_ty::SongMessageEmbedded);
    if (val & 0x2) ret = bitset_set(ret, special_flags_ty::EditHistoryEmbedded);
    if (val & 0x4) ret = bitset_set(ret, special_flags_ty::RowHighlightEmbedded);
    if (val & 0x8) ret = bitset_set(ret, special_flags_ty::MidiConfigurationEmbedded);
    return ret;
}

file_header_ty
read_header(context &ctx) {
    auto read_offsets = [&] (size_t len) {
        return replicate(len, [&] () { return read_uint32_le(ctx); });
    };
    file_header_ty ret;
    const auto tag = read_uint32_be(ctx);
    if (tag != 'IMPM') throw invalid_data_exception();
    read_bytestring_arr(ctx, ret.song_name);
    ret.row_highlight_minor        = read_uint8(ctx);
    ret.row_highlight_major        = read_uint8(ctx);
    ret.orders                     = read_uint16_le(ctx);
    ret.instruments                = read_uint16_le(ctx);
    ret.samples                    = read_uint16_le(ctx);
    ret.patterns                   = read_uint16_le(ctx);
    ret.version_created_with       = read_uint16_le(ctx);
    ret.minimum_compatible_version = read_uint16_le(ctx);
    ret.flags                      = song_flags(read_uint16_le(ctx));
    ret.special                    = special_flags(read_uint16_le(ctx));
    ret.global_volume              = read_uint8(ctx);
    ret.mix_volume                 = read_uint8(ctx);
    ret.initial_speed              = read_uint8(ctx);
    ret.initial_tempo              = read_uint8(ctx);
    ret.panning_separation         = read_uint8(ctx);
    ret.pitch_wheel_depth          = read_uint8(ctx);
    ret.offsets.message_length     = read_uint16_le(ctx);
    ret.offsets.message            = read_uint32_le(ctx);
    ret.reserved                   = read_uint32_le(ctx);
    read_bytestring_arr(ctx, ret.channel_pan);
    read_bytestring_arr(ctx, ret.channel_vol);
    ret.orderlist           = read_bytestring(ctx, ret.orders);
    ret.offsets.instruments = read_offsets(ret.instruments);
    ret.offsets.samples     = read_offsets(ret.samples);
    ret.offsets.patterns    = read_offsets(ret.patterns);
    return ret;
}

nna_ty
new_note_action(const uint8_t val) {
    switch (val) {
    case 1  : return nna_ty::Continue;
    case 2  : return nna_ty::NoteOff;
    case 3  : return nna_ty::NoteFade;
    default : return nna_ty::Cut;
    }
}

dct_ty
duplicate_check_type(const uint8_t val) {
    switch (val) {
    case 1  : return dct_ty::Note;
    case 2  : return dct_ty::Sample;
    case 3  : return dct_ty::Instrument;
    default : return dct_ty::Off;
    }
}

dca_ty
duplicate_check_action(const uint8_t val) {
    switch (val) {
    case 1  : return dca_ty::NoteOff;
    case 2  : return dca_ty::NoteFade;
    default : return dca_ty::Cut;
    }
}

bitset<env_flags_ty>
env_flags(const uint8_t val) {
    bitset<env_flags_ty> ret;
    if (val & 0x1) ret = bitset_set(ret, env_flags_ty::Enabled);
    if (val & 0x2) ret = bitset_set(ret, env_flags_ty::Loop);
    if (val & 0x4) ret = bitset_set(ret, env_flags_ty::Sustain);
    return ret;
}

bitset<pitch_env_flags_ty>
pitch_env_flags(const uint8_t val) {
    bitset<pitch_env_flags_ty> ret;
    if (val & 0x1)  ret = bitset_set(ret, pitch_env_flags_ty::Enabled);
    if (val & 0x2)  ret = bitset_set(ret, pitch_env_flags_ty::Loop);
    if (val & 0x4)  ret = bitset_set(ret, pitch_env_flags_ty::Sustain);
    if (val & 0x80) ret = bitset_set(ret, pitch_env_flags_ty::Filter);
    return ret;
}

template <typename FunTy>
auto
read_envelope(context &ctx, uint32_t offset, FunTy parse)
    -> envelope_ty<decltype(parse(std::declval<uint8_t>()))>
{
    envelope_ty<decltype(parse(std::declval<uint8_t>()))> ret;
    ret.flags              = parse(read_uint8(ctx));
    ret.num_points         = read_uint8(ctx);
    ret.loop_begin         = read_uint8(ctx);
    ret.loop_end           = read_uint8(ctx);
    ret.sustain_loop_begin = read_uint8(ctx);
    ret.sustain_loop_end   = read_uint8(ctx);
    for (auto &item : ret.points) {
        const auto y_ = read_uint8(ctx) + offset;
        const auto x = read_uint16_le(ctx);
        const auto y = clamp<uint8_t>(y_, 0, 64);
        item = std::make_tuple(x, y);
    }
    read_skip(ctx, 1);
    return ret;
}

boost::optional<uint8_t>
read_modular_instrument_suffix(context &ctx) {
    auto found_header = read_txn(ctx, [&] (context &ctx) {
        const auto tag = read_uint32_be(ctx);
        if (tag != 'MSNI') read_rollback();
        return unit;
    });
    if (found_header) {
        boost::optional<uint8_t> ret;
        const auto size = read_uint32_le(ctx);
        const auto slice = read_bytestring_slice(ctx, size);
        auto nested = mkcontext(std::shared_ptr<const uint8_t>(slice, noop), size);
        auto reader = [&nested, &ret] () {
            return read_txn(nested, [&ret] (context &ctx) {
                const auto tag = read_uint32_le(ctx);
                if (tag == 'PLUG') ret = read_uint8(ctx);
                return unit;
            });
        };
        repeat_until_failure_(reader, [] (unit_ty) { });
        return ret;
    }
    return boost::none;
}

instrument_ty
read_instrument(context &ctx) {
    const auto tag = read_uint32_be(ctx);
    if (tag != 'IMPI') throw invalid_data_exception();
    instrument_ty ret;
    read_bytestring_arr(ctx, ret.filename);
    read_skip(ctx, 1);
    ret.new_note_action          = new_note_action(read_uint8(ctx));
    ret.duplicate_check_type     = duplicate_check_type(read_uint8(ctx));
    ret.duplicate_check_action   = duplicate_check_action(read_uint8(ctx));
    ret.fadeout                  = read_uint16_le(ctx);
    ret.pitch_pan_separation     = read_uint8(ctx);
    ret.pitch_pan_center         = read_uint8(ctx);
    ret.global_volume            = read_uint8(ctx);
    ret.default_pan              = read_uint8(ctx);
    ret.random_volume_variation  = read_uint8(ctx);
    ret.random_panning_variation = read_uint8(ctx);
    ret.version_created_with     = read_uint16_le(ctx);
    ret.num_associated_samples   = read_uint8(ctx);
    read_skip(ctx, 1);
    read_bytestring_arr(ctx, ret.name);
    ret.default_filter_cutoff    = read_uint8(ctx);
    ret.default_filter_resonance = read_uint8(ctx);
    ret.midi_channel             = read_uint8(ctx);
    ret.midi_program             = read_uint8(ctx);
    ret.midi_bank                = read_uint16_le(ctx);
    for (auto &item : ret.note_sample_map) {
        const auto note   = read_uint8(ctx);
        const auto sample = read_uint8(ctx);
        item = std::make_tuple(note, sample);
    }
    ret.volume_envelope = read_envelope(ctx, 0, env_flags);
    ret.pan_envelope    = read_envelope(ctx, 32, env_flags);
    ret.pitch_envelope  = read_envelope(ctx, 32, pitch_env_flags);
    read_skip(ctx, 4);
    return ret;
}

bitset<sample_flags_ty>
sample_flags(const uint8_t val) {
    bitset<sample_flags_ty> ret;
    if (val & 0x1)  ret = bitset_set(ret, sample_flags_ty::HasSampleData);
    if (val & 0x2)  ret = bitset_set(ret, sample_flags_ty::SixteenBit);
    if (val & 0x4)  ret = bitset_set(ret, sample_flags_ty::Stereo);
    if (val & 0x8)  ret = bitset_set(ret, sample_flags_ty::Compressed);
    if (val & 0x10) ret = bitset_set(ret, sample_flags_ty::LoopEnabled);
    if (val & 0x20) ret = bitset_set(ret, sample_flags_ty::SusLoopEnabled);
    if (val & 0x40) ret = bitset_set(ret, sample_flags_ty::PingPongLoop);
    if (val & 0x80) ret = bitset_set(ret, sample_flags_ty::PingPongSusLoop);
    return ret;
}

bitset<convert_flags_ty>
sample_convert_flags(const uint8_t val) {
    bitset<convert_flags_ty> ret;
    if (val & 0x1)  ret = bitset_set(ret, convert_flags_ty::SignedSamples);
    if (val & 0x2)  ret = bitset_set(ret, convert_flags_ty::BigEndian);
    if (val & 0x4)  ret = bitset_set(ret, convert_flags_ty::DeltaEncoded);
    if (val & 0x8)  ret = bitset_set(ret, convert_flags_ty::ByteDeltaEncoded);
    if (val & 0x10) ret = bitset_set(ret, convert_flags_ty::TwelveBit);
    if (val & 0x20) ret = bitset_set(ret, convert_flags_ty::LeftRightStereoPrompt);
    return ret;
}

vibrato_wave_ty
vibrato_waveform(const uint8_t val) {
    switch (val) {
    case 1  : return vibrato_wave_ty::RampDown;
    case 2  : return vibrato_wave_ty::Square;
    case 3  : return vibrato_wave_ty::Random;
    default : return vibrato_wave_ty::Sine;
    }
}

std::tuple<sample_ty, uint32_t>
read_sample(context &ctx) {
    sample_ty ret;
    const auto tag = read_uint32_be(ctx);
    if (tag != 'IMPS') throw invalid_data_exception();
    read_bytestring_arr(ctx, ret.filename);
    read_skip(ctx, 1);
    ret.global_volume  = read_uint8(ctx);
    ret.flags          = sample_flags(read_uint8(ctx));
    ret.default_volume = read_uint8(ctx);
    read_bytestring_arr(ctx, ret.name);
    ret.convert_flags          = sample_convert_flags(read_uint8(ctx));
    ret.default_pan            = read_uint8(ctx);
    ret.num_frames             = read_uint32_le(ctx);
    ret.loop_begin_frame       = read_uint32_le(ctx);
    ret.loop_end_frame         = read_uint32_le(ctx);
    ret.c5_freq                = read_uint32_le(ctx);
    ret.sus_loop_begin_frame   = read_uint32_le(ctx);
    ret.sus_loop_end_frame     = read_uint32_le(ctx);
    const uint32_t data_offset = read_uint32_le(ctx);
    ret.vibrato_speed          = read_uint8(ctx);
    ret.vibrato_depth          = read_uint8(ctx);
    ret.vibrato_rate           = read_uint8(ctx);
    ret.vibrato_waveform       = vibrato_waveform(read_uint8(ctx));
    ret.data                   = nullptr;
    return std::make_tuple(ret, data_offset);
}

void
inject_sample_data(context &ctx, sample_ty &sample, const uint32_t offset) {
    // XXXih: ompt assumes that the offset is well-defined even if
    // HasSampleData does not hold
    read_seek(ctx, offset);
    if (bitset_is_set(sample.flags, sample_flags_ty::HasSampleData)) {
        const uint32_t adjust =
            (bitset_is_set(sample.flags, sample_flags_ty::SixteenBit) ? 2 : 1) *
            (bitset_is_set(sample.flags, sample_flags_ty::Stereo) ? 2 : 1);
        const uint32_t byte_length = sample.num_frames * adjust;
        if (byte_length < sample.num_frames) throw invalid_data_exception();
        sample.data = read_bytestring_slice(ctx, byte_length);
    } else {
        sample.data = nullptr;
    }
}

bool
probably_old_unmo3(const file_header_ty &header) {
    return header.row_highlight_major == 0
        && header.row_highlight_minor == 0
        && header.minimum_compatible_version == 0x214
        && header.version_created_with == 0x214
        && header.reserved == 0
        && !bitset_is_set(header.special, special_flags_ty::EditHistoryEmbedded)
        && !bitset_is_set(header.special, special_flags_ty::RowHighlightEmbedded);
}

boost::optional<uint32_t>
minimum_offset(const file_header_ty &header) {
    const auto ins_min = minimum(header.offsets.instruments);
    const auto smp_min = minimum(header.offsets.samples);
    const auto pat_min = minimum(header.offsets.patterns);
    const auto msg_min = bitset_is_set(header.special, special_flags_ty::SongMessageEmbedded)
        ? some(header.offsets.message)
        : boost::none;
    return min_(min_(min_(pat_min, msg_min), smp_min), ins_min);
}

boost::optional<std::vector<edit_history_ty>>
read_history(context &ctx, const file_header_ty &header) {
    return read_txn(ctx, [&] (context &ctx) {
        const auto min_offs = minimum_offset(header);
        const auto num_entries = read_uint16_le(ctx);
        if (min_offs && static_cast<uint32_t>(num_entries * 8) > min_offs.get()) {
            read_rollback();
        }
        return replicate(num_entries, [&] () {
            edit_history_ty ret;
            const auto dosfat_date   = read_uint16_le(ctx);
            const auto dosfat_time   = read_uint16_le(ctx);
            const auto ticks_elapsed = read_uint32_le(ctx);
            ret.year   = ((dosfat_date >> 9) & 0x7f) + 1980;
            ret.month  = clamp((dosfat_date >> 5) & 0x0f, 1, 12) - 1;
            ret.day    = clamp(dosfat_date & 0x1f, 1, 31);
            ret.hour   = clamp((dosfat_time >> 11) & 0x1f, 0, 23);
            ret.minute = clamp((dosfat_time >> 5) & 0x3f, 0, 59);
            ret.second = clamp((dosfat_time & 0x1f) * 2, 0, 59);
            ret.open_time_seconds = ticks_elapsed * 18.2;
            return ret;
        });
    });
}

void
skip_zero_length_history(context &ctx) {
    read_txn(ctx, [&] (context &ctx) {
        const auto num_entries = read_uint16_le(ctx);
        if (num_entries != 0) read_rollback();
        return unit;
    });
}

boost::optional<std::vector<edit_history_ty>>
try_read_history(context &ctx, const file_header_ty &header) {
    if (bitset_is_set(header.special, special_flags_ty::EditHistoryEmbedded)) {
        return read_history(ctx, header);
    } else if (probably_old_unmo3(header)) {
        skip_zero_length_history(ctx);
    }
    return boost::none;
}

std::vector<uint8_t>
read_channel_plugin_assignments(context &ctx) {
    return replicate(MaxChannelSize, [&] () {
        return static_cast<uint8_t>(read_uint32_le(ctx));
    });
}

void
inject_plugin_info(context &ctx, plugin_ty &ref) {
    ref.info.plugin_id_1    = read_uint32_le(ctx);
    ref.info.plugin_id_2    = read_uint32_le(ctx);
    ref.info.input_routing  = read_uint32_le(ctx);
    ref.info.output_routing = read_uint32_le(ctx);
    for (auto &res : ref.info.reserved) {
        res = read_uint32_le(ctx);
    }
    read_bytestring_arr(ctx, ref.info.name);
    read_bytestring_arr(ctx, ref.info.dll_file_name);
    const auto vst_chunk_size = read_uint32_le(ctx);
    read_txn(ctx, [&] (context &ctx) {
        const auto rawbuf = read_bytestring_slice(ctx, vst_chunk_size);
        const auto plugin_data = std::shared_ptr<uint8_t>(
            new uint8_t[vst_chunk_size],
            [] (uint8_t *ptr) { delete [] ptr; });
        memcpy(plugin_data.get(), rawbuf, vst_chunk_size);
        ref.data_size = vst_chunk_size;
        ref.data      = plugin_data;
        return unit;
    });
    const auto modular_data_size = read_uint32_le(ctx);
    auto pred = [&] () { return read_txn(ctx, [&] (context &ctx) {
        return read_uint32_be(ctx);
    });};
    auto dispatch = [&] (const uint32_t tag) {
        if (tag == 'DWRT') ref.dry_ratio = read_float32_le(ctx);
        else if (tag == 'PROG') ref.default_program = read_uint32_le(ctx);
        else read_skip(ctx, 1);
    };
    repeat_until_failure_(pred, dispatch);
};

std::shared_ptr<mix_info_ty>
read_plugin_data(context &ctx) {
    auto ret = std::make_shared<mix_info_ty>();
    auto pred = [&] () { return read_txn(ctx, [&] (context &ctx) {
        const auto tag = read_uint32_be(ctx);
        const auto size = read_uint32_le(ctx);
        check_overflow(ctx, size);
        if (tag == 'XTPM') read_rollback();
        return std::make_tuple(tag, size);
    });};
    auto dispatch = [&] (const std::tuple<const uint32_t, uint32_t> &tagsize) {
        const auto tag = std::get<0>(tagsize);
        const auto size = std::get<1>(tagsize);
        if (tag == 'CHFX') {
            ret->channel_mix_plugins = read_channel_plugin_assignments(ctx);
        } else if (tag >= 'FX00' && tag <= 'FX99') {
            const uint8_t idx = ((((tag >> 8) & 0xff) - '0') * 10) + ((tag & 0xff) - '0');
            auto &ref = ret->plugin_data[idx];
            inject_plugin_info(ctx, ref);
        }
    };
    repeat_until_failure_(pred, dispatch);
    return ret;
}

boost::optional<std::vector<channel_name_ty>>
read_channel_names(context &ctx) {
    return read_magic(ctx, "CNAM", [&] () {
        const auto len   = read_uint32_le(ctx);
        const auto names = len / ChannelNameSize;
        const auto rem   = len % ChannelNameSize;
        auto ret = replicate(names, [&] () {
            channel_name_ty buf;
            read_bytestring_arr(ctx, buf);
            return buf;
        });
        read_skip(ctx, rem);
        return ret;
    });
}

boost::optional<const uint8_t *>
read_raw_pattern_names(context &ctx) {
    return read_magic(ctx, "PNAM", [&] () {
        const auto len = read_uint32_le(ctx);
        return read_bytestring_slice(ctx, len);
    });
}

boost::optional<midi_cfg_ty>
read_midi_config(context &ctx, const file_header_ty &header) {
    const auto enabled = 
        bitset_is_set(header.flags, flags_ty::RequestEmbeddedMidiCfg) ||
        bitset_is_set(header.special, special_flags_ty::MidiConfigurationEmbedded);
    if (!enabled) return boost::none;
    return read_txn(ctx, [] (context &ctx) {
        midi_cfg_ty ret;
        auto read_macro = [&] (midi_cfg_ty::macro_arr_ty &ref) {
            read_bytestring_arr(ctx, ref);
        };
        for_each(ret.global, read_macro);
        for_each(ret.parameterized, read_macro);
        for_each(ret.fixed, read_macro);
        return ret;
    });
}

boost::optional<std::string>
read_comments(context &ctx, const file_header_ty &header) {
    const auto enabled = bitset_is_set(header.special, special_flags_ty::SongMessageEmbedded);
    if (!enabled || header.offsets.message_length == 0) return boost::none;
    return read_txn(ctx, [&] (context &ctx) {
        read_seek(ctx, header.offsets.message);
        return read_bytestring_str(ctx, header.offsets.message_length);
    });
}

template<typename Ty>
void
move_some(Ty &ref, boost::optional<Ty> &&x) {
    if (x) ref = std::move(x.get());
}

boost::optional<unit_ty>
inject_extended_instr_block(context &ctx, mpt_extensions_ty &ret, const size_t instrs) {
    return read_txn(ctx, [&] (context &ctx) {
        const auto raw = read_uint32_le(ctx);
        if (raw == 'MPTS') read_rollback();
        const auto tag = instr_tag_of_bytes(raw);
        const auto len = read_uint16_le(ctx);
        if (tag) {
            for (auto &item : ret.instruments) {
                inject_extended_instr_data(ctx, tag.get(), len, item);
            }
        } else {
            read_skip(ctx, instrs * len);
        }
        return unit;
    });
}

boost::optional<unit_ty>
inject_extended_song_block(context &ctx, mpt_song_ty &ret) {
    return read_txn(ctx, [&] (context &ctx) {
        const auto tag = song_tag_of_bytes(read_uint32_le(ctx));
        const auto len = read_uint16_le(ctx);
        if (tag) inject_extended_song_data(ctx, tag.get(), len, ret);
        else read_skip(ctx, len);
        return unit;
    });
}

void
inject_extended_info(
    context &ctx,
    const boost::optional<uint32_t> offset,
    const file_header_ty &header,
    mpt_extensions_ty &ret)
{
    if (offset) read_seek(ctx, offset.get());
    read_txn(ctx, [&] (context &ctx) {
        ret.instruments.resize(header.instruments);
        const auto blocktag = read_uint32_le(ctx);
        if (blocktag != 'MPTX') read_rollback();
        auto read = [&] () { return inject_extended_instr_block(ctx, ret, header.instruments); };
        repeat_until_failure_(read, [] (unit_ty) {});
        return unit;
    });
    read_txn(ctx, [&] (context &ctx) {
        const auto blocktag = read_uint32_le(ctx);
        if (blocktag != 'MPTS') read_rollback();
        auto read = [&] () { return inject_extended_song_block(ctx, ret.song); };
        repeat_until_failure_(read, [] (const unit_ty) {});
        return unit;
    });
}

std::tuple<vol_ty, uint8_t>
parse_vol(const uint8_t val) {
    auto tsp = [&] (const vol_ty ty, const uint8_t minval) {
        return std::make_tuple(ty, val - minval);
    };
    if      (val <= 64)  return tsp(vol_ty::VolSet, 0);
    else if (val <= 74)  return tsp(vol_ty::VolFineUp, 65);
    else if (val <= 84)  return tsp(vol_ty::VolFineDown, 75);
    else if (val <= 94)  return tsp(vol_ty::VolSlideUp, 85);
    else if (val <= 104) return tsp(vol_ty::VolSlideDown, 95);
    else if (val <= 114) return tsp(vol_ty::PortamentoDown, 105);
    else if (val <= 124) return tsp(vol_ty::PortamentoUp, 115);
    else if (val <= 192) return tsp(vol_ty::PanSet, 128);
    else if (val <= 202) return tsp(vol_ty::PortamentoUp, 193);
    else if (val <= 212) return tsp(vol_ty::PortamentoUp, 203);
    else                 return EmptyVol;
}

std::tuple<cmd_ty, uint8_t>
parse_cmd(const uint8_t rawcmd, const uint8_t rawparam) {
    auto full = [=] (const cmd_ty type) {
        return std::make_tuple(type, rawparam);
    };
    auto nibble = [=] (const cmd_ty type) {
        return std::make_tuple(type, rawparam & 0xf);
    };
    auto slide = [=] (
        const cmd_ty up, const cmd_ty down,
        const cmd_ty fineup, const cmd_ty finedown,
        const cmd_ty cont)
    {
        const uint8_t hi = rawparam >> 4;
        const uint8_t lo = rawparam & 0xf;
        if      (hi == 0xf && lo > 0) return std::make_tuple(finedown, lo);
        else if (lo == 0xf && hi > 0) return std::make_tuple(fineup, hi);
        else if (hi == 0x0 && lo > 0) return std::make_tuple(down, lo);
        else if (lo == 0x0 && hi > 0) return std::make_tuple(up, hi);
        else                          return std::make_tuple(cont, rawparam);
    };
    auto uni_slide = [=] (
        const cmd_ty normal,
        const cmd_ty fine,
        const cmd_ty extrafine)
    {
        const uint8_t hi = rawparam >> 4;
        const uint8_t lo = rawparam & 0xf;
        if      (hi == 0xf) return std::make_tuple(fine, lo);
        else if (hi == 0xe) return std::make_tuple(extrafine, lo);
        else                return std::make_tuple(normal, rawparam);
    };
    switch (rawcmd) {
    case 0           : return full(cmd_ty::Nothing);
    case 'A' - 0x40  : return full(cmd_ty::SetSpeed);
    case 'B' - 0x40  : return full(cmd_ty::JumpToOrder);
    case 'C' - 0x40  : return full(cmd_ty::BreakToRow);
    case 'D' - 0x40  : return slide(
        cmd_ty::VolSlideUp,
        cmd_ty::VolSlideDown,
        cmd_ty::FineVolSlideUp,
        cmd_ty::FineVolSlideDown,
        cmd_ty::VolSlideContinue);
    case 'E' - 0x40  : return uni_slide(
        cmd_ty::PortamentoDown,
        cmd_ty::FinePortamentoDown,
        cmd_ty::ExtraFinePortamentoDown);
    case 'F' - 0x40  : return uni_slide(
        cmd_ty::PortamentoUp,
        cmd_ty::FinePortamentoUp,
        cmd_ty::ExtraFinePortamentoUp);
    case 'G' - 0x40  : return full(cmd_ty::Portamento);
    case 'H' - 0x40  : return full(cmd_ty::Vibrato);
    case 'J' - 0x40  : return full(cmd_ty::Arpeggio);
    case 'K' - 0x40  : return slide(
        cmd_ty::VolSlideVibratoUp,
        cmd_ty::VolSlideVibratoDown,
        cmd_ty::FineVolSlideVibratoUp,
        cmd_ty::FineVolSlideVibratoDown,
        cmd_ty::VolSlideVibratoContinue);
    case 'L' - 0x40  : return slide(
        cmd_ty::VolSlidePortaUp,
        cmd_ty::VolSlidePortaDown,
        cmd_ty::FineVolSlidePortaUp,
        cmd_ty::FineVolSlidePortaDown,
        cmd_ty::VolSlidePortaContinue);
    case 'M' - 0x40  : return full(cmd_ty::SetChannelVol);
    case 'N' - 0x40  : return slide(
        cmd_ty::ChannelVolSlideUp,
        cmd_ty::ChannelVolSlideDown,
        cmd_ty::ChannelFineVolSlideUp,
        cmd_ty::ChannelFineVolSlideDown,
        cmd_ty::ChannelVolSlideContinue);
    case 'O' - 0x40  : return full(cmd_ty::SetSampleOffset);
    case 'P' - 0x40  : return slide(
        cmd_ty::PanSlideLeft,
        cmd_ty::PanSlideRight,
        cmd_ty::FinePanSlideLeft,
        cmd_ty::FinePanSlideRight,
        cmd_ty::PanSlideContinue);
    case 'Q' - 0x40  : return full(cmd_ty::Retrigger);
    case 'R' - 0x40  : return full(cmd_ty::Tremolo);
    case 'S' - 0x40  : switch (rawparam >> 4) {
        case 0x1 : return nibble(cmd_ty::GlissandoControl);
        case 0x3 : return nibble(cmd_ty::VibratoWaveform);
        case 0x4 : return nibble(cmd_ty::TremoloWaveform);
        case 0x5 : return nibble(cmd_ty::PanbrelloWaveform);
        case 0x6 : return nibble(cmd_ty::FinePatternDelay);
        case 0x7 : return nibble(cmd_ty::InstrControl);
        case 0x8 : return nibble(cmd_ty::SetPanShort);
        case 0x9 : return nibble(cmd_ty::SoundControl);
        case 0xa : return nibble(cmd_ty::SetHighSampleOffset);
        case 0xb : return nibble(cmd_ty::PatternLoop);
        case 0xc : return nibble(cmd_ty::NoteCut);
        case 0xd : return nibble(cmd_ty::NoteDelay);
        case 0xe : return nibble(cmd_ty::PatternDelay);
        case 0xf : return nibble(cmd_ty::SetActiveMidiMacro);
        };
    case 'T' - 0x40  : return full(cmd_ty::SetTempo);
    case 'U' - 0x40  : return full(cmd_ty::FineVibrato);
    case 'V' - 0x40  : return full(cmd_ty::SetGlobalVol);
    case 'W' - 0x40  : return slide(
        cmd_ty::GlobalVolSlideUp,
        cmd_ty::GlobalVolSlideDown,
        cmd_ty::GlobalFineVolSlideUp,
        cmd_ty::GlobalFineVolSlideDown,
        cmd_ty::GlobalVolSlideContinue);
    case 'X' - 0x40  : return full(cmd_ty::SetPan);
    case 'Y' - 0x40  : return full(cmd_ty::Panbrello);
    case 'Z' - 0x40  : return full(cmd_ty::MidiMacro);
    case '\\' - 0x40 : return full(cmd_ty::SmoothMidiMacro);
    case ']' - 0x40  : return full(cmd_ty::DelayCut);
    case '[' - 0x40  : return full(cmd_ty::ExtendedParameter);
    default          : return EmptyEffect;
    }
}

void
unpack_pattern_row(
    context &ctx,
    uint8_t (&channel_masks)[MaxChannelSize],
    pattern_entry_ty (&entry_acc)[MaxChannelSize],
    pattern_entry_ty * const output
    )
{
    using namespace std::placeholders;
    const uint8_t ChanMask = 0x7f;
    const uint8_t ReadMask = 0x80;
    while (true) {
        const auto blockinfo_opt = read_txn(ctx, std::bind(read_uint8, _1));
        if (!blockinfo_opt) return;
        const auto blockinfo = blockinfo_opt.get();
        if ((blockinfo & ChanMask) == 0) return;
        const auto channel = clamp<uint8_t>((blockinfo & ChanMask) - 1, 0, MaxChannelSize - 1);
        auto cur = &output[channel];
        if (blockinfo & ReadMask) channel_masks[channel] = read_uint8(ctx);
        auto mask  = channel_masks[channel];
        auto &last = entry_acc[channel];
        if (mask & ReadNote) {
            last.note = read_uint8(ctx);
            mask |= UseLastNote;
        }
        if (mask & ReadInstr) {
            last.instr = read_uint8(ctx);
            mask |= UseLastInstr;
        }
        if (mask & ReadVol) {
            std::tie(last.volcmd, last.volcmd_parameter) = 
                parse_vol(read_uint8(ctx));
            mask |= UseLastVol;
        }
        if (mask & ReadCmd) {
            const auto rawcmd   = read_uint8(ctx);
            const auto rawparam = read_uint8(ctx);
            std::tie(last.effect_type, last.effect_parameter) =
                parse_cmd(rawcmd, rawparam);
            mask |= UseLastCmd;
        }
        if (mask & UseLastNote) cur->note = last.note;
        if (mask & UseLastInstr) cur->instr = last.instr;
        if (mask & UseLastVol) {
            cur->volcmd = last.volcmd;
            cur->volcmd_parameter = last.volcmd_parameter;
        }
        if (mask & UseLastCmd) {
            cur->effect_type = last.effect_type;
            cur->effect_parameter= last.effect_parameter;
        }
    }
}

void
inject_pattern_data(context &ctx, pattern_ty &pattern) {
    uint8_t channel_masks[MaxChannelSize] = { 0 };
    pattern_entry_ty entry_acc[MaxChannelSize];
    const auto length   = read_uint16_le(ctx);
    const auto num_rows = read_uint16_le(ctx);
    read_skip(ctx, 4);
    const auto packed = read_bytestring_slice(ctx, length);
    auto nested = mkcontext(std::shared_ptr<const uint8_t>(packed, noop), length);
    pattern.resize(boost::extents[num_rows][MaxChannelSize]);
    auto ptr = pattern.data();
    for (uint16_t i = 0; i < num_rows; ++i) {
        unpack_pattern_row(nested, channel_masks, entry_acc, ptr + (i * MaxChannelSize));
    }
}

std::shared_ptr<song>
read(context &ctx) {
    auto ptr = std::make_shared<song>();
    auto &ret = *ptr;
    ret.header = read_header(ctx);
    move_some(ret.edit_history, try_read_history(ctx, ret.header));
    ret.midi_cfg = read_midi_config(ctx, ret.header);
    read_magic(ctx, "MODU", [&] () { return unit; });
    const auto patbuf = read_raw_pattern_names(ctx);
    move_some(ret.channel_names, read_channel_names(ctx));
    ret.mix_info = read_plugin_data(ctx);

    ret.instruments.reserve(ret.header.instruments);
    ret.samples.reserve(ret.header.samples);
    ret.patterns.reserve(ret.header.patterns);
    boost::optional<uint32_t> max_offset;
    auto recalc_max = [&] () { max_offset = max_(max_offset, some(read_offset(ctx))); };

    for (const auto &offset : ret.header.offsets.instruments) {
        read_seek(ctx, offset);
        ret.instruments.emplace_back(read_instrument(ctx));
        ret.mpt_extensions.instr_plugnum.emplace_back(read_modular_instrument_suffix(ctx));
        recalc_max();
    }

    for (const auto &offset : ret.header.offsets.samples) {
        read_seek(ctx, offset);
        sample_ty sample;
        uint32_t data_offset;
        std::tie(sample, data_offset) = read_sample(ctx);
        recalc_max();
        inject_sample_data(ctx, sample, data_offset);
        recalc_max();
        ret.samples.emplace_back(sample);
    }

    for (const auto &offset : ret.header.offsets.patterns) {
        read_seek(ctx, offset);
        ret.patterns.emplace_back();
        inject_pattern_data(ctx, ret.patterns.back());
    }

    inject_extended_info(ctx, max_offset, ret.header, ret.mpt_extensions);

    move_some(ret.comments, read_comments(ctx, ret.header));
    return ptr;
}

}
}
}
