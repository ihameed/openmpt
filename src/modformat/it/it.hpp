#pragma once

#include <array>
#include <unordered_map>

#include "boost/multi_array.hpp"

#include "../pervasives/bitset.h"
#include "../pervasives/type.h"

#include "../internal.h"
#include "../common/mptexts.h"

namespace modplug {

namespace pervasives { namespace binaryparse { struct context; } }

namespace modformat {
namespace it {

enum struct flags_ty : uint8_t {
    Stereo                 = 1 << 0,
    Vol0MixOptimizations   = 1 << 1,
    UseInstruments         = 1 << 2,
    UseLinearSlides        = 1 << 3,
    UseOldEffects          = 1 << 4,
    LinkPortaWithSlide     = 1 << 5,
    UseMidiPitchController = 1 << 6,
    RequestEmbeddedMidiCfg = 1 << 7
};

enum struct special_flags_ty : uint8_t {
    SongMessageEmbedded       = 1 << 0,
    MidiConfigurationEmbedded = 1 << 1,
    EditHistoryEmbedded       = 1 << 2,
    RowHighlightEmbedded      = 1 << 3
};

enum struct nna_ty : uint8_t {
    Cut,
    Continue,
    NoteOff,
    NoteFade
};

enum struct dct_ty : uint8_t {
    Off,
    Note,
    Sample,
    Instrument
};

enum struct dca_ty : uint8_t {
    Cut,
    NoteOff,
    NoteFade
};

struct offsets_ty {
    std::vector<uint32_t> instruments;
    std::vector<uint32_t> samples;
    std::vector<uint32_t> patterns;
    uint32_t message;
    uint16_t message_length;
};

struct edit_history_ty {
    int16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    double open_time_seconds;
};

struct file_header_ty {
    typedef modplug::pervasives::bitset<flags_ty> flag_set;
    typedef modplug::pervasives::bitset<special_flags_ty> special_set;
    std::array<uint8_t, 26> song_name;
    uint8_t row_highlight_minor;
    uint8_t row_highlight_major;
    uint16_t orders;
    uint16_t instruments;
    uint16_t samples;
    uint16_t patterns;
    uint16_t version_created_with;
    uint16_t minimum_compatible_version;
    flag_set flags;
    special_set special;
    uint8_t global_volume;
    uint8_t mix_volume;
    uint8_t initial_speed;
    uint8_t initial_tempo;
    uint8_t panning_separation;
    uint8_t pitch_wheel_depth;
    std::array<uint8_t, 64> channel_pan;
    std::array<uint8_t, 64> channel_vol;
    std::vector<uint8_t> orderlist;
    uint32_t reserved;
    std::vector<edit_history_ty> edit_history;
    offsets_ty offsets;
};

enum struct env_flags_ty : uint8_t {
    Enabled = 1 << 0,
    Loop    = 1 << 1,
    Sustain = 1 << 2,
    Carry   = 1 << 3
};

enum struct pitch_env_flags_ty : uint8_t {
    Enabled = 1 << 0,
    Loop    = 1 << 1,
    Sustain = 1 << 2,
    Carry   = 1 << 3,
    Filter  = 1 << 4
};

template <typename FlagsTy>
struct envelope_ty {
    FlagsTy flags;
    uint8_t num_points;
    uint8_t loop_begin;
    uint8_t loop_end;
    uint8_t sustain_loop_begin;
    uint8_t sustain_loop_end;
    std::array<std::tuple<uint16_t, uint8_t>, 25> points;
};

struct instrument_ty {
    typedef modplug::pervasives::bitset<env_flags_ty> env_set;
    typedef modplug::pervasives::bitset<pitch_env_flags_ty> pitch_set;
    std::array<uint8_t, 12> filename;
    nna_ty new_note_action;
    dct_ty duplicate_check_type;
    dca_ty duplicate_check_action;
    uint16_t fadeout;
    int8_t pitch_pan_separation;
    uint8_t pitch_pan_center;
    uint8_t global_volume;
    uint8_t default_pan;
    uint8_t random_volume_variation;
    uint8_t random_panning_variation;
    uint16_t version_created_with;
    uint8_t num_associated_samples;
    std::array<uint8_t, 26> name;
    uint8_t default_filter_cutoff;
    uint8_t default_filter_resonance;
    uint8_t midi_channel;
    uint8_t midi_program;
    uint16_t midi_bank;
    std::array<std::tuple<uint8_t, uint8_t>, 120> note_sample_map;
    envelope_ty<env_set> volume_envelope;
    envelope_ty<env_set> pan_envelope;
    envelope_ty<pitch_set> pitch_envelope;
};

enum struct sample_flags_ty : uint8_t {
    HasSampleData   = 1 << 0,
    SixteenBit      = 1 << 1,
    Stereo          = 1 << 2,
    Compressed      = 1 << 3,
    LoopEnabled     = 1 << 4,
    SusLoopEnabled  = 1 << 5,
    PingPongLoop    = 1 << 6,
    PingPongSusLoop = 1 << 7,
};

enum struct convert_flags_ty : uint8_t {
    SignedSamples         = 1 << 0,
    BigEndian             = 1 << 1,
    DeltaEncoded          = 1 << 2,
    ByteDeltaEncoded      = 1 << 3,
    TwelveBit             = 1 << 4,
    LeftRightStereoPrompt = 1 << 5
};

enum struct vibrato_wave_ty : uint8_t {
    Sine,
    RampDown,
    Square,
    Random
};

struct sample_ty {
    typedef modplug::pervasives::bitset<sample_flags_ty> flag_set;
    typedef modplug::pervasives::bitset<convert_flags_ty> conv_set;
    std::array<uint8_t, 12> filename;
    uint8_t global_volume;
    flag_set flags;
    uint8_t default_volume;
    std::array<uint8_t, 26> name;
    conv_set convert_flags;
    uint8_t default_pan;
    uint32_t num_frames;
    uint32_t loop_begin_frame;
    uint32_t loop_end_frame;
    uint32_t c5_freq;
    uint32_t sus_loop_begin_frame;
    uint32_t sus_loop_end_frame;
    uint8_t vibrato_speed;
    uint8_t vibrato_depth;
    vibrato_wave_ty vibrato_waveform;
    uint8_t vibrato_rate;
    const uint8_t *data;
};

#define MODPLUG_EXPAND_AS_ENUM(tycon) tycon,

#define MODPLUG_IT_VOL_TAGS(expand) \
    expand(Nothing) \
    expand(VolSet) \
    expand(PanSet) \
    expand(VolFineUp) \
    expand(VolFineDown) \
    expand(VolSlideUp) \
    expand(VolSlideDown) \
    expand(PortamentoDown) \
    expand(PortamentoUp) \
    expand(Portamento) \
    expand(VibDepth)

#define MODPLUG_IT_CMD_TAGS(expand) \
    expand(Nothing) \
    expand(SetSpeed) \
    expand(JumpToOrder) \
    expand(BreakToRow) \
\
    expand(VolSlideContinue) \
    expand(VolSlideUp) \
    expand(VolSlideDown) \
    expand(FineVolSlideUp) \
    expand(FineVolSlideDown) \
\
    expand(PortamentoDown) \
    expand(FinePortamentoDown) \
    expand(ExtraFinePortamentoDown) \
    expand(PortamentoUp) \
    expand(FinePortamentoUp) \
    expand(ExtraFinePortamentoUp) \
    expand(Portamento) \
\
    expand(Vibrato) \
    expand(Tremor) \
    expand(Arpeggio) \
\
    expand(VolSlideVibratoContinue) \
    expand(VolSlideVibratoUp) \
    expand(VolSlideVibratoDown) \
    expand(FineVolSlideVibratoUp) \
    expand(FineVolSlideVibratoDown) \
\
    expand(VolSlidePortaContinue) \
    expand(VolSlidePortaUp) \
    expand(VolSlidePortaDown) \
    expand(FineVolSlidePortaUp) \
    expand(FineVolSlidePortaDown) \
\
    expand(SetChannelVol) \
    expand(ChannelVolSlideContinue) \
    expand(ChannelVolSlideUp) \
    expand(ChannelVolSlideDown) \
    expand(ChannelFineVolSlideUp) \
    expand(ChannelFineVolSlideDown) \
\
    expand(SetSampleOffset) \
\
    expand(PanSlideContinue) \
    expand(PanSlideLeft) \
    expand(PanSlideRight) \
    expand(FinePanSlideLeft) \
    expand(FinePanSlideRight) \
\
    expand(Retrigger) \
    expand(Tremolo) \
\
    expand(GlissandoControl) \
    expand(VibratoWaveform) \
    expand(TremoloWaveform) \
    expand(PanbrelloWaveform) \
    expand(FinePatternDelay) \
    expand(InstrControl) \
    expand(SetPanShort) \
    expand(SoundControl) \
    expand(SetHighSampleOffset) \
    expand(PatternLoop) \
    expand(NoteCut) \
    expand(NoteDelay) \
    expand(PatternDelay) \
    expand(SetActiveMidiMacro) \
\
    expand(SetTempo) \
    expand(FineVibrato) \
\
    expand(SetGlobalVol) \
    expand(GlobalVolSlideContinue) \
    expand(GlobalVolSlideUp) \
    expand(GlobalVolSlideDown) \
    expand(GlobalFineVolSlideUp) \
    expand(GlobalFineVolSlideDown) \
\
    expand(SetPan) \
    expand(Panbrello) \
    expand(MidiMacro) \
    expand(SmoothMidiMacro) \
    expand(DelayCut) \
    expand(ExtendedParameter)

enum struct vol_ty : uint8_t {
    MODPLUG_IT_VOL_TAGS(MODPLUG_EXPAND_AS_ENUM)
};

enum struct cmd_ty : uint8_t {
    MODPLUG_IT_CMD_TAGS(MODPLUG_EXPAND_AS_ENUM)
};

const std::tuple<vol_ty, uint8_t> EmptyVol =
    std::make_tuple(vol_ty::Nothing, 0);

const std::tuple<cmd_ty, uint8_t> EmptyEffect =
    std::make_tuple(cmd_ty::Nothing, 0);

const uint8_t ReadNote     = 1 << 0;
const uint8_t ReadInstr    = 1 << 1;
const uint8_t ReadVol      = 1 << 2;
const uint8_t ReadCmd      = 1 << 3;
const uint8_t UseLastNote  = 1 << 4;
const uint8_t UseLastInstr = 1 << 5;
const uint8_t UseLastVol   = 1 << 6;
const uint8_t UseLastCmd   = 1 << 7;

struct pattern_entry_ty {
    boost::optional<uint8_t> note;
    uint8_t instr;
    vol_ty volcmd;
    uint8_t volcmd_parameter;
    cmd_ty effect_type;
    uint8_t effect_parameter;

    const static uint8_t NoteOff  = 255;
    const static uint8_t NoteCut  = 254;
    const static uint8_t NoteFade = 253;
    const static uint8_t MaxNote  = 119;
    const static uint8_t MinNote  = 0;

    pattern_entry_ty()
        : note(), instr(0)
        , volcmd(vol_ty::Nothing), volcmd_parameter(0)
        , effect_type(cmd_ty::Nothing), effect_parameter(0)
        { };
};

const pattern_entry_ty EmptyEntry;

inline bool
operator == (const pattern_entry_ty &x, const pattern_entry_ty &y) {
    return x.note == y.note
        && x.instr == y.instr
        && x.volcmd == y.volcmd
        && x.volcmd_parameter == y.volcmd_parameter
        && x.effect_type == y.effect_type
        && x.effect_parameter == y.effect_parameter;
}

typedef boost::multi_array<pattern_entry_ty, 2> pattern_ty;

struct midi_cfg_ty {
    typedef std::array<uint8_t, 32> macro_arr_ty;
    std::array<macro_arr_ty, 9> global;
    std::array<macro_arr_ty, 16> parameterized;
    std::array<macro_arr_ty, 128> fixed;
};

static_assert(
    sizeof(midi_cfg_ty) == 4896, 
    "MIDI configuration data in IT files must be 4896 bytes long; the static "
    "array parser depends on the static size of the array");

struct plugin_info_ty {
    uint32_t plugin_id_1;
    uint32_t plugin_id_2;
    uint32_t input_routing;
    uint32_t output_routing;
    std::array<uint32_t, 4> reserved;
    std::array<uint8_t, 32> name;
    std::array<uint8_t, 64> dll_file_name;

    plugin_info_ty()
        : plugin_id_1(0), plugin_id_2(0), input_routing(0), output_routing(0)
        { }
};

struct plugin_ty {
    float dry_ratio;
    uint32_t default_program;
    uint32_t data_size;
    std::shared_ptr<uint8_t> data;
    plugin_info_ty info;

    plugin_ty()
        : dry_ratio(1.0f), default_program(0), data_size(0), data(nullptr)
        { }
};

static_assert(
    sizeof(size_t) >= sizeof(uint32_t),
    "opaque plugin data may be up to 2^32 bytes long");

struct mix_info_ty {
    std::vector<uint8_t> channel_mix_plugins;
    std::unordered_map<uint8_t, plugin_ty> plugin_data;
};

const size_t MaxChannelSize = 127;

const size_t ChannelNameSize = 20;

static_assert(MaxChannelSize < 0x80, "MaxChannelSize must not exceed 0x80");

typedef std::array<uint8_t, ChannelNameSize> channel_name_ty;

struct mpt_extensions_ty {
    std::vector<modplug::modformat::mptexts::mpt_instrument_ty> instruments;
    std::vector<boost::optional<uint8_t>> instr_plugnum;
    modplug::modformat::mptexts::mpt_song_ty song;
};

struct song {
    file_header_ty header;
    std::vector<instrument_ty> instruments;
    std::vector<sample_ty> samples;
    std::vector<pattern_ty> patterns;
    boost::optional<midi_cfg_ty> midi_cfg;
    std::shared_ptr<mix_info_ty> mix_info;
    std::vector<channel_name_ty> channel_names;
    std::vector<edit_history_ty> edit_history;
    std::string comments;
    mpt_extensions_ty mpt_extensions;
};

std::shared_ptr<song>
read(modplug::pervasives::binaryparse::context &ctx);

}
}
}

namespace std {
std::string
to_string(const modplug::modformat::it::vol_ty);

std::string
to_string(const modplug::modformat::it::cmd_ty);

std::string
to_string(const modplug::modformat::it::pattern_entry_ty &);

std::string
to_string(const modplug::modformat::it::pattern_ty &);
}
