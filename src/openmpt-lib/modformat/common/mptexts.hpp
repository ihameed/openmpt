#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "boost/optional.hpp"

namespace modplug {

namespace pervasives { namespace binaryparse { struct context; } }

namespace modformat {
namespace mptexts {

const size_t MaxEnvPoints  = 240;
const size_t SampleMapSize = 128;
const size_t NameSize      = 32;
const size_t FilenameSize  = 12;

struct mpt_chan_ty {
    uint8_t volume;
    uint8_t pan;
};

enum struct tempo_mode_ty : uint8_t {
    Classic,
    Alternative,
    Modern
};

typedef std::array<uint16_t, MaxEnvPoints> envtick_ty;
typedef std::array<uint8_t, MaxEnvPoints> envnode_ty;

typedef std::array<uint8_t, SampleMapSize> notemap_ty;
typedef std::array<uint16_t, SampleMapSize> keymap_ty;

typedef std::array<uint8_t, NameSize> name_ty;
typedef std::array<uint8_t, FilenameSize> fname_ty;

typedef std::vector<mpt_chan_ty> ext_chan_ty;

#define MODPLUG_FIELD_TAGS(expand) \
    expand('FO..', uint32_t  , FadeOut) \
    expand('dF..', uint32_t  , ExtensionFlags) \
    expand('GV..', uint32_t  , GlobalVolume) \
    expand('P...', uint32_t  , DefaultPan) \
    expand('VE..', uint32_t  , NumVolEnvNodes) \
    expand('PE..', uint32_t  , NumPanEnvNodes) \
    expand('PiE.', uint32_t  , NumPitchEnvNodes) \
\
    expand('VLS.', uint8_t   , VolEnvLoopStart) \
    expand('VLE.', uint8_t   , VolEnvLoopEnd) \
    expand('VSB.', uint8_t   , VolEnvSustainStart) \
    expand('VSE.', uint8_t   , VolEnvSustainEnd) \
\
    expand('PLS.', uint8_t   , PanEnvLoopStart) \
    expand('PLE.', uint8_t   , PanEnvLoopEnd) \
    expand('PSB.', uint8_t   , PanEnvSustainStart) \
    expand('PSE.', uint8_t   , PanEnvSustainEnd) \
\
    expand('PiLS', uint8_t   , PitchEnvLoopStart) \
    expand('PiLE', uint8_t   , PitchEnvLoopEnd) \
    expand('PiSB', uint8_t   , PitchEnvSustainStart) \
    expand('PiSE', uint8_t   , PitchEnvSustainEnd) \
\
    expand('NNA.', uint8_t   , NewNoteAction) \
    expand('DCT.', uint8_t   , DuplicateCheckType) \
    expand('DNA.', uint8_t   , DuplicateNoteAction) \
    expand('PS..', uint8_t   , RandomPanWeight) \
    expand('VS..', uint8_t   , RandomVolumeWeight) \
    expand('IFC.', uint8_t   , DefaultFilterCutoff) \
    expand('IFR.', uint8_t   , DefaultFilterResonance) \
\
    expand('MB..', uint8_t   , MidiBank) \
    expand('MP..', uint8_t   , MidiProgram) \
    expand('MC..', uint8_t   , MidiChannel) \
    expand('MDK.', uint8_t   , MidiDrumSet) \
\
    expand('PPS.', uint8_t   , PitchPanSeparation) \
    expand('PPC.', uint8_t   , PitchPanCenter) \
\
    expand('VP[.', envtick_ty, VolEnvTicks) \
    expand('PP[.', envtick_ty, PanEnvTicks) \
    expand('PiP[', envtick_ty, PitchEnvTicks) \
    expand('VE[.', envtick_ty, VolEnvVals) \
    expand('PE[.', envtick_ty, PanEnvVals) \
    expand('PiE[', envtick_ty, PitchEnvVals) \
\
    expand('NM[.', notemap_ty, NoteMap) \
    expand('K[..', keymap_ty , KeyMap) \
    expand('n[..', name_ty   , Name) \
    expand('fn[.', fname_ty  , Filename) \
\
    expand('MiP.', uint8_t   , MixPlugAssignment) \
    expand('VR..', uint16_t  , VolRampUp) \
    expand('VRD.', uint16_t  , VolRampDown) \
    expand('R...', uint32_t  , ResampleMode) \
\
    expand('CS..', uint8_t   , RandomCutoffWeight) \
    expand('RS..', uint8_t   , RandomResoWeight) \
    expand('FM..', uint8_t   , DefaultFilterMode) \
\
    expand('PTTL', uint16_t  , PitchToTempoLock) \
    expand('PVEH', uint8_t   , PluginVelocityHandling) \
    expand('PVOH', uint8_t   , PluginVolumeHandling) \
\
    expand('PERN', uint8_t   , PitchEnvRelNode) \
    expand('AERN', uint8_t   , PanEnvRelNode) \
    expand('VERN', uint8_t   , VolEnvRelNode) \
\
    expand('PFLG', uint32_t  , PitchEnvFlags) \
    expand('AFLG', uint32_t  , PanEnvFlags) \
    expand('VFLG', uint32_t  , VolEnvFlags)

#define MODPLUG_SONG_FIELD_TAGS(expand) \
    expand('DT..', uint32_t     , DefaultTempo) \
    expand('RPB.', uint32_t     , RowsPerBeat) \
    expand('RPM.', uint32_t     , RowsPerMeasure) \
    expand('C...', uint16_t     , NumChannels) \
    expand('TM..', tempo_mode_ty, TempoMode) \
    expand('PMM.', uint8_t      , MixLevels) \
    expand('CWV.', uint32_t     , VersionCreatedWith) \
    expand('LSWV', uint32_t     , VersionLastSavedWith) \
    expand('SPA.', uint32_t     , SamplePreAmp) \
    expand('VSTV', uint32_t     , VstiVolume) \
    expand('DGV.', uint32_t     , DefaultGlobalVolume) \
    expand('RP..', uint16_t     , RestartPos) \
    expand('MSF.', uint16_t     , ModFlags) \
    expand('MIMA', nullptr_t    , MidiMapper) \
    expand('ChnS', ext_chan_ty  , ExtraChannelInfo)

#define MODPLUG_EXPAND_OPTIONAL_FIELD(bytes, datatype, tycon) \
    boost::optional< datatype > field_##tycon;

#define MODPLUG_EXPAND_TAG_AS_ENUM(bytes, datatype, tycon) tycon,

enum struct instr_tag_ty {
MODPLUG_FIELD_TAGS(MODPLUG_EXPAND_TAG_AS_ENUM)
};

enum struct song_tag_ty {
MODPLUG_SONG_FIELD_TAGS(MODPLUG_EXPAND_TAG_AS_ENUM)
};

struct mpt_instrument_ty {
MODPLUG_FIELD_TAGS(MODPLUG_EXPAND_OPTIONAL_FIELD)
};

struct mpt_song_ty {
MODPLUG_SONG_FIELD_TAGS(MODPLUG_EXPAND_OPTIONAL_FIELD)
};

boost::optional<instr_tag_ty>
instr_tag_of_bytes(const uint32_t);

uint32_t
bytes_of_instr_tag(const instr_tag_ty);

boost::optional<song_tag_ty>
song_tag_of_bytes(const uint32_t);

uint32_t
bytes_of_song_tag(const song_tag_ty);

nullptr_t
inject_extended_instr_data(
    modplug::pervasives::binaryparse::context &,
    const instr_tag_ty, const uint16_t, mpt_instrument_ty &);

nullptr_t
inject_extended_song_data(
    modplug::pervasives::binaryparse::context &,
    const song_tag_ty, const uint16_t, mpt_song_ty &);

}
}
}
