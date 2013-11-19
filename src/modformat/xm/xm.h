#pragma once

#include <array>

#include "boost/multi_array.hpp"

#include "../pervasives/type.h"
#include "../pervasives/bitset.h"

#include "../internal.h"

namespace modplug {

namespace pervasives { namespace binaryparse { struct context; } }

namespace modformat {
namespace xm {

const size_t SizeOfFileHeader = 60;
const size_t SizeOfInstrumentHeader = 260;

const uint8_t MaxNote = 97;
const uint8_t MinNote = 0;

struct file_header_ty {
    std::array<uint8_t, 20> song_name;
    std::array<uint8_t, 20> tracker_name;
    uint16_t version;
    uint32_t size;
    uint16_t orders;
    uint16_t restartpos;
    uint16_t channels;
    uint16_t patterns;
    uint16_t instruments;
    uint16_t flags;
    uint16_t speed;
    uint16_t tempo;
};

struct env_point_ty {
    uint16_t x;
    uint16_t y;
};

enum struct env_flags_ty : uint8_t {
    Enabled = 1 << 0,
    Sustain = 1 << 1,
    Loop    = 1 << 2 
};

enum struct sample_loop_ty : uint8_t {
    NoLoop,
    ForwardLoop,
    BidiLoop
};

enum struct sample_width_ty : uint8_t {
    EightBit,
    SixteenBit
};

enum struct sample_format_ty : uint8_t {
    DeltaEncoded,
    AdpcmEncoded
};

struct sample_header_ty {
    uint32_t sample_frames;
    uint32_t loop_start;
    uint32_t loop_length;
    uint8_t vol;
    int8_t fine_tune;
    sample_loop_ty loop_type;
    sample_width_ty width;
    uint8_t pan;
    int8_t relative_note;
    sample_format_ty sample_format;
    std::array<uint8_t, 22> name;
};

struct sample_ty {
    sample_header_ty header;
    const uint8_t *data;
};

struct env_ty {
    std::array<env_point_ty, 12> nodes;
    uint8_t num_points;
    uint8_t sustain_point;
    uint8_t loop_start_point;
    uint8_t loop_end_point;
    modplug::pervasives::bitset<env_flags_ty> flags;
};

struct instrument_header_ty {
    uint32_t size;
    std::array<uint8_t, 22> name;
    uint8_t type;
    uint16_t num_samples;
    uint32_t sample_header_size;
    std::array<uint8_t, 96> keymap;
    env_ty vol_envelope;
    env_ty pan_envelope;
    uint8_t vibrato_type;
    uint8_t vibrato_sweep;
    uint8_t vibrato_depth;
    uint8_t vibrato_rate;
    uint16_t vol_fadeout;
    uint8_t midi_enabled;
    uint8_t midi_channel;
    uint16_t midi_program;
    uint16_t pitch_wheel_range;
    uint8_t mute_computer;
};

struct instrument_ty {
    instrument_header_ty header;
    std::vector<sample_ty> samples;
};

enum struct vol_ty : uint8_t {
    Nothing,
    VolSet,
    VolSlideDown,
    VolSlideUp,
    VolFineDown,
    VolFineUp,
    VibSpeed,
    VibDepth,
    PanSet,
    PanSlideLeft,
    PanSlideRight,
    Portamento
};

enum struct cmd_ty : uint8_t {
    Nothing,
    Arpeggio,
    PortaUp,
    PortaDown,
    Portamento,
    Vibrato,
    PortamentoVolSlide,
    VibratoVolSlide,
    Tremolo,
    SetPan,
    SampleOffset,
    VolSlide,
    PosJump,
    SetVol,
    PatternBreak,
    FinePortaUp,
    FinePortaDown,
    GlissControl,
    VibratoControl,
    Finetune,
    LoopControl,
    TremoloControl,
    RetriggerNote,
    FineVolSlideUp,
    FineVolSlideDown,
    NoteCut,
    NoteDelay,
    PatternDelay,
    SetTicksTempo,
    SetGlobalVolume,
    GlobalVolumeSlide,
    SetEnvelopePosition,
    PanningSlide,
    MultiRetrigNote,
    Tremor,
    ExtraFinePortaUp,
    ExtraFinePortaDown
};

const std::tuple<vol_ty, uint8_t> EmptyVol =
    std::make_tuple(vol_ty::Nothing, 0);
const std::tuple<cmd_ty, uint8_t> EmptyFx =
    std::make_tuple(cmd_ty::Nothing, 0);

const uint8_t IsFlag             = 0x80;
const uint8_t NotePresent        = 0x1;
const uint8_t InstrumentPresent  = 0x2;
const uint8_t VolCmdPresent      = 0x4;
const uint8_t EffectTypePresent  = 0x8;
const uint8_t EffectParamPresent = 0x10;
const uint8_t AllPresent         = 0xff;

struct pattern_entry_ty {
    uint8_t note;
    uint8_t instrument;
    vol_ty volcmd;
    uint8_t volcmd_parameter;
    cmd_ty effect_type;
    uint8_t effect_parameter;
};

struct pattern_header_ty {
    uint32_t size;
    uint8_t packing_type;
    uint16_t num_rows;
    uint16_t data_size;
};

struct pattern_ty {
    pattern_header_ty header;
    boost::multi_array<pattern_entry_ty, 2> data;
};

struct song {
    file_header_ty header;
    std::vector<uint8_t> orders;
    std::vector<pattern_ty> patterns;
    std::vector<instrument_ty> instruments;
};

std::shared_ptr<song>
read(modplug::pervasives::binaryparse::context &ctx);

}
}
}
