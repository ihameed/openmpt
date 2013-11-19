#include "stdafx.h"

#include "it.hpp"

#include <functional>

#include "legacy_util.h"
#include "samplecodec.h"
#include "../modformat/it/it.hpp"
#include "../../pervasives/binaryparse.h"
#include "../../pervasives/pervasives.h"
#include "../legacy_soundlib/Sndfile.h"

using namespace modplug::pervasives;
using namespace modplug::serializers;
using namespace modplug::tracker;

namespace modplug {
namespace serializers {
namespace it {

namespace binaryparse = modplug::pervasives::binaryparse;
namespace samplecodec = modplug::serializers::samplecodec;
namespace it = modplug::modformat::it;
namespace mpt = modplug::modformat::mptexts;

namespace internal {

std::tuple<volcmd_t, vol_t>
vol_of_it_vol(const it::vol_ty vol, const uint8_t param) {
    using namespace it;
    auto pass = [&] (const volcmd_t retcmd) {
        return std::make_tuple(retcmd, param);
    };
    switch (vol) {
    case vol_ty::Nothing       : return pass(VolCmdNone);
    case vol_ty::VolSet        : return pass(VolCmdVol);
    case vol_ty::PanSet        : return pass(VolCmdPan);
    case vol_ty::VolFineUp     : return pass(VolCmdFineUp);
    case vol_ty::VolFineDown   : return pass(VolCmdFineDown);
    case vol_ty::VolSlideUp    : return pass(VolCmdSlideUp);
    case vol_ty::VolSlideDown  : return pass(VolCmdSlideDown);
    case vol_ty::PortamentoDown: return pass(VolCmdPortamentoDown);
    case vol_ty::PortamentoUp  : return pass(VolCmdPortamentoUp);
    case vol_ty::Portamento    : return pass(VolCmdPortamento);
    case vol_ty::VibDepth      : return pass(VolCmdVibratoDepth);
    default                    : return pass(VolCmdNone);
    }
}

std::tuple<cmd_t, param_t>
cmd_of_it_cmd(const it::cmd_ty cmd, const uint8_t param) {
    using namespace it;
    auto pass = [&] (const cmd_t retcmd) {
        return std::make_tuple(retcmd, param);
    };
    auto extrafineslidedown = [&] (const cmd_t retcmd) {
        return std::make_tuple(retcmd, param | 0xe0);
    };
    auto extrafineslideup = [&] (const cmd_t retcmd) {
        return std::make_tuple(retcmd, (param << 4) | 0xe0);
    };
    auto fineslidedown = [&] (const cmd_t retcmd) {
        return std::make_tuple(retcmd, param | 0xf0);
    };
    auto fineslideup = [&] (const cmd_t retcmd) {
        return std::make_tuple(retcmd, (param << 4) | 0x0f);
    };
    auto slidedown = [&] (const cmd_t retcmd) {
        return std::make_tuple(retcmd, param & 0xf);
    };
    auto slideup = [&] (const cmd_t retcmd) {
        return std::make_tuple(retcmd, param << 4);
    };
    auto extended = [&] (const uint8_t subcmd) {
        return std::make_tuple(CmdS3mCmdEx, (subcmd << 4) & (param & 0xf));
    };
    switch (cmd) {
    case cmd_ty::Nothing    : return pass(CmdNone);
    case cmd_ty::SetSpeed   : return pass(CmdSpeed);
    case cmd_ty::JumpToOrder: return pass(CmdPositionJump);
    case cmd_ty::BreakToRow : return pass(CmdPatternBreak);

    case cmd_ty::VolSlideContinue: return pass(CmdVolSlide);
    case cmd_ty::VolSlideUp      : return slideup(CmdVolSlide);
    case cmd_ty::VolSlideDown    : return slidedown(CmdVolSlide);
    case cmd_ty::FineVolSlideUp  : return fineslideup(CmdVolSlide);
    case cmd_ty::FineVolSlideDown: return fineslidedown(CmdVolSlide);

    case cmd_ty::PortamentoDown         : return pass(CmdPortaDown);
    case cmd_ty::FinePortamentoDown     : return fineslidedown(CmdPortaDown);
    case cmd_ty::ExtraFinePortamentoDown: return extrafineslidedown(CmdPortaDown);
    case cmd_ty::PortamentoUp           : return pass(CmdPortaUp);
    case cmd_ty::FinePortamentoUp       : return fineslidedown(CmdPortaUp);
    case cmd_ty::ExtraFinePortamentoUp  : return extrafineslidedown(CmdPortaUp);
    case cmd_ty::Portamento             : return pass(CmdPorta);

    case cmd_ty::Vibrato : return pass(CmdVibrato);
    case cmd_ty::Tremor  : return pass(CmdTremor);
    case cmd_ty::Arpeggio: return pass(CmdArpeggio);

    case cmd_ty::VolSlideVibratoContinue: return pass(CmdVibratoVolSlide);
    case cmd_ty::VolSlideVibratoUp      : return slideup(CmdVibratoVolSlide);
    case cmd_ty::VolSlideVibratoDown    : return slidedown(CmdVibratoVolSlide);
    case cmd_ty::FineVolSlideVibratoUp  : return fineslideup(CmdVibratoVolSlide);
    case cmd_ty::FineVolSlideVibratoDown: return fineslidedown(CmdVibratoVolSlide);

    case cmd_ty::VolSlidePortaContinue: return pass(CmdPortaVolSlide);
    case cmd_ty::VolSlidePortaUp      : return slideup(CmdPortaVolSlide);
    case cmd_ty::VolSlidePortaDown    : return slidedown(CmdPortaVolSlide);
    case cmd_ty::FineVolSlidePortaUp  : return fineslideup(CmdPortaVolSlide);
    case cmd_ty::FineVolSlidePortaDown: return fineslidedown(CmdPortaVolSlide);

    case cmd_ty::SetChannelVol          : return pass(CmdChannelVol);
    case cmd_ty::ChannelVolSlideContinue: return pass(CmdChannelVolSlide);
    case cmd_ty::ChannelVolSlideUp      : return slideup(CmdChannelVolSlide);
    case cmd_ty::ChannelVolSlideDown    : return slidedown(CmdChannelVolSlide);
    case cmd_ty::ChannelFineVolSlideUp  : return fineslideup(CmdChannelVolSlide);
    case cmd_ty::ChannelFineVolSlideDown: return fineslidedown(CmdChannelVolSlide);

    case cmd_ty::SetSampleOffset: return pass(CmdOffset);

    case cmd_ty::PanSlideContinue  : return pass(CmdPanningSlide);
    case cmd_ty::PanSlideLeft      : return slideup(CmdPanningSlide);
    case cmd_ty::PanSlideRight     : return slidedown(CmdPanningSlide);
    case cmd_ty::FinePanSlideLeft  : return fineslideup(CmdPanningSlide);
    case cmd_ty::FinePanSlideRight : return fineslidedown(CmdPanningSlide);

    case cmd_ty::Retrigger: return pass(CmdRetrig);
    case cmd_ty::Tremolo  : return pass(CmdTremolo);

    case cmd_ty::GlissandoControl   : return extended(0x1);
    case cmd_ty::VibratoWaveform    : return extended(0x3);
    case cmd_ty::TremoloWaveform    : return extended(0x4);
    case cmd_ty::PanbrelloWaveform  : return extended(0x5);
    case cmd_ty::FinePatternDelay   : return extended(0x6);
    case cmd_ty::InstrControl       : return extended(0x7);
    case cmd_ty::SetPanShort        : return extended(0x8);
    case cmd_ty::SoundControl       : return extended(0x9);
    case cmd_ty::SetHighSampleOffset: return extended(0xa);
    case cmd_ty::PatternLoop        : return extended(0xb);
    case cmd_ty::NoteCut            : return extended(0xc);
    case cmd_ty::NoteDelay          : return extended(0xd);
    case cmd_ty::PatternDelay       : return extended(0xe);
    case cmd_ty::SetActiveMidiMacro : return extended(0xf);

    case cmd_ty::SetTempo   : return pass(CmdTempo);
    case cmd_ty::FineVibrato: return pass(CmdFineVibrato);

    case cmd_ty::SetGlobalVol          : return pass(CmdGlobalVol);
    case cmd_ty::GlobalVolSlideContinue: return pass(CmdGlobalVolSlide);
    case cmd_ty::GlobalVolSlideUp      : return slideup(CmdGlobalVolSlide);
    case cmd_ty::GlobalVolSlideDown    : return slidedown(CmdGlobalVolSlide);
    case cmd_ty::GlobalFineVolSlideUp  : return fineslideup(CmdGlobalVolSlide);
    case cmd_ty::GlobalFineVolSlideDown: return fineslidedown(CmdGlobalVolSlide);

    case cmd_ty::SetPan           : return pass(CmdPanning8);
    case cmd_ty::Panbrello        : return pass(CmdPanbrello);
    case cmd_ty::MidiMacro        : return pass(CmdMidi);
    case cmd_ty::SmoothMidiMacro  : return pass(CmdSmoothMidi);
    case cmd_ty::DelayCut         : return pass(CmdDelayCut);
    case cmd_ty::ExtendedParameter: return pass(CmdExtendedParameter);
    default: return pass(CmdNone);
    }
}

uint8_t
uint8_of_nna(const it::nna_ty val) {
    using namespace it;
    switch (val) {
    case nna_ty::Continue : return 1;
    case nna_ty::NoteOff  : return 2;
    case nna_ty::NoteFade : return 3;
    default               : return 0;
    }
}

uint8_t
uint8_of_dct(const it::dct_ty val) {
    using namespace it;
    switch (val) {
    case dct_ty::Note       : return 1;
    case dct_ty::Sample     : return 2;
    case dct_ty::Instrument : return 3;
    default                 : return 0;
    }
}

uint8_t
uint8_of_dca(const it::dca_ty val) {
    using namespace it;
    switch (val) {
    case dca_ty::NoteOff  : return 1;
    case dca_ty::NoteFade : return 2;
    default               : return 0;
    }
}

uint32_t
uint32_of_env_flags(const bitset<it::env_flags_ty> val) {
    using namespace it;
    uint32_t ret = 0;
    if (bitset_is_set(val, env_flags_ty::Enabled)) ret |= ENV_ENABLED;
    if (bitset_is_set(val, env_flags_ty::Loop))    ret |= ENV_LOOP;
    if (bitset_is_set(val, env_flags_ty::Sustain)) ret |= ENV_SUSTAIN;
    return ret;
}

uint32_t
uint32_of_pitch_env_flags(const bitset<it::pitch_env_flags_ty> val) {
    using namespace it;
    uint32_t ret = 0;
    if (bitset_is_set(val, pitch_env_flags_ty::Enabled)) ret |= ENV_ENABLED;
    if (bitset_is_set(val, pitch_env_flags_ty::Loop))    ret |= ENV_LOOP;
    if (bitset_is_set(val, pitch_env_flags_ty::Sustain)) ret |= ENV_SUSTAIN;
    if (bitset_is_set(val, pitch_env_flags_ty::Filter))  ret |= ENV_FILTER;
    return ret;
}

uint16_t
uint16_of_sample_flags(const bitset<it::sample_flags_ty> val) {
    using namespace it;
    uint16_t ret = 0;
    if (bitset_is_set(val, sample_flags_ty::LoopEnabled))     ret |= CHN_LOOP;
    if (bitset_is_set(val, sample_flags_ty::SusLoopEnabled))  ret |= CHN_SUSTAINLOOP;
    if (bitset_is_set(val, sample_flags_ty::PingPongLoop))    ret |= CHN_PINGPONGLOOP;
    if (bitset_is_set(val, sample_flags_ty::PingPongSusLoop)) ret |= CHN_PINGPONGSUSTAIN;
    if (bitset_is_set(val, sample_flags_ty::SixteenBit))      ret |= CHN_16BIT;
    if (bitset_is_set(val, sample_flags_ty::Stereo))          ret |= CHN_STEREO;
    return ret;
}

void
import_inst_plugin(modinstrument_t &ref, const boost::optional<uint8_t> plugin_id) {
    ref.nMixPlug = plugin_id ? plugin_id.get() : 0;
}

void
import_extended_instr(modinstrument_t &ref, const mpt::mpt_instrument_ty &ext) {
}

template<typename Ty, typename ConvTy>
void
import_envelope(modenvelope_t &ref, const it::envelope_ty<Ty> &env, ConvTy flagconv) {
    ref.loop_end = env.loop_end;
    ref.loop_start = env.loop_begin;
    ref.sustain_end = env.sustain_loop_end;
    ref.sustain_start = env.sustain_loop_begin;
    ref.num_nodes = env.num_points;
    ref.flags = flagconv(env.flags);
    for (size_t i = 0; i < env.points.size(); ++i) {
        std::tie(ref.Ticks[i], ref.Values[i]) = env.points[i];
    }
    ref.release_node = ENV_RELEASE_NODE_UNSET;
}

void
import_note_map(uint8_t *notemap, uint16_t *keyboard, const it::instrument_ty &instr) {
    for (const auto &item : instr.note_sample_map) {
        uint8_t sample;
        uint16_t note;
        std::tie(note, sample) = item;
        *notemap = note < 120 ? note + 1 : note;
        *keyboard = sample;
        ++keyboard;
        ++notemap;
    }
}

void
import_instrument(modinstrument_t &ref, const it::instrument_ty &instr) {
    memcpy(ref.name, instr.name.data(), instr.name.size());
    memcpy(ref.legacy_filename, instr.filename.data(), instr.filename.size());

    ref.new_note_action       = uint8_of_nna(instr.new_note_action);
    ref.duplicate_check_type  = uint8_of_dct(instr.duplicate_check_type);
    ref.duplicate_note_action = uint8_of_dca(instr.duplicate_check_action);

    ref.fadeout              = instr.fadeout;
    ref.pitch_pan_separation = instr.pitch_pan_separation;
    ref.pitch_pan_center     = instr.pitch_pan_center;
    ref.global_volume        = instr.global_volume;
    ref.default_pan          = instr.default_pan;

    ref.random_volume_weight = instr.random_volume_variation;
    ref.random_pan_weight = instr.random_panning_variation;
    ref.default_filter_cutoff = instr.default_filter_cutoff;
    ref.default_filter_resonance = instr.default_filter_resonance;

    ref.midi_channel = instr.midi_channel;
    ref.midi_program = instr.midi_program;
    ref.midi_bank = instr.midi_bank;

    import_envelope(ref.volume_envelope, instr.volume_envelope, uint32_of_env_flags);
    import_envelope(ref.panning_envelope, instr.pan_envelope, uint32_of_env_flags);
    import_envelope(ref.pitch_envelope, instr.pitch_envelope, uint32_of_pitch_env_flags);

    import_note_map(ref.NoteMap, ref.Keyboard, instr);
}

void
import_sample_data(modsample_t &ref, const it::sample_ty &sample) {
    using namespace it;
    auto bytelength = sample.num_frames;
    const bool is_sixteen = bitset_is_set(sample.flags, sample_flags_ty::SixteenBit);
    const bool is_stereo  = bitset_is_set(sample.flags, sample_flags_ty::Stereo);
    const bool is_signed  = bitset_is_set(sample.convert_flags, convert_flags_ty::SignedSamples);
    bytelength *= is_sixteen ? 2 : 1;
    bytelength *= is_stereo ? 2 : 1;
    ref.sample_data.generic = modplug::legacy_soundlib::alloc_sample(bytelength);
    if (is_sixteen) {
        // FIXME
        if (is_signed) {
            if (is_stereo) {
                samplecodec::copy_non_interleaved
                    < samplecodec::signed_bytes_ty, std::identity, 2 >
                    (ref.sample_data.int16, sample.data, sample.num_frames);
            } else {
                samplecodec::copy_non_interleaved
                    < samplecodec::signed_bytes_ty, std::identity, 1 >
                    (ref.sample_data.int16, sample.data, sample.num_frames);
            }
        } else {
            if (is_stereo) {
                samplecodec::copy_non_interleaved
                    < samplecodec::unsigned_bytes_ty, std::identity, 2 >
                    (ref.sample_data.int16, sample.data, sample.num_frames);
            } else {
                samplecodec::copy_non_interleaved
                    < samplecodec::unsigned_bytes_ty, std::identity, 1 >
                    (ref.sample_data.int16, sample.data, sample.num_frames);
            }
        }
    } else {
        if (is_signed) {
            if (is_stereo) {
                samplecodec::copy_non_interleaved
                    < samplecodec::signed_bytes_ty, std::identity, 2 >
                    (ref.sample_data.int8, sample.data, sample.num_frames);
            } else {
                samplecodec::copy_non_interleaved
                    < samplecodec::signed_bytes_ty, std::identity, 1 >
                    (ref.sample_data.int8, sample.data, sample.num_frames);
            }
        } else {
            if (is_stereo) {
                samplecodec::copy_non_interleaved
                    < samplecodec::unsigned_bytes_ty, std::identity, 2 >
                    (ref.sample_data.int8, sample.data, sample.num_frames);
            } else {
                samplecodec::copy_non_interleaved
                    < samplecodec::unsigned_bytes_ty, std::identity, 1 >
                    (ref.sample_data.int8, sample.data, sample.num_frames);
            }
        }
    }
}

void
import_sample(modsample_t &ref, const it::sample_ty &sample) {
    memcpy(ref.legacy_filename, sample.filename.data(), sample.filename.size());
    memcpy(ref.name, sample.name.data(), sample.name.size());
    ref.global_volume  = sample.global_volume;
    ref.default_volume = sample.default_volume;
    ref.default_pan    = sample.default_pan;
    ref.loop_start     = sample.loop_begin_frame;
    ref.loop_end       = sample.loop_end_frame;
    ref.c5_samplerate  = sample.c5_freq;
    ref.sustain_start  = sample.sus_loop_begin_frame;
    ref.sustain_end    = sample.sus_loop_end_frame;
    ref.vibrato_sweep  = sample.vibrato_speed;
    ref.vibrato_depth  = sample.vibrato_depth;
    ref.vibrato_rate   = sample.vibrato_rate;
    ref.vibrato_type   = 0; //sample.vibrato_waveform;
    ref.flags          = uint16_of_sample_flags(sample.flags);
    auto bytelength = sample.num_frames;
    if (bitset_is_set(sample.flags, it::sample_flags_ty::HasSampleData)) {
        ref.length = sample.num_frames;
        import_sample_data(ref, sample);
    } else {
        ref.length = 0;
        ref.sample_data.generic = nullptr;
    }
}

uint16_t
pattern_calc_used_channels(const it::pattern_ty &pattern) {
    using namespace std::placeholders;
    using namespace it;
    uint16_t ret = 0;
    auto is_empty = std::bind(operator ==, EmptyEntry, _1);
    for (auto &row : pattern) {
        const auto last = std::find_if_not(row.rbegin(), row.rend(), is_empty);
        if (last != row.rend()) {
            const auto base = last.base() - 1;
            const auto size = static_cast<uint16_t>(1 + clamp<ptrdiff_t>(
                base - row.begin(), 0, UINT16_MAX - 1
            ));
            ret = std::max(ret, size);
        }
    }
    return ret;
}

uint16_t
calc_used_channels(const std::vector<it::pattern_ty> &patterns) {
    auto f = [] (const uint16_t x, const it::pattern_ty &pattern) {
        return std::max(x, pattern_calc_used_channels(pattern));
    };
    return std::accumulate(patterns.cbegin(), patterns.cend(), 0, f);
}

void
import_pattern(CPattern &ref, const it::pattern_ty &pattern, const size_t cols) {
    rowindex_t row_idx = 0;
    auto getrow = [&ref, &row_idx] () {
        auto ret = ref.GetRow(row_idx);
        ++row_idx;
        return ret;
    };
    for (auto &row : pattern) {
        auto cur = row.begin();
        auto outrow = getrow();
        replicate_(cols, [&] () {
            auto &evt = *outrow;
            auto &source = *cur;
            evt.note = match(source.note,
                [] (uint8_t note) { return note < 0x80 ? note + 1 : note; },
                [] () { return 0; });
            evt.instr = source.instr;
            std::tie(evt.volcmd, evt.vol) =
                vol_of_it_vol(source.volcmd, source.volcmd_parameter);
            std::tie(evt.command, evt.param) =
                cmd_of_it_cmd(source.effect_type, source.effect_parameter);
            ++cur;
            ++outrow;
        });
    }
}

void
import_mpt_extensions(module_renderer &song, const mpt::mpt_song_ty &ext) {
    if (ext.field_TempoMode) {
        uint8_t fieldval = 0;
        switch (ext.field_TempoMode.get()) {
        case mpt::tempo_mode_ty::Alternative: fieldval = 1; break;
        case mpt::tempo_mode_ty::Modern: fieldval = 2; break;
        }
        song.m_nTempoMode = fieldval;
    }
}

}

template <typename ValTy>
boost::optional<const ValTy &>
lookup(const std::vector<ValTy> &col, const size_t key) {
    return key < col.size() ? some<const ValTy &>(col[key]) : boost::none;
}

bool
read(module_renderer &song, binaryparse::context &ctx) {
    using namespace std::placeholders;
    using namespace internal;
    using std::bind;
    try {
        const auto it_song_ptr = it::read(ctx);
        const auto &it_song    = *it_song_ptr;
        const auto &header     = it_song.header;
        assign_without_padding(
            song.song_name,
            reinterpret_cast<const char *>(header.song_name.data()),
            header.song_name.size()
        );
        song.m_nInstruments  = header.instruments;
        song.m_nSamples      = header.samples;
        song.m_nDefaultSpeed = header.initial_speed;
        song.m_nDefaultTempo = header.initial_tempo;
        song.m_nType         = MOD_TYPE_IT;
        song.m_nMinPeriod    = 8;
        song.m_nMaxPeriod    = 0xF000;
        song.m_nChannels     = calc_used_channels(it_song.patterns);
        song.SetModFlags(0);
        song.Order.ReadAsByte(
            header.orderlist.data(),
            header.orderlist.size(),
            header.orderlist.size()
        );

        auto alloc_instr   = mk_alloc_instr(song);
        auto alloc_sample  = mk_alloc_sample(song);
        auto alloc_pattern = mk_alloc_pattern(song);

        for (size_t i = 0; i < it_song.instruments.size(); ++i) {
            auto &ref = alloc_instr();
            const auto &instr   = it_song.instruments[i];
            const auto extended = lookup(it_song.mpt_extensions.instruments, i);
            const auto plugin   = lookup(it_song.mpt_extensions.instr_plugnum, i);
            import_instrument(ref, instr);
            match(extended, bind(import_extended_instr, ref, _1), noop);
            match(plugin, bind(import_inst_plugin, ref, _1), noop);
        }

        for (const auto &sample : it_song.samples) {
            auto &ref = alloc_sample();
            import_sample(ref, sample);
        }

        for (const auto &pattern : it_song.patterns) {
            const auto sizes = pattern.shape();
            auto &ref = alloc_pattern(sizes[0]);
            import_pattern(ref, pattern, song.m_nChannels);
        }

        match(it_song.midi_cfg, [&] (const it::midi_cfg_ty &cfg) { }, noop);

        import_mpt_extensions(song, it_song.mpt_extensions.song);
        return true;
    } catch (...) {
        return false;
    }
}

}
}
}
