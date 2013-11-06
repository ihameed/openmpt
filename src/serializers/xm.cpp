#include "stdafx.h"

#include "xm.h"
#include "binaryparse.h"
#include "samplecodec.h"
#include "../modformat/xm/xm.h"
#include "../../pervasives/pervasives.h"
#include "../legacy_soundlib/Sndfile.h"

#include <type_traits>

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace serializers {
namespace xm {

namespace binaryparse = modplug::serializers::binaryparse;
namespace samplecodec = modplug::serializers::samplecodec;
namespace xm = modplug::modformat::xm;

namespace xm_internal {


std::tuple<volcmd_t, vol_t>
volcmd_of_xm_vol(const xm::vol_type_ty cmd, const uint8_t param) {
    using namespace modplug::modformat::xm;
    auto passthrough = [&] (const volcmd_t retcmd) {
        return std::make_tuple(retcmd, param);
    };
    switch (cmd) {
    case vol_type_ty::Nothing      : return passthrough(VolCmdNone);
    case vol_type_ty::VolSet       : return passthrough(VolCmdVol);
    case vol_type_ty::VolSlideDown : return passthrough(VolCmdSlideDown);
    case vol_type_ty::VolSlideUp   : return passthrough(VolCmdSlideUp);
    case vol_type_ty::VolFineDown  : return passthrough(VolCmdFineDown);
    case vol_type_ty::VolFineUp    : return passthrough(VolCmdFineUp);
    case vol_type_ty::VibSpeed     : return passthrough(VolCmdVibratoSpeed);
    case vol_type_ty::VibDepth     : return passthrough(VolCmdVibratoDepth);
    case vol_type_ty::PanSet       : return std::make_tuple(VolCmdPan, (param * 2) + 2);
    case vol_type_ty::PanSlideLeft : return passthrough(VolCmdPanSlideLeft);
    case vol_type_ty::PanSlideRight: return passthrough(VolCmdPanSlideRight);
    case vol_type_ty::Portamento   : return passthrough(VolCmdPortamento);
    default                        : return passthrough(VolCmdNone);
    }
}

std::tuple<xm::vol_type_ty, uint8_t>
xm_vol_of_volcmd(const volcmd_t cmd, const vol_t param) {
    using namespace modplug::modformat::xm;
    auto passthrough = [&] (const vol_type_ty retcmd) {
        return std::make_tuple(retcmd, param);
    };
    switch (cmd) {
    case VolCmdNone         : return passthrough(vol_type_ty::Nothing);
    case VolCmdVol          : return passthrough(vol_type_ty::VolSet);
    case VolCmdSlideDown    : return passthrough(vol_type_ty::VolSlideDown);
    case VolCmdSlideUp      : return passthrough(vol_type_ty::VolSlideUp);
    case VolCmdFineDown     : return passthrough(vol_type_ty::VolFineDown);
    case VolCmdFineUp       : return passthrough(vol_type_ty::VolFineUp);
    case VolCmdVibratoSpeed : return passthrough(vol_type_ty::VibSpeed);
    case VolCmdVibratoDepth : return passthrough(vol_type_ty::VibDepth);
    case VolCmdPan          : return std::make_tuple(vol_type_ty::PanSet, (param - 2) / 2);
    case VolCmdPanSlideLeft : return passthrough(vol_type_ty::PanSlideLeft);
    case VolCmdPanSlideRight: return passthrough(vol_type_ty::PanSlideRight);
    case VolCmdPortamento   : return passthrough(vol_type_ty::Portamento);
    default                 : return EmptyVol;
    }
};

std::tuple<cmd_t, param_t>
cmd_of_xm_cmd(const xm::fx_type_ty cmd, const uint8_t param) {
    using namespace modplug::modformat::xm;
    auto passthrough = [&] (const cmd_t retcmd) {
        return std::make_tuple(retcmd, param);
    };
    auto extended = [&] (const uint8_t subcmd) {
        return std::make_tuple(CmdModCmdEx, (subcmd << 4) & (param & 0xf));
    };
    auto extrafine = [&] (const uint8_t subcmd) {
        return std::make_tuple(CmdExtendedParameter, (subcmd << 4) & (param & 0xf));
    };
    switch (cmd) {
    case fx_type_ty::Nothing            : return passthrough(CmdNone);
    case fx_type_ty::Arpeggio           : return passthrough(CmdArpeggio);
    case fx_type_ty::PortaUp            : return passthrough(CmdPortaUp);
    case fx_type_ty::PortaDown          : return passthrough(CmdPortaDown);
    case fx_type_ty::Portamento         : return passthrough(CmdPorta);
    case fx_type_ty::Vibrato            : return passthrough(CmdVibrato);
    case fx_type_ty::PortamentoVolSlide : return passthrough(CmdPortaVolSlide);
    case fx_type_ty::VibratoVolSlide    : return passthrough(CmdVibratoVolSlide);
    case fx_type_ty::Tremolo            : return passthrough(CmdTremolo);
    case fx_type_ty::SetPan             : return passthrough(CmdPanning8);
    case fx_type_ty::SampleOffset       : return passthrough(CmdOffset);
    case fx_type_ty::VolSlide           : return passthrough(CmdVolSlide);
    case fx_type_ty::PosJump            : return passthrough(CmdPositionJump);
    case fx_type_ty::SetVol             : return passthrough(CmdVol);
    case fx_type_ty::PatternBreak       : return passthrough(CmdPatternBreak);
    case fx_type_ty::FinePortaUp        : return extended(0x1);
    case fx_type_ty::FinePortaDown      : return extended(0x2);
    case fx_type_ty::GlissControl       : return extended(0x3);
    case fx_type_ty::VibratoControl     : return extended(0x4);
    case fx_type_ty::Finetune           : return extended(0x5);
    case fx_type_ty::LoopControl        : return extended(0x6);
    case fx_type_ty::TremoloControl     : return extended(0x7);
    case fx_type_ty::RetriggerNote      : return extended(0x9);
    case fx_type_ty::FineVolSlideUp     : return extended(0xa);
    case fx_type_ty::FineVolSlideDown   : return extended(0xb);
    case fx_type_ty::NoteCut            : return extended(0xc);
    case fx_type_ty::NoteDelay          : return extended(0xd);
    case fx_type_ty::PatternDelay       : return extended(0xe);
    case fx_type_ty::SetTicksTempo      : return passthrough(param <= 0x1f ? CmdSpeed : CmdTempo);
    case fx_type_ty::SetGlobalVolume    : return passthrough(CmdGlobalVol);
    case fx_type_ty::GlobalVolumeSlide  : return passthrough(CmdVolSlide);
    case fx_type_ty::SetEnvelopePosition: return passthrough(CmdSetEnvelopePosition);
    case fx_type_ty::PanningSlide       : return passthrough(CmdPanningSlide);
    case fx_type_ty::MultiRetrigNote    : return passthrough(CmdRetrig);
    case fx_type_ty::Tremor             : return passthrough(CmdTremor);
    case fx_type_ty::ExtraFinePortaUp   : return extrafine(0x1);
    case fx_type_ty::ExtraFinePortaDown : return extrafine(0x2);
    default                             : return passthrough(CmdNone);
    }
}

std::tuple<xm::fx_type_ty, uint8_t>
xm_cmd_of_cmd(const cmd_t cmd, const uint8_t param) {
    using namespace modplug::modformat::xm;
    auto passthrough = [&] (const fx_type_ty retcmd) {
        return std::make_tuple(retcmd, param);
    };
    auto clamped_passthrough = [&] (const fx_type_ty retcmd, const uint8_t min, const uint8_t max) {
        return std::make_tuple(retcmd, clamp(min, max, param));
    };
    auto extended = [&] (const fx_type_ty retcmd) {
        return std::make_tuple(retcmd, param & 0xf);
    };
    switch (cmd) {
    case CmdNone                 : return passthrough(fx_type_ty::Nothing);
    case CmdArpeggio             : return passthrough(fx_type_ty::Arpeggio);
    case CmdPortaUp              : return passthrough(fx_type_ty::PortaUp);
    case CmdPortaDown            : return passthrough(fx_type_ty::PortaDown);
    case CmdPorta                : return passthrough(fx_type_ty::Portamento);
    case CmdVibrato              : return passthrough(fx_type_ty::Vibrato);
    case CmdPortaVolSlide        : return passthrough(fx_type_ty::PortamentoVolSlide);
    case CmdVibratoVolSlide      : return passthrough(fx_type_ty::VibratoVolSlide);
    case CmdTremolo              : return passthrough(fx_type_ty::Tremolo);
    case CmdPanning8             : return passthrough(fx_type_ty::SetPan);
    case CmdOffset               : return passthrough(fx_type_ty::SampleOffset);
    case CmdVolSlide             : return passthrough(fx_type_ty::VolSlide);
    case CmdPositionJump         : return passthrough(fx_type_ty::PosJump);
    case CmdVol                  : return passthrough(fx_type_ty::SetVol);
    case CmdPatternBreak         : return passthrough(fx_type_ty::PatternBreak);
    case CmdRetrig               : return passthrough(fx_type_ty::RetriggerNote);
    case CmdSpeed                : return clamped_passthrough(fx_type_ty::SetTicksTempo, 0, 0x1f);
    case CmdTempo                : return clamped_passthrough(fx_type_ty::SetTicksTempo, 0x20, 0xff);
    case CmdTremor               : return passthrough(fx_type_ty::Tremor);
    case CmdModCmdEx             : switch (param >> 4) {
        case 0x1: return extended(fx_type_ty::FinePortaUp);
        case 0x2: return extended(fx_type_ty::FinePortaDown);
        case 0x3: return extended(fx_type_ty::GlissControl);
        case 0x4: return extended(fx_type_ty::VibratoControl);
        case 0x5: return extended(fx_type_ty::Finetune);
        case 0x6: return extended(fx_type_ty::LoopControl);
        case 0x7: return extended(fx_type_ty::TremoloControl);
        case 0x9: return extended(fx_type_ty::RetriggerNote);
        case 0xa: return extended(fx_type_ty::FineVolSlideUp);
        case 0xb: return extended(fx_type_ty::FineVolSlideDown);
        case 0xc: return extended(fx_type_ty::NoteCut);
        case 0xd: return extended(fx_type_ty::NoteDelay);
        case 0xe: return extended(fx_type_ty::PatternDelay);
        };
    case CmdS3mCmdEx             : return EmptyFx;
    case CmdChannelVol           : return EmptyFx;
    case CmdChannelVolSlide      : return EmptyFx;
    case CmdGlobalVol            : return passthrough(fx_type_ty::SetGlobalVolume);
    case CmdGlobalVolSlide       : return passthrough(fx_type_ty::GlobalVolumeSlide);
    case CmdKeyOff               : return EmptyFx;
    case CmdFineVibrato          : return EmptyFx;
    case CmdPanbrello            : return EmptyFx;
    case CmdExtraFinePortaUpDown : switch (param >> 4) {
        case 0x1: return extended(fx_type_ty::ExtraFinePortaUp);
        case 0x2: return extended(fx_type_ty::ExtraFinePortaDown);
        };
    case CmdPanningSlide         : return passthrough(fx_type_ty::PanningSlide);
    case CmdSetEnvelopePosition  : return passthrough(fx_type_ty::SetEnvelopePosition);
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
convert_env_flags(const xm::env_flags_ty flags) {
    uint32_t ret = 0;
    if (is_set(flags, xm::env_flags_ty::Enabled)) ret |= ENV_ENABLED;
    if (is_set(flags, xm::env_flags_ty::Loop))    ret |= ENV_LOOP;
    if (is_set(flags, xm::env_flags_ty::Sustain)) ret |= ENV_SUSTAIN;
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
    ref.sample_data = modplug::legacy_soundlib::alloc_sample(
        bytelength
    );
    ref.length = header.sample_frames;
    ref.flags = 0;
    switch (header.width) {
    case xm::sample_width_ty::SixteenBit:
        ref.flags |= CHN_16BIT;
        break;
    }
    switch (header.loop_type) {
    case xm::sample_loop_ty::ForwardLoop:
        ref.flags |= CHN_LOOP;
        break;
    case xm::sample_loop_ty::BidiLoop:
        ref.flags |= CHN_LOOP | CHN_PINGPONGLOOP;
        break;
    }
    switch (header.width) {
        case xm::sample_width_ty::SixteenBit:
            samplecodec::copy_and_convert<
                samplecodec::byte_convert_ty,
                samplecodec::delta_decode,
                std::identity,
                samplecodec::identity_convert_ty,
                int16_t,
                int16_t,
                uint8_t
                >
                (reinterpret_cast<int16_t *>(ref.sample_data), sample.data, header.sample_frames);
            break;
        case xm::sample_width_ty::EightBit:
            samplecodec::copy_and_convert<
                samplecodec::byte_convert_ty,
                samplecodec::delta_decode,
                std::identity,
                samplecodec::identity_convert_ty,
                int8_t,
                int8_t,
                uint8_t
                >
                (reinterpret_cast<int8_t *>(ref.sample_data), sample.data, header.sample_frames);
            break;
    }
    ref.global_volume = 64;
    ref.default_volume = clamp(0, 256, header.vol * 4);
    ref.default_pan = header.pan;
    ref.nFineTune = header.fine_tune;
    ref.RelativeTone = header.relative_note;
    ref.flags |= CHN_PANNING;
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
    module_renderer &song, modinstrument_t *&ref,
    const xm::instrument_ty &instr,
    const size_t samps_len)
{
    ref = new modinstrument_t;
    *ref = song.m_defaultInstrument;
    memcpy(ref->name, instr.header.name.data(), 22);
    SpaceToNullStringFixed<22>(ref->name);
    ref->nPluginVelocityHandling = PLUGIN_VELOCITYHANDLING_CHANNEL;
    ref->nPluginVolumeHandling   = PLUGIN_VOLUMEHANDLING_IGNORE;
    ref->midi_program            = instr.header.type;
    ref->fadeout                 = instr.header.vol_fadeout;
    ref->default_pan             = 128;
    ref->pitch_pan_center        = 5 * 12;
    song.SetDefaultInstrumentValues(ref);
    convert_env(ref->volume_envelope, instr.header.vol_envelope);
    convert_env(ref->panning_envelope, instr.header.pan_envelope);
    convert_note_map(ref->NoteMap, ref->Keyboard, samps_len, instr.header);
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
            evt.volcmd = VolCmdNone;
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
    try {
        song.m_nChannels = 0;
        auto xm_song = xm::read(ctx);
        auto &fileheader = xm_song.header;
        assign_without_padding(
            song.song_name,
            (const char *)fileheader.song_name.data(),
            fileheader.song_name.size()
        );
        song.Order.ReadAsByte(
            xm_song.orders.data(),
            xm_song.orders.size(),
            xm_song.orders.size()
        );
        song.m_nChannels = fileheader.channels;
        song.m_nInstruments = fileheader.instruments;
        song.m_nDefaultSpeed = clamp(fileheader.speed, 1, 31);
        song.m_nDefaultTempo = clamp(fileheader.tempo, 32, 512);
        song.m_nType = MOD_TYPE_XM;
        song.m_nMinPeriod = 27;
        song.m_nMaxPeriod = 54784;

        size_t instr_idx = 1;
        size_t sample_idx = 1;
        size_t samps_len = 0;
        for_each(xm_song.instruments, [&] (const xm::instrument_ty &instr) {
            xm_internal::import_instrument(song, song.Instruments[instr_idx], instr, samps_len);
            for_each(instr.samples, [&] (const xm::sample_ty &sample) {
                xm_internal::import_sample(song.Samples[sample_idx], instr, sample);
                ++sample_idx;
            });
            samps_len += instr.header.num_samples;
            ++instr_idx;
        });
        song.m_nSamples = samps_len;

        size_t pat_idx = 0;
        for_each(xm_song.patterns, [&] (const xm::pattern_ty &pattern) {
            xm_internal::import_pattern(song, pattern, pat_idx, fileheader);
            ++pat_idx;
        });
        return true;
    } catch (...) {
        return false;
    }
}


}
}
}
