#pragma once

#include "../../soundlib/Snd_defs.h"
#include "../../soundlib/modcommand.h"
#include <cstdint>

class CTuningBase;
typedef CTuningBase CTuning;

namespace modplug {
namespace mixer {


// Sample Struct
struct MODSAMPLE
{
    uint32_t nLength, nLoopStart, nLoopEnd;	// In samples, not bytes
    uint32_t nSustainStart, nSustainEnd;	// Dito
    char *pSample;						// Pointer to sample data
    uint32_t nC5Speed;						// Frequency of middle c, in Hz (for IT/S3M/MPTM)
    uint16_t nPan;							// Default sample panning (if pan flag is set)
    uint16_t nVolume;						// Default volume
    uint16_t nGlobalVol;					// Global volume (sample volume is multiplied by this)
    uint16_t uFlags;						// Sample flags
    signed char RelativeTone;			// Relative note to middle c (for MOD/XM)
    signed char nFineTune;				// Finetune period (for MOD/XM)
    uint8_t nVibType;						// Auto vibrato type
    uint8_t nVibSweep;						// Auto vibrato sweep (i.e. how long it takes until the vibrato effect reaches its full strength)
    uint8_t nVibDepth;						// Auto vibrato depth
    uint8_t nVibRate;						// Auto vibrato rate (speed)
    //char name[MAX_SAMPLENAME];		// Maybe it would be nicer to have sample names here, but that would require some refactoring. Also, would this slow down the mixer (cache misses)?
    char filename[MAX_SAMPLEFILENAME];

    // Return the size of one (elementary) sample in bytes.
    uint8_t GetElementarySampleSize() const {return (uFlags & CHN_16BIT) ? 2 : 1;}

    // Return the number of channels in the sample.
    uint8_t GetNumChannels() const {return (uFlags & CHN_STEREO) ? 2 : 1;}

    // Return the number of bytes per sampling point. (Channels * Elementary Sample Size)
    uint8_t GetBytesPerSample() const {return GetElementarySampleSize() * GetNumChannels();}

    // Return the size which pSample is at least.
    uint32_t GetSampleSizeInBytes() const {return nLength * GetBytesPerSample();}

    // Returns sample rate of the sample. The argument is needed because 
    // the sample rate is obtained differently for different module types.
    uint32_t GetSampleRate(const MODTYPE type) const;
};




// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"

/*---------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------
MODULAR STRUCT DECLARATIONS :
-----------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------------
---------------------------------------------------------------------------------------------*/




// Instrument Envelopes
struct INSTRUMENTENVELOPE
{
    uint32_t dwFlags;				// envelope flags
    uint16_t Ticks[MAX_ENVPOINTS];	// envelope point position (x axis)
    uint8_t Values[MAX_ENVPOINTS];	// envelope point value (y axis)
    uint32_t nNodes;				// amount of nodes used
    uint8_t nLoopStart;			// loop start node
    uint8_t nLoopEnd;				// loop end node
    uint8_t nSustainStart;			// sustain start node
    uint8_t nSustainEnd;			// sustain end node
    uint8_t nReleaseNode;			// release node
};

// Instrument Struct
struct MODINSTRUMENT
{
    uint32_t nFadeOut;				// Instrument fadeout speed
    uint32_t dwFlags;				// Instrument flags
    uint32_t nGlobalVol;			// Global volume (all sample volumes are multiplied with this)
    uint32_t nPan;					// Default pan (overrides sample panning), if the appropriate flag is set

    INSTRUMENTENVELOPE VolEnv;		// Volume envelope data
    INSTRUMENTENVELOPE PanEnv;		// Panning envelope data
    INSTRUMENTENVELOPE PitchEnv;	// Pitch / filter envelope data

    uint8_t NoteMap[128];	// Note mapping, f.e. C-5 => D-5.
    uint16_t Keyboard[128];	// Sample mapping, f.e. C-5 => Sample 1

    uint8_t nNNA;			// New note action
    uint8_t nDCT;			// Duplicate check type	(i.e. which condition will trigger the duplicate note action)
    uint8_t nDNA;			// Duplicate note action
    uint8_t nPanSwing;		// Random panning factor
    uint8_t nVolSwing;		// Random volume factor
    uint8_t nIFC;			// Default filter cutoff (00...7F). Used if the high bit is set
    uint8_t nIFR;			// Default filter resonance (00...7F). Used if the high bit is set

    uint16_t wMidiBank;		// MIDI bank
    uint8_t nMidiProgram;	// MIDI program
    uint8_t nMidiChannel;	// MIDI channel
    uint8_t nMidiDrumKey;	// Drum set note mapping (currently only used by the .MID loader)
    signed char nPPS;	//Pitch/Pan separation (i.e. how wide the panning spreads)
    unsigned char nPPC;	//Pitch/Pan centre

    char name[32];		// Note: not guaranteed to be null-terminated.
    char filename[32];

    PLUGINDEX nMixPlug;				// Plugin assigned to this instrument
// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
    uint16_t nVolRampUp;				// Default sample ramping up
    uint16_t nVolRampDown;			// Default sample ramping down
// -! NEW_FEATURE#0027
    uint32_t nResampling;				// Resampling mode
    uint8_t nCutSwing;					// Random cutoff factor
    uint8_t nResSwing;					// Random resonance factor
    uint8_t nFilterMode;				// Default filter mode
    uint16_t wPitchToTempoLock;			// BPM at which the samples assigned to this instrument loop correctly
    uint8_t nPluginVelocityHandling;	// How to deal with plugin velocity
    uint8_t nPluginVolumeHandling;		// How to deal with plugin volume
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// WHEN adding new members here, ALSO update Sndfile.cpp (instructions near the top of this file)!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    CTuning* pTuning;				// sample tuning assigned to this instrument
    static CTuning* s_DefaultTuning;

    void SetTuning(CTuning* pT)
    {
        pTuning = pT;
    }
};


// Envelope playback info for each MODCHANNEL
struct  MODCHANNEL_ENVINFO
{
    uint32_t nEnvPosition;
    int32_t nEnvValueAtReleaseJump;
};


#pragma warning(disable : 4324) //structure was padded due to __declspec(align())

// Channel Struct
typedef struct __declspec(align(32)) _MODCHANNEL
{
    // First 32-bytes: Most used mixing information: don't change it
    // These fields are accessed directly by the MMX mixing code (look out for CHNOFS_PCURRENTSAMPLE), so the order is crucial
    char *pCurrentSample;		
    uint32_t nPos;
    uint32_t nPosLo;	// actually 16-bit
    int32_t nInc;		// 16.16
    int32_t nRightVol;
    int32_t nLeftVol;
    int32_t nRightRamp;
    int32_t nLeftRamp;
    // 2nd cache line
    uint32_t nLength;
    uint32_t dwFlags;
    uint32_t nLoopStart;
    uint32_t nLoopEnd;
    int32_t nRampRightVol;
    int32_t nRampLeftVol;

    float nFilter_Y1, nFilter_Y2, nFilter_Y3, nFilter_Y4;
    float nFilter_A0, nFilter_B0, nFilter_B1;
    uint8_t nFilter_HP;

    int32_t nROfs, nLOfs;
    int32_t nRampLength;
    // Information not used in the mixer
    char *pSample;
    int32_t nNewRightVol, nNewLeftVol;
    int32_t nRealVolume, nRealPan;
    int32_t nVolume, nPan, nFadeOutVol;
    int32_t nPeriod, nC5Speed, nPortamentoDest;
    MODINSTRUMENT *pModInstrument;					// Currently assigned instrument slot
    MODCHANNEL_ENVINFO VolEnv, PanEnv, PitchEnv;	// Envelope playback info
    MODSAMPLE *pModSample;							// Currently assigned sample slot
    uint32_t nMasterChn, nVUMeter;
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
    ROWINDEX nPatternLoop;
    // 8-bit members
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

    uint16_t m_RowPlugParam;			//NOTE_PCs memory.
    float m_nPlugParamValueStep;  //rewbs.smoothVST 
    float m_nPlugInitialParamValue; //rewbs.smoothVST
    PLUGINDEX m_RowPlug;			//NOTE_PCs memory.
    
    void ClearRowCmd() {nRowNote = NOTE_NONE; nRowInstr = 0; nRowVolCmd = VOLCMD_NONE; nRowVolume = 0; nRowCommand = CMD_NONE; nRowParam = 0;}

    typedef uint32_t VOLUME;
    VOLUME GetVSTVolume() {return (pModInstrument) ? pModInstrument->nGlobalVol*4 : nVolume;}

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
} MODCHANNEL;

#define CHNRESET_CHNSETTINGS	1 //  1 b 
#define CHNRESET_SETPOS_BASIC	2 // 10 b
#define	CHNRESET_SETPOS_FULL	7 //111 b
#define CHNRESET_TOTAL			255 //11111111b


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
