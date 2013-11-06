#include "stdafx.h"

#include "xm.h"
#include "../../pervasives/pervasives.h"
#include "../../serializers/binaryparse.h"

using namespace modplug::pervasives;

namespace modplug {
namespace modformat {
namespace xm {

using namespace modplug::serializers::binaryparse;
using namespace modplug::serializers;

file_header_ty
read_file_header(context &ctx) {
    file_header_ty header;
    auto buf = read_bytestring_str(ctx, 17);
    if (buf != "Extended Module: ") throw invalid_data_exception();
    read_bytestring_arr(ctx, header.song_name);
    read_skip(ctx, 1);
    read_bytestring_arr(ctx, header.tracker_name);
    header.version     = read_uint16_le(ctx);
    header.size        = read_uint32_le(ctx);
    header.orders      = read_uint16_le(ctx);
    header.restartpos  = read_uint16_le(ctx);
    header.channels    = read_uint16_le(ctx);
    header.patterns    = read_uint16_le(ctx);
    header.instruments = read_uint16_le(ctx);
    header.flags       = read_uint16_le(ctx);
    header.speed       = read_uint16_le(ctx);
    header.tempo       = read_uint16_le(ctx);
    return header;
}

template <typename FunTy>
pattern_header_ty
read_pattern_header(context &ctx, FunTy read_row_len) {
    auto ret = read_lookahead(ctx, [&] (context &ctx) {
        pattern_header_ty ret;
        ret.size         = read_uint32_le(ctx);
        ret.packing_type = read_uint8(ctx);
        ret.num_rows     = read_row_len(ctx);
        ret.data_size    = read_uint16_le(ctx);
        return ret;
    });
    read_skip(ctx, ret.size);
    return ret;
}

std::tuple<vol_type_ty, uint8_t>
parse_volcmd(uint8_t raw) {
    auto low_nibble = [=] (vol_type_ty type) {
        return std::make_tuple(type, raw & 0xf);
    };
    if (raw < 0x0f) {
        return EmptyVol;
    } else if (raw <= 0x50) {
        return std::make_tuple(vol_type_ty::VolSet, raw - 0x10);
    } else if (raw <= 0x60) {
        return EmptyVol;
    } else if (raw <= 0x6f) {
        return low_nibble(vol_type_ty::VolSlideDown);
    } else if (raw <= 0x7f) {
        return low_nibble(vol_type_ty::VolSlideUp);
    } else if (raw <= 0x8f) {
        return low_nibble(vol_type_ty::VolFineDown);
    } else if (raw <= 0x9f) {
        return low_nibble(vol_type_ty::VolFineUp);
    } else if (raw <= 0xaf) {
        return low_nibble(vol_type_ty::VibSpeed);
    } else if (raw <= 0xbf) {
        return low_nibble(vol_type_ty::VibDepth);
    } else if (raw <= 0xcf) {
        return low_nibble(vol_type_ty::PanSet);
    } else if (raw <= 0xdf) {
        return low_nibble(vol_type_ty::PanSlideLeft);
    } else if (raw <= 0xef) {
        return low_nibble(vol_type_ty::PanSlideRight);
    } else {
        return low_nibble(vol_type_ty::Portamento);
    }
}

std::tuple<fx_type_ty, uint8_t>
parse_fxcmd(uint8_t rawcmd, uint8_t rawparam) {
    auto low_nibble = [=] (fx_type_ty type) {
        return std::make_tuple(type, rawparam & 0xf);
    };
    auto full_param = [=] (fx_type_ty type) {
        return std::make_tuple(type, rawparam);
    };
    switch (rawcmd) {
    case 0x0: return full_param(rawparam ? fx_type_ty::Arpeggio : fx_type_ty::Nothing);
    case 0x1: return full_param(fx_type_ty::PortaUp);
    case 0x2: return full_param(fx_type_ty::PortaDown);
    case 0x3: return full_param(fx_type_ty::Portamento);
    case 0x4: return full_param(fx_type_ty::Vibrato);
    case 0x5: return full_param(fx_type_ty::PortamentoVolSlide);
    case 0x6: return full_param(fx_type_ty::VibratoVolSlide);
    case 0x7: return full_param(fx_type_ty::Tremolo);
    case 0x8: return full_param(fx_type_ty::SetPan);
    case 0x9: return full_param(fx_type_ty::SampleOffset);
    case 0xa: return full_param(fx_type_ty::VolSlide);
    case 0xb: return full_param(fx_type_ty::PosJump);
    case 0xc: return full_param(fx_type_ty::SetVol);
    case 0xd: return full_param(fx_type_ty::PatternBreak);
    case 0xe: switch ((rawparam & 0xf0) >> 4) {
        case 0x1: return low_nibble(fx_type_ty::FinePortaUp);
        case 0x2: return low_nibble(fx_type_ty::FinePortaDown);
        case 0x3: return low_nibble(fx_type_ty::GlissControl);
        case 0x4: return low_nibble(fx_type_ty::VibratoControl);
        case 0x5: return low_nibble(fx_type_ty::Finetune);
        case 0x6: return low_nibble(fx_type_ty::LoopControl);
        case 0x7: return low_nibble(fx_type_ty::TremoloControl);
        case 0x9: return low_nibble(fx_type_ty::RetriggerNote);
        case 0xa: return low_nibble(fx_type_ty::FineVolSlideUp);
        case 0xb: return low_nibble(fx_type_ty::FineVolSlideDown);
        case 0xc: return low_nibble(fx_type_ty::NoteCut);
        case 0xd: return low_nibble(fx_type_ty::NoteDelay);
        case 0xe: return low_nibble(fx_type_ty::PatternDelay);
        };
    case 0xf: return full_param(fx_type_ty::SetTicksTempo);
    case 'G' - 55: return full_param(fx_type_ty::SetGlobalVolume);
    case 'H' - 55: return full_param(fx_type_ty::GlobalVolumeSlide);
    case 'L' - 55: return full_param(fx_type_ty::SetEnvelopePosition);
    case 'P' - 55: return full_param(fx_type_ty::PanningSlide);
    case 'R' - 55: return full_param(fx_type_ty::MultiRetrigNote);
    case 'X' - 55: switch ((rawparam & 0xf0) >> 4) {
        case 0x1: return low_nibble(fx_type_ty::ExtraFinePortaUp);
        case 0x2: return low_nibble(fx_type_ty::ExtraFinePortaDown);
        };
    default: return EmptyFx;
    };
}

pattern_entry_ty
read_pattern_entry(context &ctx) {
    pattern_entry_ty elem =
        { 0, 0, vol_type_ty::Nothing, 0, fx_type_ty::Nothing, 0 };
    auto flag = read_uint8(ctx);
    if (flag & IsFlag) {
        if (flag & NotePresent) elem.note = read_uint8(ctx);
    } else {
        elem.note = flag;
        flag = AllPresent;
    }
    if (flag & InstrumentPresent) elem.instrument = read_uint8(ctx);
    if (flag & VolCmdPresent) 
        std::tie(elem.volcmd, elem.volcmd_parameter) =
            parse_volcmd(read_uint8(ctx));
    uint8_t fxtype = 0;
    uint8_t fxparam = 0;
    if (flag & EffectTypePresent) fxtype = read_uint8(ctx);
    if (flag & EffectParamPresent) fxparam = read_uint8(ctx);
    std::tie(elem.effect_type, elem.effect_parameter) =
        parse_fxcmd(fxtype, fxparam);
    return elem;
}

boost::multi_array<pattern_entry_ty, 2>
read_pattern_data(
    context &ctx,
    const pattern_header_ty &pattern_header,
    const file_header_ty &file_header
) {
    auto ret = read_lookahead(ctx, [&] (context &ctx) {
        boost::multi_array<pattern_entry_ty, 2> pattern
            (boost::extents[pattern_header.num_rows][file_header.channels]);
        auto begin = pattern.data();
        replicate_(pattern_header.num_rows, [&] () {
            replicate_(file_header.channels, [&] () {
                *begin = read_pattern_entry(ctx);
                ++begin;
            });
        });
        return pattern;
    });
    read_skip(ctx, pattern_header.data_size);
    return ret;
}

template <typename FunTy>
std::vector<pattern_ty>
read_patterns(context &ctx, const file_header_ty &header, FunTy read_row_len) {
    return replicate(header.patterns, [&] () {
        auto pattern_header = read_pattern_header(ctx, read_row_len);
        auto data           = read_pattern_data(ctx, pattern_header, header);
        pattern_ty item     = { pattern_header, data };
        return item;
    });
}

void
read_envelope(context &ctx, std::array<env_point_ty, 12> &env) {
    for_each(env, [&] (env_point_ty &point) {
        point.x = read_uint16_le(ctx);
        point.y = read_uint16_le(ctx);
    });
}

env_flags_ty
env_flags(const uint8_t val) {
    env_flags_ty ret  = env_flags_ty::Empty;
    if (val & 1) ret |= env_flags_ty::Enabled;
    if (val & 2) ret |= env_flags_ty::Sustain;
    if (val & 4) ret |= env_flags_ty::Loop;
    return ret;
}

std::tuple<sample_loop_ty, sample_width_ty>
sample_flags(const uint8_t val) {
    auto lo = val & 3;
    auto loop = lo == 1
        ? sample_loop_ty::ForwardLoop
        : lo == 2
            ? sample_loop_ty::BidiLoop
            : sample_loop_ty::NoLoop;
    auto width = val & 8
        ? sample_width_ty::SixteenBit
        : sample_width_ty::EightBit;
    return std::make_tuple(loop, width);
}

sample_format_ty
sample_format(const uint8_t val) {
    return val == 0xad
        ? sample_format_ty::AdpcmEncoded
        : sample_format_ty::DeltaEncoded;
}

void
read_sample_data(context &ctx, sample_ty &sample) {
    const auto frames = sample.header.sample_frames;
    const auto len = sample.header.width == sample_width_ty::SixteenBit
        ? frames * 2
        : frames;
    sample.data = read_bytestring_slice(ctx, len);
}

void
read_sample_data_in_place(context &ctx, instrument_ty &instr) {
    auto &samples = instr.samples;
    for_each(samples, [&] (sample_ty &sample) {
        read_sample_data(ctx, sample);
    });
}

std::vector<sample_ty>
read_sample_headers(context &ctx, const instrument_header_ty &header) {
    auto samples = replicate(header.num_samples, [&] () {
        sample_ty ret;
        auto &h = ret.header;
        h.sample_frames                = read_uint32_le(ctx);
        h.loop_start                   = read_uint32_le(ctx);
        h.loop_length                  = read_uint32_le(ctx);
        h.vol                          = read_uint8(ctx);
        h.fine_tune                    = read_uint8(ctx);
        std::tie(h.loop_type, h.width) = sample_flags(read_uint8(ctx));
        h.pan                          = read_uint8(ctx);
        h.relative_note                = read_uint8(ctx);
        h.sample_format                = sample_format(read_uint8(ctx));
        read_bytestring_arr(ctx, h.name);
        if (h.width == sample_width_ty::SixteenBit) {
            h.sample_frames /= 2;
            h.loop_start    /= 2;
            h.loop_length   /= 2;
        }
        ret.data = nullptr;
        return ret;
    });
    return samples;
}

instrument_header_ty
read_instrument_header(context &ctx) {
    auto ret = read_lookahead(ctx, [&] (context &ctx) {
        instrument_header_ty ret;
        ret.size = read_uint32_le(ctx);
        uint8_t temp[SizeOfInstrumentHeader] = { 0 };
        read_bytestring_ptr(ctx, std::min(ret.size, SizeOfInstrumentHeader), temp);
        std::shared_ptr<uint8_t> shared(temp, [] (void *) { });
        auto temp_ctx = mkcontext(shared, SizeOfInstrumentHeader);
        read_bytestring_arr(temp_ctx, ret.name);
        ret.type               = read_uint8(temp_ctx);
        ret.num_samples        = read_uint16_le(temp_ctx);
        ret.sample_header_size = read_uint32_le(temp_ctx);
        read_bytestring_arr(temp_ctx, ret.keymap);
        read_envelope(temp_ctx, ret.vol_envelope.nodes);
        read_envelope(temp_ctx, ret.pan_envelope.nodes);
        ret.vol_envelope.num_points       = clamp(read_uint8(temp_ctx), 0, 12);
        ret.pan_envelope.num_points       = clamp(read_uint8(temp_ctx), 0, 12);
        ret.vol_envelope.sustain_point    = read_uint8(temp_ctx);
        ret.vol_envelope.loop_start_point = read_uint8(temp_ctx);
        ret.vol_envelope.loop_end_point   = read_uint8(temp_ctx);
        ret.pan_envelope.sustain_point    = read_uint8(temp_ctx);
        ret.pan_envelope.loop_start_point = read_uint8(temp_ctx);
        ret.pan_envelope.loop_end_point   = read_uint8(temp_ctx);
        ret.vol_envelope.flags            = env_flags(read_uint8(temp_ctx));
        ret.pan_envelope.flags            = env_flags(read_uint8(temp_ctx));
        ret.vibrato_type      = read_uint8(temp_ctx);
        ret.vibrato_sweep     = read_uint8(temp_ctx);
        ret.vibrato_depth     = read_uint8(temp_ctx);
        ret.vibrato_rate      = read_uint8(temp_ctx);
        ret.vol_fadeout       = read_uint16_le(temp_ctx);
        ret.midi_enabled      = read_uint8(temp_ctx);
        ret.midi_channel      = read_uint8(temp_ctx);
        ret.midi_program      = read_uint16_le(temp_ctx);
        ret.pitch_wheel_range = read_uint16_le(temp_ctx);
        ret.mute_computer     = read_uint8(temp_ctx);
        read_skip(temp_ctx, 15); // padding
        return ret;
    });
    read_skip(ctx, ret.size);
    return ret;
}

instrument_ty
read_instrument(context &ctx) {
    instrument_ty ret;
    ret.header = read_instrument_header(ctx);
    ret.samples = read_sample_headers(ctx, ret.header);
    return ret;
}

std::vector<instrument_ty>
read_instruments(context &ctx, const file_header_ty &header) {
    return replicate(header.instruments, [&] () {
        instrument_ty instr = read_instrument(ctx);
        read_sample_data_in_place(ctx, instr);
        return instr;
    });
}

std::vector<instrument_ty>
read_incomplete_instruments(context &ctx, const file_header_ty &header) {
    return replicate(header.instruments, [&] () {
        return read_instrument(ctx);
    });
}

void
unify_instruments_and_samples(
    context &ctx,
    std::vector<instrument_ty> &instrs
) {
    for_each(instrs, [&] (instrument_ty &instr) {
        read_sample_data_in_place(ctx, instr);
    });
}

song
read(context &ctx) {
    song ret;
    read_lookahead(ctx, [&] (context &ctx) {
        ret.header = read_file_header(ctx);
        ret.orders = read_bytestring(ctx, ret.header.orders);
    });
    read_skip(ctx, SizeOfFileHeader + ret.header.size);
    if (ret.header.version >= 0x0104) {
        auto read_row_len = [] (context &ctx) {
            return read_uint16_le(ctx);
        };
        ret.patterns    = read_patterns(ctx, ret.header, read_row_len);
        ret.instruments = read_instruments(ctx, ret.header);
    } else {
        auto read_row_len = [] (context &ctx) {
            return static_cast<uint16_t>(read_uint8(ctx)) + 1;
        };
        ret.instruments = read_incomplete_instruments(ctx, ret.header);
        ret.patterns    = read_patterns(ctx, ret.header, read_row_len);
        unify_instruments_and_samples(ctx, ret.instruments);
    }
    return ret;
}


}
}
}
