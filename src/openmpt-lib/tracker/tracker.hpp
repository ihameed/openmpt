#pragma once

#include <cstdint>

#include "modevent.hpp"
#include "types.hpp"
#include "sample.hpp"

#include "../pervasives/bitset.hpp"

#include "../legacy_soundlib/Snd_defs.h"
#include "../mixgraph/constants.hpp"

class CTuningBase;
typedef CTuningBase CTuning;

namespace modplug {
namespace tracker {

enum struct sflag_ty : uint32_t {
    SixteenBit      = 1u << 0,
    Loop            = 1u << 1,
    BidiLoop        = 1u << 2,
    SustainLoop     = 1u << 3,
    BidiSustainLoop = 1u << 4,
    ForcedPanning   = 1u << 5,
    Stereo          = 1u << 6
};

enum struct vflag_ty : uint32_t {
    SixteenBit           = 1u << 0,
    Loop                 = 1u << 1,
    BidiLoop             = 1u << 2,
    SustainLoop          = 1u << 3,
    BidiSustainLoop      = 1u << 4,
    Stereo               = 1u << 5,
    ScrubBackwards       = 1u << 6,
    Mute                 = 1u << 7,
    KeyOff               = 1u << 8,
    NoteFade             = 1u << 9,
    Surround             = 1u << 10,
    DisableInterpolation = 1u << 11,
    AlreadyLooping       = 1u << 12,
    Filter               = 1u << 13,
    VolumeRamp           = 1u << 14,
    Vibrato              = 1u << 15,
    Tremolo              = 1u << 16,
    Panbrello            = 1u << 17,
    Portamento           = 1u << 18,
    Glissando            = 1u << 19,
    VolEnvOn             = 1u << 20,
    PanEnvOn             = 1u << 21,
    PitchEnvOn           = 1u << 22,
    FastVolRamp          = 1u << 23,
    ExtraLoud            = 1u << 24,
    Solo                 = 1u << 27,
    DisableDsp           = 1u << 28,
    SyncMute             = 1u << 29,
    PitchEnvAsFilter     = 1u << 30,
    ForcedPanning        = 1u << 31
};

typedef pervasives::bitset<vflag_ty> vflags_ty;

modplug::pervasives::bitset<vflag_ty> __forceinline
voicef_from_legacy(const uint32_t old) {
    using namespace modplug::pervasives;
    bitset<vflag_ty> ret;
    const auto set = [&] (const uint32_t sf, const vflag_ty vf) {
        if (old & sf) bitset_add(ret, vf);
    };
    set(CHN_16BIT, vflag_ty::SixteenBit);
    set(CHN_LOOP, vflag_ty::Loop);
    set(CHN_PINGPONGLOOP, vflag_ty::BidiLoop);
    set(CHN_SUSTAINLOOP, vflag_ty::SustainLoop);
    set(CHN_PINGPONGSUSTAIN, vflag_ty::BidiSustainLoop);
    set(CHN_PANNING, vflag_ty::ForcedPanning);
    set(CHN_STEREO, vflag_ty::Stereo);
    return ret;
}

modplug::pervasives::bitset<vflag_ty> __forceinline
voicef_from_smpf(const modplug::pervasives::bitset<sflag_ty> smpflags) {
    using namespace modplug::pervasives;
    bitset<vflag_ty> ret;
    const auto set = [&] (const sflag_ty sf, const vflag_ty vf) {
        if (bitset_is_set(smpflags, sf)) bitset_add(ret, vf);
    };
    set(sflag_ty::SixteenBit, vflag_ty::SixteenBit);
    set(sflag_ty::Loop, vflag_ty::Loop);
    set(sflag_ty::BidiLoop, vflag_ty::BidiLoop);
    set(sflag_ty::SustainLoop, vflag_ty::SustainLoop);
    set(sflag_ty::BidiSustainLoop, vflag_ty::BidiSustainLoop);
    set(sflag_ty::ForcedPanning, vflag_ty::ForcedPanning);
    set(sflag_ty::Stereo, vflag_ty::Stereo);
    return ret;
}

modplug::pervasives::bitset<vflag_ty> __forceinline
voicef_unset_smpf(const modplug::pervasives::bitset<vflag_ty> vf) {
    auto ret = bitset_unset(vf, vflag_ty::SixteenBit);
    bitset_remove(ret, vflag_ty::SixteenBit);
    bitset_remove(ret, vflag_ty::Loop);
    bitset_remove(ret, vflag_ty::BidiLoop);
    bitset_remove(ret, vflag_ty::SustainLoop);
    bitset_remove(ret, vflag_ty::BidiSustainLoop);
    bitset_remove(ret, vflag_ty::ForcedPanning);
    bitset_remove(ret, vflag_ty::Stereo);
    return ret;
}

modplug::pervasives::bitset<vflag_ty> __forceinline
voicef_from_oldchn(const uint32_t old) {
    auto ret = voicef_from_legacy(old);
    const auto set = [&] (const uint32_t sf, const vflag_ty vf) {
        if (old & sf) bitset_add(ret, vf);
    };
    set(CHN_PINGPONGFLAG, vflag_ty::ScrubBackwards);
    set(CHN_MUTE, vflag_ty::Mute);
    set(CHN_KEYOFF, vflag_ty::KeyOff);
    set(CHN_NOTEFADE, vflag_ty::NoteFade);
    set(CHN_SURROUND, vflag_ty::Surround);
    set(CHN_NOIDO, vflag_ty::DisableInterpolation);
    set(CHN_FILTER, vflag_ty::Filter);
    set(CHN_VOLUMERAMP, vflag_ty::VolumeRamp);
    set(CHN_VIBRATO, vflag_ty::Vibrato);
    set(CHN_TREMOLO, vflag_ty::Tremolo);
    set(CHN_PANBRELLO, vflag_ty::Panbrello);
    set(CHN_PORTAMENTO, vflag_ty::Portamento);
    set(CHN_GLISSANDO, vflag_ty::Glissando);
    set(CHN_VOLENV, vflag_ty::VolEnvOn);
    set(CHN_PANENV, vflag_ty::PanEnvOn);
    set(CHN_PITCHENV, vflag_ty::PitchEnvOn);
    set(CHN_FASTVOLRAMP, vflag_ty::FastVolRamp);
    set(CHN_EXTRALOUD, vflag_ty::ExtraLoud);
    set(CHN_SOLO, vflag_ty::Solo);
    set(CHN_NOFX, vflag_ty::DisableDsp);
    set(CHN_SYNCMUTE, vflag_ty::SyncMute);
    set(CHN_FILTERENV, vflag_ty::PitchEnvAsFilter);
    return ret;
}

typedef int32_t fixedpt_t;

//struct modsample_t;

struct resampler_config_t {
    //rewbs.resamplerConf
    long ramp_in_length;
    long ramp_out_length;
    double fir_cutoff;
    uint8_t fir_type; //WFIR_KAISER4T;
    //end rewbs.resamplerConf
};

struct modenvelope_t {
    uint32_t flags;

    uint16_t Ticks[MAX_ENVPOINTS]; // envelope point position (x axis)
    uint8_t Values[MAX_ENVPOINTS]; // envelope point value (y axis)

    uint32_t num_nodes;

    uint8_t loop_start;
    uint8_t loop_end;
    uint8_t sustain_start;
    uint8_t sustain_end;

    uint8_t release_node;
};


struct modinstrument_t {
    uint32_t fadeout;
    uint32_t flags;
    uint32_t global_volume;
    uint32_t default_pan;

    modenvelope_t volume_envelope;
    modenvelope_t panning_envelope;
    modenvelope_t pitch_envelope;

    uint8_t NoteMap[128];    // Note mapping, f.e. C-5 => D-5.
    uint16_t Keyboard[128];  // Sample mapping, f.e. C-5 => Sample 1

    uint8_t new_note_action;
    uint8_t duplicate_check_type;
    uint8_t duplicate_note_action;

    uint8_t random_pan_weight;
    uint8_t random_volume_weight;

    uint8_t default_filter_cutoff;
    uint8_t default_filter_resonance;

    uint16_t midi_bank;
    uint8_t midi_program;
    uint8_t midi_channel;
    uint8_t midi_drum_set;

    signed char pitch_pan_separation;
    unsigned char pitch_pan_center;

    char name[32];            // Note: not guaranteed to be null-terminated.
    char legacy_filename[32];

    PLUGINDEX nMixPlug;                            // Plugin assigned to this instrument

    modplug::mixgraph::id_t graph_insert;

    uint16_t volume_ramp_up;
    uint16_t volume_ramp_down;

    uint32_t resampling_mode;                            // Resampling mode

    uint8_t random_cutoff_weight;
    uint8_t random_resonance_weight;
    uint8_t default_filter_mode;

    uint16_t pitch_to_tempo_lock;

    uint8_t nPluginVelocityHandling;    // How to deal with plugin velocity
    uint8_t nPluginVolumeHandling;            // How to deal with plugin volume
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// WHEN adding new members here, ALSO update Sndfile.cpp (instructions near the top of this file)!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    CTuning* pTuning;                            // sample tuning assigned to this instrument
    static CTuning* s_DefaultTuning;

    void SetTuning(CTuning* pT)
    {
        pTuning = pT;
    }
};


struct modenvstate_t {
    uint32_t position;
    int32_t release_value;
};

const size_t SpillMax = 256 * 2;

#pragma warning(disable : 4324) //structure was padded due to __declspec(align())

__declspec(align(32)) struct modchannel_t {
    // First 32-bytes: Most used mixing information: don't change it
    // These fields are accessed directly by the MMX mixing code (look out for CHNOFS_PCURRENTSAMPLE), so the order is crucial
    char *active_sample_data;
    sampleoffset_t sample_position;
    fixedpt_t fractional_sample_position; // actually 16-bit
    fixedpt_t position_delta; // 16.16

    int32_t right_volume;
    int32_t left_volume;
    int32_t right_ramp;
    int32_t left_ramp;

    // 2nd cache line
    int32_t length;
    vflags_ty flags;
    sampleoffset_t loop_start;
    sampleoffset_t loop_end;
    int32_t nRampRightVol;
    int32_t nRampLeftVol;

    modplug::mixgraph::sample_t nFilter_Y1, nFilter_Y2, nFilter_Y3, nFilter_Y4;
    modplug::mixgraph::sample_t nFilter_A0, nFilter_B0, nFilter_B1, nFilter_HP;

    int32_t nROfs, nLOfs;
    int32_t nRampLength;
    // Information not used in the mixer
    char *sample_data;
    int32_t nNewRightVol, nNewLeftVol;
    int32_t nRealVolume, nRealPan;
    int32_t nVolume, nPan, nFadeOutVol;
    int32_t nPeriod, nC5Speed, nPortamentoDest;

    modinstrument_t *instrument;
    modenvstate_t volume_envelope, panning_envelope, pitch_envelope;
    modsample_t *sample;

    uint32_t parent_channel, nVUMeter;
    int32_t nGlobalVol, nInsVol;
    int32_t nFineTune, nTranspose;
    int32_t nPortamentoSlide, nAutoVibDepth;
    uint32_t nAutoVibPos, nVibratoPos, nTremoloPos, nPanbrelloPos;
    int32_t nVolSwing, nPanSwing;
    int32_t nCutSwing, nResSwing;
    int32_t nRestorePanOnNewNote; //If > 0, nPan should be set to nRestorePanOnNewNote - 1 on new note. Used to recover from panswing.
    uint32_t nOldGlobalVolSlide;
    uint32_t nEFxOffset; // offset memory for Invert Loop (EFx, .MOD only)
    int nRetrigCount, nRetrigParam;
    rowindex_t nPatternLoop;

    uint8_t nRestoreResonanceOnNewNote; //Like above
    uint8_t nRestoreCutoffOnNewNote; //Like above
    uint8_t nNote, nNNA;
    uint8_t nNewNote, nNewIns, nCommand, nArpeggio;
    uint8_t nOldVolumeSlide, nOldFineVolUpDown;
    uint8_t nOldPortaUpDown, nOldFinePortaUpDown;
    uint8_t nOldPanSlide, nOldChnVolSlide;
    uint32_t nNoteSlideCounter, nNoteSlideSpeed, nNoteSlideStep;
    uint8_t nVibratoType, nVibratoSpeed, nVibratoDepth;
    uint8_t nTremoloType, nTremoloSpeed, nTremoloDepth;
    uint8_t nPanbrelloType, nPanbrelloSpeed, nPanbrelloDepth;
    uint8_t nOldCmdEx, nOldVolParam, nOldTempo;
    uint8_t nOldOffset, nOldHiOffset;
    uint8_t nCutOff, nResonance;
    uint8_t nTremorCount, nTremorParam;
    uint8_t nPatternLoopCount;
    uint8_t nRowNote, nRowInstr;
    uint8_t nRowVolCmd, nRowVolume;
    uint8_t nRowCommand, nRowParam;
    uint8_t nLeftVU, nRightVU;
    uint8_t nActiveMacro, nFilterMode;
    uint8_t nEFxSpeed, nEFxDelay; // memory for Invert Loop (EFx, .MOD only)

    uint16_t m_RowPlugParam;                    //NOTE_PCs memory.
    float m_nPlugParamValueStep;  //rewbs.smoothVST
    float m_nPlugInitialParamValue; //rewbs.smoothVST
    PLUGINDEX m_RowPlug;                    //NOTE_PCs memory.

    bool inactive() { return length == 0; }
    uint32_t muted() {
        using namespace modplug::pervasives;
        return bitset_is_set(flags, vflag_ty::Mute);
    }

    void ClearRowCmd() {
        nRowNote = NoteNone; nRowInstr = 0;
        nRowVolCmd = VolCmdNone; nRowVolume = 0;
        nRowCommand = CmdNone; nRowParam = 0;
    }

    typedef uint32_t VOLUME;
    VOLUME GetVSTVolume() {return (instrument) ? instrument->global_volume*4 : nVolume;}

    //-->Variables used to make user-definable tuning modes work with pattern effects.
        bool m_ReCalculateFreqOnFirstTick;
        //If true, freq should be recalculated in ReadNote() on first tick.
        //Currently used only for vibrato things - using in other context might be
        //problematic.

        bool m_CalculateFreq;
        //To tell whether to calculate frequency.

        int32_t m_PortamentoFineSteps;
        long m_PortamentoTickSlide;

        uint32_t m_Freq;
        float m_VibratoDepth;
    //<----

    typedef modplug::mixgraph::sample_t sample_t;
    sample_t spill_back[SpillMax];
    sample_t spill_fwd[SpillMax];
};

#define CHNRESET_CHNSETTINGS    1 //  1 b
#define CHNRESET_SETPOS_BASIC    2 // 10 b
#define    CHNRESET_SETPOS_FULL        7 //111 b
#define CHNRESET_TOTAL                    255 //11111111b


struct MODCHANNELSETTINGS
{
    uint32_t nPan;
    uint32_t nVolume;
    uint32_t dwFlags;
    PLUGINDEX nMixPlugin;
    char szName[MAX_CHANNELNAME];
};


}
}
