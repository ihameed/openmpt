#include "stdafx.h"

#include "xm.hpp"

#include <type_traits>

#include "legacy_util.hpp"
#include "samplecodec.hpp"
#include "../modformat/xm/xm.hpp"
#include "../../pervasives/binaryparse.hpp"
#include "../../pervasives/pervasives.hpp"
#include "../legacy_soundlib/Sndfile.h"

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace serializers {
namespace xm {

namespace binaryparse = modplug::pervasives::binaryparse;
namespace samplecodec = modplug::serializers::samplecodec;
namespace xm = modplug::modformat::xm;

namespace internal {

std::tuple<volcmd_t, vol_t>
volcmd_of_xm_vol(const xm::vol_ty cmd, const uint8_t param) {
    using namespace xm;
    auto pass = [&] (const volcmd_t retcmd) {
        return std::make_tuple(retcmd, param);
    };
    switch (cmd) {
    case vol_ty::Nothing      : return pass(VolCmdNone);
    case vol_ty::VolSet       : return pass(VolCmdVol);
    case vol_ty::VolSlideDown : return pass(VolCmdSlideDown);
    case vol_ty::VolSlideUp   : return pass(VolCmdSlideUp);
    case vol_ty::VolFineDown  : return pass(VolCmdFineDown);
    case vol_ty::VolFineUp    : return pass(VolCmdFineUp);
    case vol_ty::VibSpeed     : return pass(VolCmdVibratoSpeed);
    case vol_ty::VibDepth     : return pass(VolCmdVibratoDepth);
    case vol_ty::PanSet       : return std::make_tuple(VolCmdPan, (param * 2) + 2);
    case vol_ty::PanSlideLeft : return pass(VolCmdPanSlideLeft);
    case vol_ty::PanSlideRight: return pass(VolCmdPanSlideRight);
    case vol_ty::Portamento   : return pass(VolCmdPortamento);
    default                   : return pass(VolCmdNone);
    }
}

std::tuple<xm::vol_ty, uint8_t>
xm_vol_of_volcmd(const volcmd_t cmd, const vol_t param) {
    using namespace xm;
    auto pass = [&] (const vol_ty retcmd) {
        return std::make_tuple(retcmd, param);
    };
    switch (cmd) {
    case VolCmdNone         : return pass(vol_ty::Nothing);
    case VolCmdVol          : return pass(vol_ty::VolSet);
    case VolCmdSlideDown    : return pass(vol_ty::VolSlideDown);
    case VolCmdSlideUp      : return pass(vol_ty::VolSlideUp);
    case VolCmdFineDown     : return pass(vol_ty::VolFineDown);
    case VolCmdFineUp       : return pass(vol_ty::VolFineUp);
    case VolCmdVibratoSpeed : return pass(vol_ty::VibSpeed);
    case VolCmdVibratoDepth : return pass(vol_ty::VibDepth);
    case VolCmdPan          : return std::make_tuple(vol_ty::PanSet, (param - 2) / 2);
    case VolCmdPanSlideLeft : return pass(vol_ty::PanSlideLeft);
    case VolCmdPanSlideRight: return pass(vol_ty::PanSlideRight);
    case VolCmdPortamento   : return pass(vol_ty::Portamento);
    default                 : return EmptyVol;
    }
};

std::tuple<cmd_t, param_t>
cmd_of_xm_cmd(const xm::cmd_ty cmd, const uint8_t param) {
    using namespace xm;
    auto pass = [&] (const cmd_t retcmd) {
        return std::make_tuple(retcmd, param);
    };
    auto extended = [&] (const uint8_t subcmd) {
        return std::make_tuple(CmdModCmdEx, (subcmd << 4) & (param & 0xf));
    };
    auto extrafine = [&] (const uint8_t subcmd) {
        return std::make_tuple(CmdExtraFinePortaUpDown, (subcmd << 4) & (param & 0xf));
    };
    switch (cmd) {
    case cmd_ty::Nothing            : return pass(CmdNone);
    case cmd_ty::Arpeggio           : return pass(CmdArpeggio);
    case cmd_ty::PortaUp            : return pass(CmdPortaUp);
    case cmd_ty::PortaDown          : return pass(CmdPortaDown);
    case cmd_ty::Portamento         : return pass(CmdPorta);
    case cmd_ty::Vibrato            : return pass(CmdVibrato);
    case cmd_ty::PortamentoVolSlide : return pass(CmdPortaVolSlide);
    case cmd_ty::VibratoVolSlide    : return pass(CmdVibratoVolSlide);
    case cmd_ty::Tremolo            : return pass(CmdTremolo);
    case cmd_ty::SetPan             : return pass(CmdPanning8);
    case cmd_ty::SampleOffset       : return pass(CmdOffset);
    case cmd_ty::VolSlide           : return pass(CmdVolSlide);
    case cmd_ty::PosJump            : return pass(CmdPositionJump);
    case cmd_ty::SetVol             : return pass(CmdVol);
    case cmd_ty::PatternBreak       : return pass(CmdPatternBreak);
    case cmd_ty::FinePortaUp        : return extended(0x1);
    case cmd_ty::FinePortaDown      : return extended(0x2);
    case cmd_ty::GlissControl       : return extended(0x3);
    case cmd_ty::VibratoControl     : return extended(0x4);
    case cmd_ty::Finetune           : return extended(0x5);
    case cmd_ty::LoopControl        : return extended(0x6);
    case cmd_ty::TremoloControl     : return extended(0x7);
    case cmd_ty::RetriggerNote      : return extended(0x9);
    case cmd_ty::FineVolSlideUp     : return extended(0xa);
    case cmd_ty::FineVolSlideDown   : return extended(0xb);
    case cmd_ty::NoteCut            : return extended(0xc);
    case cmd_ty::NoteDelay          : return extended(0xd);
    case cmd_ty::PatternDelay       : return extended(0xe);
    case cmd_ty::SetTicksTempo      : return pass(param <= 0x1f ? CmdSpeed : CmdTempo);
    case cmd_ty::SetGlobalVolume    : return pass(CmdGlobalVol);
    case cmd_ty::GlobalVolumeSlide  : return pass(CmdVolSlide);
    case cmd_ty::SetEnvelopePosition: return pass(CmdSetEnvelopePosition);
    case cmd_ty::PanningSlide       : return pass(CmdPanningSlide);
    case cmd_ty::MultiRetrigNote    : return pass(CmdRetrig);
    case cmd_ty::Tremor             : return pass(CmdTremor);
    case cmd_ty::ExtraFinePortaUp   : return extrafine(0x1);
    case cmd_ty::ExtraFinePortaDown : return extrafine(0x2);
    default                         : return pass(CmdNone);
    }
}

std::tuple<xm::cmd_ty, uint8_t>
xm_cmd_of_cmd(const cmd_t cmd, const uint8_t param) {
    using namespace modplug::modformat::xm;
    auto pass = [&] (const cmd_ty retcmd) {
        return std::make_tuple(retcmd, param);
    };
    auto clamped_pass = [&] (const cmd_ty retcmd, const uint8_t min, const uint8_t max) {
        return std::make_tuple(retcmd, clamp(min, max, param));
    };
    auto extended = [&] (const cmd_ty retcmd) {
        return std::make_tuple(retcmd, param & 0xf);
    };
    switch (cmd) {
    case CmdNone                 : return pass(cmd_ty::Nothing);
    case CmdArpeggio             : return pass(cmd_ty::Arpeggio);
    case CmdPortaUp              : return pass(cmd_ty::PortaUp);
    case CmdPortaDown            : return pass(cmd_ty::PortaDown);
    case CmdPorta                : return pass(cmd_ty::Portamento);
    case CmdVibrato              : return pass(cmd_ty::Vibrato);
    case CmdPortaVolSlide        : return pass(cmd_ty::PortamentoVolSlide);
    case CmdVibratoVolSlide      : return pass(cmd_ty::VibratoVolSlide);
    case CmdTremolo              : return pass(cmd_ty::Tremolo);
    case CmdPanning8             : return pass(cmd_ty::SetPan);
    case CmdOffset               : return pass(cmd_ty::SampleOffset);
    case CmdVolSlide             : return pass(cmd_ty::VolSlide);
    case CmdPositionJump         : return pass(cmd_ty::PosJump);
    case CmdVol                  : return pass(cmd_ty::SetVol);
    case CmdPatternBreak         : return pass(cmd_ty::PatternBreak);
    case CmdRetrig               : return pass(cmd_ty::RetriggerNote);
    case CmdSpeed                : return clamped_pass(cmd_ty::SetTicksTempo, 0, 0x1f);
    case CmdTempo                : return clamped_pass(cmd_ty::SetTicksTempo, 0x20, 0xff);
    case CmdTremor               : return pass(cmd_ty::Tremor);
    case CmdModCmdEx             : switch (param >> 4) {
        case 0x1: return extended(cmd_ty::FinePortaUp);
        case 0x2: return extended(cmd_ty::FinePortaDown);
        case 0x3: return extended(cmd_ty::GlissControl);
        case 0x4: return extended(cmd_ty::VibratoControl);
        case 0x5: return extended(cmd_ty::Finetune);
        case 0x6: return extended(cmd_ty::LoopControl);
        case 0x7: return extended(cmd_ty::TremoloControl);
        case 0x9: return extended(cmd_ty::RetriggerNote);
        case 0xa: return extended(cmd_ty::FineVolSlideUp);
        case 0xb: return extended(cmd_ty::FineVolSlideDown);
        case 0xc: return extended(cmd_ty::NoteCut);
        case 0xd: return extended(cmd_ty::NoteDelay);
        case 0xe: return extended(cmd_ty::PatternDelay);
        };
    case CmdS3mCmdEx             : return EmptyFx;
    case CmdChannelVol           : return EmptyFx;
    case CmdChannelVolSlide      : return EmptyFx;
    case CmdGlobalVol            : return pass(cmd_ty::SetGlobalVolume);
    case CmdGlobalVolSlide       : return pass(cmd_ty::GlobalVolumeSlide);
    case CmdKeyOff               : return EmptyFx;
    case CmdFineVibrato          : return EmptyFx;
    case CmdPanbrello            : return EmptyFx;
    case CmdExtraFinePortaUpDown : switch (param >> 4) {
        case 0x1: return extended(cmd_ty::ExtraFinePortaUp);
        case 0x2: return extended(cmd_ty::ExtraFinePortaDown);
        };
    case CmdPanningSlide         : return pass(cmd_ty::PanningSlide);
    case CmdSetEnvelopePosition  : return pass(cmd_ty::SetEnvelopePosition);
    case CmdMidi                 : return EmptyFx;
    case CmdSmoothMidi           : return EmptyFx;
    case CmdDelayCut             : return EmptyFx;
    case CmdExtendedParameter    : return EmptyFx;
    case CmdNoteSlideUp          : return EmptyFx;
    case CmdNoteSlideDown        : return EmptyFx;
    default                      : return EmptyFx;
    }
}

uint32_t
convert_env_flags(const bitset<xm::env_flags_ty> flags) {
    uint32_t ret = 0;
    if (bitset_is_set(flags, xm::env_flags_ty::Enabled)) ret |= ENV_ENABLED;
    if (bitset_is_set(flags, xm::env_flags_ty::Loop))    ret |= ENV_LOOP;
    if (bitset_is_set(flags, xm::env_flags_ty::Sustain)) ret |= ENV_SUSTAIN;
    return ret;
}

void
convert_env(modenvelope_t &ref, const xm::env_ty &env) {
    ref.flags = convert_env_flags(env.flags);
    ref.num_nodes = env.num_points;
    ref.sustain_start = env.sustain_point;
    ref.sustain_end = env.sustain_point;
    ref.loop_start = env.loop_start_point;
    ref.loop_end = env.loop_end_point;
    uint16_t *old = nullptr;
    for (size_t i = 0; i < env.nodes.size(); ++i) {
        ref.Ticks[i] = env.nodes[i].x;
        ref.Values[i] = static_cast<uint8_t>(env.nodes[i].y);
        auto &cur = ref.Ticks[i];
        if (old && cur < *old) {
            cur &= 0xff;
            cur += *old & 0xff00;
            if (cur < *old) cur += 0x100;
        }
        old = &cur;
    }
}

void
convert_note_map(
    uint8_t *notemap, uint16_t *keyboard, size_t samps_len,
    const xm::instrument_header_ty &instr)
{
    for (size_t i = 0; i < 128; ++i) { // HACK
        keyboard[i] = 0;
    }
    for (size_t i = 0; i < instr.keymap.size(); ++i) {
        notemap[i + 12] = i + 1 + 12;
        if (instr.keymap[i] < instr.num_samples) {
            keyboard[i + 12] = samps_len + instr.keymap[i] + 1;
        }
    }
}

void
import_sample(
    modsample_t &ref, const xm::instrument_ty &instr,
    const xm::sample_ty &sample)
{
    auto &header = sample.header;
    auto bytelength = header.sample_frames + 6;
    ref.loop_start = header.loop_start;
    ref.loop_end = header.loop_length;
    if (header.width == xm::sample_width_ty::SixteenBit) {
        bytelength *= 2;
        ref.loop_start *= 2;
        ref.loop_end *= 2;
    }
    ref.sample_data.generic = modplug::legacy_soundlib::alloc_sample(bytelength);
    ref.length = header.sample_frames;
    ref.flags = sflags_ty();
    switch (header.width) {
    case xm::sample_width_ty::SixteenBit:
        ref.sample_tag = stag_ty::Int16;
        break;
    }
    switch (header.loop_type) {
    case xm::sample_loop_ty::ForwardLoop:
        bitset_add(ref.flags, sflag_ty::Loop);
        break;
    case xm::sample_loop_ty::BidiLoop:
        bitset_add(ref.flags, sflag_ty::Loop);
        bitset_add(ref.flags, sflag_ty::BidiLoop);
        break;
    }
    switch (header.width) {
        case xm::sample_width_ty::SixteenBit:
            samplecodec::copy_interleaved
                < samplecodec::signed_bytes_ty, samplecodec::delta_decode, 1 >
                (ref.sample_data.int16, sample.data, header.sample_frames);
            break;
        case xm::sample_width_ty::EightBit:
            samplecodec::copy_interleaved
                < samplecodec::signed_bytes_ty, samplecodec::delta_decode, 1 >
                (ref.sample_data.int8, sample.data, header.sample_frames);
            break;
    }
    ref.global_volume = 64;
    ref.default_volume = clamp(0, 256, header.vol * 4);
    ref.default_pan = header.pan;
    ref.nFineTune = header.fine_tune;
    ref.RelativeTone = header.relative_note;
    bitset_add(ref.flags, sflag_ty::ForcedPanning);
    ref.vibrato_type = instr.header.vibrato_type;
    ref.vibrato_sweep = instr.header.vibrato_sweep;
    ref.vibrato_depth = instr.header.vibrato_depth;
    ref.vibrato_rate = instr.header.vibrato_rate;
    ref.c5_samplerate = frequency_of_transpose(ref.RelativeTone, ref.nFineTune);
    memcpy(ref.legacy_filename, header.name.data(), 22);
    SpaceToNullStringFixed<21>(ref.legacy_filename);
}

void
import_instrument(
    modinstrument_t &ref,
    const xm::instrument_ty &instr,
    const size_t samps_len)
{
    memcpy(ref.name, instr.header.name.data(), 22);
    SpaceToNullStringFixed<22>(ref.name);
    ref.nPluginVelocityHandling = PLUGIN_VELOCITYHANDLING_CHANNEL;
    ref.nPluginVolumeHandling   = PLUGIN_VOLUMEHANDLING_IGNORE;
    ref.midi_program            = instr.header.type;
    ref.fadeout                 = instr.header.vol_fadeout;
    ref.default_pan             = 128;
    ref.pitch_pan_center        = 5 * 12;
    convert_env(ref.volume_envelope, instr.header.vol_envelope);
    convert_env(ref.panning_envelope, instr.header.pan_envelope);
    convert_note_map(ref.NoteMap, ref.Keyboard, samps_len, instr.header);
}

void
import_pattern(
    module_renderer &song, const xm::pattern_ty &pattern,
    const size_t pat_idx, const xm::file_header_ty &fileheader)
{
    auto &data = pattern.data;
    song.Patterns.Insert(pat_idx, pattern.header.num_rows);
    auto *source = pattern.data.origin();
    auto &outpattern = song.Patterns[pat_idx];
    for (size_t i = 0; i < pattern.header.num_rows; ++i) {
        auto row = outpattern.GetRow(i);
        for (size_t j = 0; j < fileheader.channels; ++j) {
            auto &evt = row[j];
            evt.note = source->note;
            evt.instr = source->instrument;
            std::tie(evt.volcmd, evt.vol) =
                volcmd_of_xm_vol(source->volcmd, source->volcmd_parameter);
            std::tie(evt.command, evt.param) =
                cmd_of_xm_cmd(source->effect_type, source->effect_parameter);
            ++source;
        }
    }
}

}

bool
read(module_renderer &song, binaryparse::context &ctx) {
    using namespace internal;
    try {
        song.m_nChannels = 0;
        auto xm_song_ptr = xm::read(ctx);
        auto &xm_song = *xm_song_ptr;
        auto &fileheader = xm_song.header;
        assign_without_padding(
            song.song_name,
            reinterpret_cast<const char *>(fileheader.song_name.data()),
            fileheader.song_name.size()
        );
        song.Order.ReadAsByte(
            xm_song.orders.data(),
            xm_song.orders.size(),
            xm_song.orders.size()
        );
        song.m_nChannels = fileheader.channels;
        song.m_nInstruments = fileheader.instruments;
        song.m_nDefaultSpeed = clamp(fileheader.speed, (uint16_t) 1, (uint16_t) 31);
        song.m_nDefaultTempo = clamp(fileheader.tempo, (uint16_t) 32, (uint16_t) 512);
        song.m_nType = MOD_TYPE_XM;
        song.m_nMinPeriod = 27;
        song.m_nMaxPeriod = 54784;

        auto alloc_instr = mk_alloc_instr(song);
        auto alloc_sample = mk_alloc_sample(song);
        auto alloc_pattern = mk_alloc_pattern(song);

        size_t samps_len = 0;

        for (const auto &instr : xm_song.instruments) {
            auto &instref = alloc_instr();
            import_instrument(instref, instr, samps_len);
            for (const auto &sample : instr.samples) {
                auto &smpref = alloc_sample();
                import_sample(smpref, instr, sample);
                ++samps_len;
            };
        };
        song.m_nSamples = samps_len;

        size_t pat_idx = 0;
        for (const auto &pattern : xm_song.patterns) {
            import_pattern(song, pattern, pat_idx, fileheader);
            ++pat_idx;
        }
        return true;
    } catch (...) {
        return false;
    }
}


}
}
}
