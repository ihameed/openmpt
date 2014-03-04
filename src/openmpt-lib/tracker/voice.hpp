#pragma once

#include "sample.hpp"
#include "../mixgraph/constants.hpp"

namespace modplug {
namespace tracker {

enum struct vflag_ty : uint32_t {
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

typedef modplug::pervasives::bitset<vflag_ty> vflags_ty;

vflags_ty
voicef_from_legacy(const uint32_t);

vflags_ty
voicef_from_smpf(const sflags_ty);

vflags_ty
voicef_unset_smpf(const vflags_ty);

vflags_ty
voicef_from_oldchn(const uint32_t);

const size_t SpillMax = 256 * 2;

struct modinstrument_t;

struct modenvstate_t {
    uint32_t position;
    int32_t release_value;
};

struct flt_hist_ty {
    sample_t y1;
    sample_t y2;
};

struct flt_lr_ty {
    flt_hist_ty left;
    flt_hist_ty right;
};

const flt_lr_ty empty_flt_lr = { 0 };

struct flt_coefs_ty {
    sample_t a0;
    sample_t b0;
    sample_t b1;
    sample_t hp;
};

#pragma warning (push)
#pragma warning (disable : 4324) //structure was padded due to __declspec(align())
__declspec(align(32))
struct voice_ty {
    typedef modplug::mixgraph::sample_t sample_t;
    // First 32-bytes: Most used mixing information: don't change it
    // These fields are accessed directly by the MMX mixing code (look out for CHNOFS_PCURRENTSAMPLE), so the order is crucial
    sample_data_ty active_sample_data;
    sampleoffset_t sample_position;
    fixedpt_t fractional_sample_position; // actually 16-bit
    fixedpt_t position_delta; // 16.16

    int32_t right_volume;
    int32_t left_volume;

    // 2nd cache line
    int32_t length;
    vflags_ty flags;
    sampleoffset_t loop_start;
    sampleoffset_t loop_end;

    flt_coefs_ty flt_coefs;
    flt_lr_ty flt_hist;

    stag_ty sample_tag;

    int32_t right_ramp;
    int32_t left_ramp;
    int32_t nRampRightVol;
    int32_t nRampLeftVol;

    int32_t nROfs, nLOfs;
    int32_t nRampLength;

    // Information not used in the mixer
    sample_data_ty sample_data;
    int32_t nNewRightVol, nNewLeftVol;
    int32_t nRealVolume, nRealPan;
    int32_t nVolume, nPan, nFadeOutVol;
    int32_t nPeriod, nC5Speed, nPortamentoDest;

    modinstrument_t *instrument;
    modenvstate_t volume_envelope;
    modenvstate_t panning_envelope;
    modenvstate_t pitch_envelope;
    modsample_t *sample;

    uint32_t parent_channel;
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

    void ClearRowCmd();
    uint32_t GetVSTVolume();

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

    sample_t spill_back[SpillMax];
    sample_t spill_fwd[SpillMax];
};
#pragma warning (pop)



}
}
