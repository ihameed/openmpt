/*
 * OpenMPT
 *
 * Sndfile.h
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#ifndef __SNDFILE_H
#define __SNDFILE_H

#include "../SoundFilePlayConfig.h"
#include "../misc_util.h"
#include "mod_specifications.h"
#include <vector>
#include <bitset>
#include "midi.h"
#include "Snd_defs.h"
#include "Endianness.h"

#include "../audioio/paudio.hpp"
#include "../tracker/tracker.hpp"
#include "../mixgraph/core.hpp"
#include "../tracker/modsequence.hpp"
#include "../tracker/orderlist.hpp"
#include "misc.h"

// For VstInt32 and stuff - a stupid workaround for IMixPlugin.
#ifndef NO_VST
#define VST_FORCE_DEPRECATED 0
#include <aeffect.h>                    // VST
#else
typedef int32_t VstInt32;
typedef intptr_t VstIntPtr;
#endif


static const TCHAR gszEmpty[] = TEXT("");

//modplug::tracker::modinstrument_t;

// -----------------------------------------------------------------------------------------
// MODULAR modplug::tracker::modinstrument_t FIELD ACCESS : body content at the (near) top of Sndfile.cpp !!!
// -----------------------------------------------------------------------------------------
extern void WriteInstrumentHeaderStruct(modplug::tracker::modinstrument_t * input, FILE * file);
extern uint8_t * GetInstrumentHeaderFieldPointer(modplug::tracker::modinstrument_t * input, __int32 fcode, __int16 fsize);

// -! NEW_FEATURE#0027

// --------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------




////////////////////////////////////////////////////////////////////
// Mix Plugins
#define MIXPLUG_MIXREADY                    0x01        // Set when cleared

typedef VstInt32 PlugParamIndex;
typedef float PlugParamValue;

class IMixPlugin
{
public:
    virtual int AddRef() = 0;
    virtual int Release() = 0;
    virtual void SaveAllParameters() = 0;
    virtual void RestoreAllParameters(long nProg=-1) = 0; //rewbs.plugDefaultProgram: added param
    virtual void Process(float *pOutL, float *pOutR, unsigned long nSamples) = 0;
    virtual void Init(unsigned long nFreq, int bReset) = 0;
    virtual bool MidiSend(uint32_t dwMidiCode) = 0;
    virtual void MidiCC(UINT nMidiCh, UINT nController, UINT nParam, UINT trackChannel) = 0;
    virtual void MidiPitchBend(UINT nMidiCh, int nParam, UINT trackChannel) = 0;
    virtual void MidiCommand(UINT nMidiCh, UINT nMidiProg, uint16_t wMidiBank, UINT note, UINT vol, UINT trackChan) = 0;
    virtual void HardAllNotesOff() = 0;            //rewbs.VSTCompliance
    virtual void RecalculateGain() = 0;
    virtual bool isPlaying(UINT note, UINT midiChn, UINT trackerChn) = 0; //rewbs.VSTiNNA
    virtual bool MoveNote(UINT note, UINT midiChn, UINT sourceTrackerChn, UINT destTrackerChn) = 0; //rewbs.VSTiNNA
    virtual void SetParameter(PlugParamIndex paramindex, PlugParamValue paramvalue) = 0;
    virtual void SetZxxParameter(UINT nParam, UINT nValue) = 0;
    virtual PlugParamValue GetParameter(PlugParamIndex nIndex) = 0;
    virtual UINT GetZxxParameter(UINT nParam) = 0; //rewbs.smoothVST
    virtual void ModifyParameter(PlugParamIndex nIndex, PlugParamValue diff);
    virtual VstIntPtr Dispatch(VstInt32 opCode, VstInt32 index, VstIntPtr value, void *ptr, float opt) =0; //rewbs.VSTCompliance
    virtual void NotifySongPlaying(bool)=0;    //rewbs.VSTCompliance
    virtual bool IsSongPlaying()=0;
    virtual bool IsResumed()=0;
    virtual void Resume()=0;
    virtual void Suspend()=0;
    virtual bool isInstrument()=0;
    virtual bool CanRecieveMidiEvents()=0;
    virtual void SetDryRatio(UINT param)=0;

};

inline void IMixPlugin::ModifyParameter(PlugParamIndex nIndex, PlugParamValue diff)
//---------------------------------------------------------------------------------
{
    float val = GetParameter(nIndex) + diff;
    Limit(val, PlugParamValue(0), PlugParamValue(1));
    SetParameter(nIndex, val);
}


                                                ///////////////////////////////////////////////////
                                                // !!! bits 8 -> 15 reserved for mixing mode !!! //
                                                ///////////////////////////////////////////////////
#define MIXPLUG_INPUTF_MASTEREFFECT                            0x01        // Apply to master mix
#define MIXPLUG_INPUTF_BYPASS                                    0x02        // Bypass effect
#define MIXPLUG_INPUTF_WETMIX                                    0x04        // Wet Mix (dry added)
// -> CODE#0028
// -> DESC="effect plugin mixing mode combo"
#define MIXPLUG_INPUTF_MIXEXPAND                            0x08        // [0%,100%] -> [-200%,200%]
// -! BEHAVIOUR_CHANGE#0028


struct SNDMIXPLUGINSTATE
{
    uint32_t dwFlags;                                    // MIXPLUG_XXXX
    LONG nVolDecayL, nVolDecayR;    // Buffer click removal
    int *pMixBuffer;                            // Stereo effect send buffer
    float *pOutBufferL;                            // Temp storage for int -> float conversion
    float *pOutBufferR;
};
typedef SNDMIXPLUGINSTATE* PSNDMIXPLUGINSTATE;

struct SNDMIXPLUGININFO
{
    uint32_t dwPluginId1;
    uint32_t dwPluginId2;
    uint32_t dwInputRouting;    // MIXPLUG_INPUTF_XXXX, bits 16-23 = gain
    uint32_t dwOutputRouting;    // 0=mix 0x80+=fx
    uint32_t dwReserved[4];    // Reserved for routing info
    CHAR szName[32];
    CHAR szLibraryName[64];    // original DLL name
}; // Size should be 128
typedef SNDMIXPLUGININFO* PSNDMIXPLUGININFO;
static_assert(sizeof(SNDMIXPLUGININFO) == 128,
              "sizeof(SNDMIXPLUGININFO) *MUST* be 128!");

struct SNDMIXPLUGIN
{
    const char* GetName() const {return Info.szName;}
    const char* GetLibraryName();
    CString GetParamName(const UINT index) const;

    IMixPlugin *pMixPlugin;
    PSNDMIXPLUGINSTATE pMixState;
    ULONG nPluginDataSize;
    PVOID pPluginData;
    SNDMIXPLUGININFO Info;
    float fDryRatio;                // rewbs.dryRatio [20040123]
    long defaultProgram;            // rewbs.plugDefaultProgram
}; // rewbs.dryRatio: Hopefully this doesn't need to be a fixed size.
typedef SNDMIXPLUGIN* PSNDMIXPLUGIN;

//class CSoundFile;
class CModDoc;
typedef    BOOL (__cdecl *PMIXPLUGINCREATEPROC)(PSNDMIXPLUGIN, module_renderer*);


// Global MIDI macros
enum
{
    MIDIOUT_START=0,
    MIDIOUT_STOP,
    MIDIOUT_TICK,
    MIDIOUT_NOTEON,
    MIDIOUT_NOTEOFF,
    MIDIOUT_VOLUME,
    MIDIOUT_PAN,
    MIDIOUT_BANKSEL,
    MIDIOUT_PROGRAM,
};

#define NUM_MACROS 16    // number of parametered macros
#define MACRO_LENGTH 32    // max number of chars per macro
struct MODMIDICFG
{
    CHAR szMidiGlb[9][MACRO_LENGTH];
    CHAR szMidiSFXExt[16][MACRO_LENGTH];
    CHAR szMidiZXXExt[128][MACRO_LENGTH];
};
static_assert(sizeof(MODMIDICFG) == 4896, "sizeof(MODMIDICFG) *MUST* be 4896!");

typedef VOID (__cdecl * LPSNDMIXHOOKPROC)(int *, unsigned long, unsigned long); // buffer, samples, channels

#include "pattern.h"
#include "patternContainer.h"
#include "PlaybackEventer.h"

// Line ending types (for reading song messages from module files)
enum enmLineEndings
{
    leCR,                    // Carriage Return (0x0D, \r)
    leLF,                    // Line Feed (0x0A \n)
    leCRLF,                    // Carriage Return, Line Feed (0x0D0A, \r\n)
    leMixed,            // It is not defined whether Carriage Return or Line Feed is the actual line ending. Both are accepted.
    leAutodetect,    // Detect suitable line ending
};

// Data type for the visited rows routines.
typedef vector<bool> VisitedRowsBaseType;
typedef vector<VisitedRowsBaseType> VisitedRowsType;

// Return values for GetLength()
struct GetLengthType
{
    double duration;            // total time in seconds
    bool targetReached;            // true if the specified order/row combination has been reached while going through the module
    modplug::tracker::orderindex_t lastOrder;    // last parsed order (if no target is specified, this is the first order that is parsed twice, i.e. not the *last* played order)
    modplug::tracker::rowindex_t lastRow;            // last parsed row (dito)
    modplug::tracker::orderindex_t endOrder;    // last order before module loops (UNDEFINED if a target is specified)
    modplug::tracker::rowindex_t endRow;            // last row before module loops (dito)
};

// Reset mode for GetLength()
enum enmGetLengthResetMode
{
    // Never adjust global variables / mod parameters
    eNoAdjust                    = 0x00,
    // Mod parameters (such as global volume, speed, tempo, etc...) will always be memorized if the target was reached (i.e. they won't be reset to the previous values).  If target couldn't be reached, they are reset to their default values.
    eAdjust                            = 0x01,
    // Same as above, but global variables will only be memorized if the target could be reached. This does *NOT* influence the visited rows vector - it will *ALWAYS* be adjusted in this mode.
    eAdjustOnSuccess    = 0x02 | eAdjust,
};


// Row advance mode for TryWriteEffect()
enum writeEffectAllowRowChange
{
    weIgnore,                    // If effect can't be written, abort.
    weTryNextRow,            // If effect can't be written, try next row.
    weTryPreviousRow,    // If effect can't be written, try previous row.
};


//Note: These are bit indeces. MSF <-> Mod(Specific)Flag.
//If changing these, ChangeModTypeTo() might need modification.
const uint8_t MSF_COMPATIBLE_PLAY            = 0;                //IT/MPT/XM
const uint8_t MSF_OLDVOLSWING                    = 1;                //IT/MPT
const uint8_t MSF_MIDICC_BUGEMULATION    = 2;                //IT/MPT/XM


class CTuningCollection;

class module_renderer : public modplug::pervasives::noncopyable {
public:
    module_renderer();
    ~module_renderer();

    void change_audio_settings(const modplug::audioio::paudio_settings_t &);

    //XXXih:   <:(
    modplug::mixgraph::core mixgraph;
    modplug::tracker::orderlist_t orders;

private:
    modplug::audioio::paudio_settings_t render_settings;

    //XXXih: old api follows:
public:
    // Real-time sound functions
    void SuspendPlugins(); //rewbs.VSTCompliance
    void ResumePlugins();  //rewbs.VSTCompliance
    void StopAllVsti();    //rewbs.VSTCompliance
    void RecalculateGainForAllPlugs();
    void ResetChannels();
    UINT ReadPattern(LPVOID lpBuffer, UINT cbBuffer);
    UINT ReadMix(LPVOID lpBuffer, UINT cbBuffer, module_renderer *, uint32_t *, LPBYTE ps=NULL);
    UINT CreateStereoMix(int count);
    UINT GetResamplingFlag(const modplug::tracker::modchannel_t *pChannel);
    BOOL FadeSong(UINT msec);
    BOOL GlobalFadeSong(UINT msec);
    UINT GetTotalTickCount() const { return m_nTotalCount; }
    void ResetTotalTickCount() { m_nTotalCount = 0;}
    void ProcessPlugins(UINT nCount);

public:
    // Mixer Config
    static BOOL InitPlayer(BOOL bReset=FALSE);
    static BOOL deprecated_SetWaveConfig(UINT nRate,UINT nBits,UINT nChannels,BOOL bMMX=FALSE);
    static BOOL deprecated_SetResamplingMode(UINT nMode); // SRCMODE_XXXX
    static uint32_t GetSampleRate() { return deprecated_global_mixing_freq; }
    static uint32_t GetBitsPerSample() { return deprecated_global_bits_per_sample; }
    static uint32_t InitSysInfo();
    static uint32_t GetSysInfo() { return deprecated_global_system_info; }
    static void EnableMMX(BOOL b) { if (b) deprecated_global_sound_setup_bitmask |= SNDMIX_ENABLEMMX; else deprecated_global_sound_setup_bitmask &= ~SNDMIX_ENABLEMMX; }

    // Analyzer Functions
    static UINT WaveConvert(LPBYTE lpSrc, signed char *lpDest, UINT nSamples);
    static UINT WaveStereoConvert(LPBYTE lpSrc, signed char *lpDest, UINT nSamples);
    static LONG SpectrumAnalyzer(signed char *pBuffer, UINT nSamples, UINT nInc, UINT nChannels);
    // Float <-> Int conversion routines
    /*static */VOID StereoMixToFloat(const int *pSrc, float *pOut1, float *pOut2, UINT nCount);
    /*static */VOID FloatToStereoMix(const float *pIn1, const float *pIn2, int *pOut, UINT nCount);
    /*static */VOID MonoMixToFloat(const int *pSrc, float *pOut, UINT nCount);
    /*static */VOID FloatToMonoMix(const float *pIn, int *pOut, UINT nCount);

public:
    BOOL ReadNote();
    BOOL ProcessRow();
    BOOL ProcessEffects();
    UINT GetNNAChannel(UINT nChn) const;
    void CheckNNA(UINT nChn, UINT instr, int note, BOOL bForceCut);
    void NoteChange(UINT nChn, int note, bool bPorta = false, bool bResetEnv = true, bool bManual = false);
    void InstrumentChange(modplug::tracker::modchannel_t *pChn, UINT instr, bool bPorta = false, bool bUpdVol = true, bool bResetEnv = true);

    // Channel Effects
    void KeyOff(UINT nChn);
    // Global Effects
    void SetTempo(UINT param, bool setAsNonModcommand = false);
    void SetSpeed(UINT param);

private:
    // Channel Effects
    void PortamentoUp(modplug::tracker::modchannel_t *pChn, UINT param, const bool fineAsRegular = false);
    void PortamentoDown(modplug::tracker::modchannel_t *pChn, UINT param, const bool fineAsRegular = false);
    void MidiPortamento(modplug::tracker::modchannel_t *pChn, int param);
    void FinePortamentoUp(modplug::tracker::modchannel_t *pChn, UINT param);
    void FinePortamentoDown(modplug::tracker::modchannel_t *pChn, UINT param);
    void ExtraFinePortamentoUp(modplug::tracker::modchannel_t *pChn, UINT param);
    void ExtraFinePortamentoDown(modplug::tracker::modchannel_t *pChn, UINT param);
    void NoteSlide(modplug::tracker::modchannel_t *pChn, UINT param, int sign);
    void TonePortamento(modplug::tracker::modchannel_t *pChn, UINT param);
    void Vibrato(modplug::tracker::modchannel_t *pChn, UINT param);
    void FineVibrato(modplug::tracker::modchannel_t *pChn, UINT param);
    void VolumeSlide(modplug::tracker::modchannel_t *pChn, UINT param);
    void PanningSlide(modplug::tracker::modchannel_t *pChn, UINT param);
    void ChannelVolSlide(modplug::tracker::modchannel_t *pChn, UINT param);
    void FineVolumeUp(modplug::tracker::modchannel_t *pChn, UINT param);
    void FineVolumeDown(modplug::tracker::modchannel_t *pChn, UINT param);
    void Tremolo(modplug::tracker::modchannel_t *pChn, UINT param);
    void Panbrello(modplug::tracker::modchannel_t *pChn, UINT param);
    void RetrigNote(UINT nChn, int param, UINT offset=0);  //rewbs.volOffset: added last param
    void SampleOffset(UINT nChn, UINT param, bool bPorta);    //rewbs.volOffset: moved offset code to own method
    void NoteCut(UINT nChn, UINT nTick);
    int PatternLoop(modplug::tracker::modchannel_t *, UINT param);
    void ExtendedMODCommands(UINT nChn, UINT param);
    void ExtendedS3MCommands(UINT nChn, UINT param);
    void ExtendedChannelEffect(modplug::tracker::modchannel_t *, UINT param);
    inline void InvertLoop(modplug::tracker::modchannel_t* pChn);
    void ProcessMidiMacro(UINT nChn, bool isSmooth, LPCSTR pszMidiMacro, UINT param = 0);
    void SetupChannelFilter(modplug::tracker::modchannel_t *pChn, bool bReset, int flt_modifier = 256) const;
    // Low-Level effect processing
    void DoFreqSlide(modplug::tracker::modchannel_t *pChn, LONG nFreqSlide);
    void GlobalVolSlide(UINT param, UINT * nOldGlobalVolSlide);
    uint32_t IsSongFinished(UINT nOrder, UINT nRow) const;
    void UpdateTimeSignature();

    UINT GetNumTicksOnCurrentRow() const { return m_nMusicSpeed * (m_nPatternDelay + 1) + m_nFrameDelay; };
public:
    // Write pattern effect functions
    bool TryWriteEffect(
        modplug::tracker::patternindex_t nPat,
        modplug::tracker::rowindex_t nRow,
        uint8_t nEffect,
        uint8_t nParam,
        bool bIsVolumeEffect,
        modplug::tracker::chnindex_t nChn = modplug::tracker::ChannelIndexInvalid,
        bool bAllowMultipleEffects = true,
        writeEffectAllowRowChange allowRowChange = weIgnore,
        bool bRetry = true
    );

    // Read/Write sample functions
    char GetDeltaValue(char prev, UINT n) const { return (char)(prev + CompressionTable[n & 0x0F]); }
    UINT PackSample(int &sample, int next);
    bool CanPackSample(LPSTR pSample, UINT nLen, UINT nPacking, uint8_t *result=NULL);
    UINT ReadSample(modplug::tracker::modsample_t *pSmp, UINT nFlags, LPCSTR pMemFile, uint32_t dwMemLength, const uint16_t format = 1);
    bool DestroySample(modplug::tracker::sampleindex_t nSample);

// -> CODE#0020
// -> DESC="rearrange sample list"
    bool MoveSample(modplug::tracker::sampleindex_t from, modplug::tracker::sampleindex_t to);
// -! NEW_FEATURE#0020

// -> CODE#0003
// -> DESC="remove instrument's samples"
    //BOOL DestroyInstrument(UINT nInstr);
    bool DestroyInstrument(modplug::tracker::instrumentindex_t nInstr, char removeSamples = 0);
// -! BEHAVIOUR_CHANGE#0003
    bool IsSampleUsed(modplug::tracker::sampleindex_t nSample) const;
    bool IsInstrumentUsed(modplug::tracker::instrumentindex_t nInstr) const;
    bool RemoveInstrumentSamples(modplug::tracker::instrumentindex_t nInstr);
    modplug::tracker::sampleindex_t DetectUnusedSamples(vector<bool> &sampleUsed) const;
    modplug::tracker::sampleindex_t RemoveSelectedSamples(const vector<bool> &keepSamples);
    void AdjustSampleLoop(modplug::tracker::modsample_t *pSmp);
    // Samples file I/O
    bool ReadSampleFromFile(modplug::tracker::sampleindex_t nSample, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool ReadWAVSample(modplug::tracker::sampleindex_t nSample, LPBYTE lpMemFile, uint32_t dwFileLength, uint32_t *pdwWSMPOffset=NULL);
    bool ReadPATSample(modplug::tracker::sampleindex_t nSample, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool ReadS3ISample(modplug::tracker::sampleindex_t nSample, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool ReadAIFFSample(modplug::tracker::sampleindex_t nSample, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool ReadXISample(modplug::tracker::sampleindex_t nSample, LPBYTE lpMemFile, uint32_t dwFileLength);

// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
//    BOOL ReadITSSample(UINT nSample, LPBYTE lpMemFile, uint32_t dwFileLength, uint32_t dwOffset=0);
    UINT ReadITSSample(modplug::tracker::sampleindex_t nSample, LPBYTE lpMemFile, uint32_t dwFileLength, uint32_t dwOffset=0);
// -! NEW_FEATURE#0027

    bool Read8SVXSample(UINT nInstr, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool SaveWAVSample(UINT nSample, LPCSTR lpszFileName);
    bool SaveRAWSample(UINT nSample, LPCSTR lpszFileName);
    // Instrument file I/O
    bool ReadInstrumentFromFile(modplug::tracker::instrumentindex_t nInstr, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool ReadXIInstrument(modplug::tracker::instrumentindex_t nInstr, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool ReadITIInstrument(modplug::tracker::instrumentindex_t nInstr, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool ReadPATInstrument(modplug::tracker::instrumentindex_t nInstr, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool ReadSampleAsInstrument(modplug::tracker::instrumentindex_t nInstr, LPBYTE lpMemFile, uint32_t dwFileLength);
    bool SaveXIInstrument(modplug::tracker::instrumentindex_t nInstr, LPCSTR lpszFileName);
    bool SaveITIInstrument(modplug::tracker::instrumentindex_t nInstr, LPCSTR lpszFileName);
    // I/O from another sound file
    bool ReadInstrumentFromSong(modplug::tracker::instrumentindex_t nInstr, module_renderer *pSrcSong, UINT nSrcInstrument);
    bool ReadSampleFromSong(modplug::tracker::sampleindex_t nSample, module_renderer *pSrcSong, UINT nSrcSample);
    // Period/Note functions
    UINT GetNoteFromPeriod(UINT period) const;
    UINT GetPeriodFromNote(UINT note, int nFineTune, UINT nC5Speed) const;
    UINT GetFreqFromPeriod(UINT period, UINT nC5Speed, int nPeriodFrac=0) const;
    // Misc functions
    modplug::tracker::modsample_t *GetSample(UINT n) { return Samples+n; }
    void ResetMidiCfg();
    void SanitizeMacros();
    UINT MapMidiInstrument(uint32_t dwProgram, UINT nChannel, UINT nNote);
    long ITInstrToMPT(const void *p, modplug::tracker::modinstrument_t *pIns, UINT trkvers); //change from BOOL for rewbs.modularInstData
    UINT LoadMixPlugins(const void *pData, UINT nLen);
//    PSNDMIXPLUGIN GetSndPlugMixPlug(IMixPlugin *pPlugin); //rewbs.plugDocAware
#ifndef NO_FILTER
    uint32_t CutOffToFrequency(UINT nCutOff, int flt_modifier=256) const; // [0-255] => [1-10KHz]
#endif
#ifdef MODPLUG_TRACKER
    VOID ProcessMidiOut(UINT nChn, modplug::tracker::modchannel_t *pChn);            //rewbs.VSTdelay : added arg.
#endif
    VOID ApplyGlobalVolume(int SoundBuffer[], long lTotalSampleCount);

    // System-Dependant functions
public:
    static UINT Normalize24BitBuffer(LPBYTE pbuffer, UINT cbsizebytes, uint32_t lmax24, uint32_t dwByteInc);

    // Song message helper functions
public:
    // Allocate memory for song message.
    // [in]  length: text length in characters, without possible trailing null terminator.
    // [out] returns true on success.
    bool AllocateMessage(size_t length);

    // Free previously allocated song message memory.
    void FreeMessage();

protected:
    // Read song message from a mapped file.
    // [in]  data: pointer to the data in memory that is going to be read
    // [in]  length: number of characters that should be read, not including a possible trailing null terminator (it is automatically appended).
    // [in]  lineEnding: line ending formatting of the text in memory.
    // [in]  pTextConverter: Pointer to a callback function which can be used to pre-process the read characters, if necessary (nullptr otherwise).
    // [out] returns true on success.
    bool ReadMessage(const uint8_t *data, const size_t length, enmLineEndings lineEnding, void (*pTextConverter)(char &) = nullptr);

    // Read comments with fixed line length from a mapped file.
    // [in]  data: pointer to the data in memory that is going to be read
    // [in]  length: number of characters that should be read, not including a possible trailing null terminator (it is automatically appended).
    // [in]  lineLength: The fixed length of a line.
    // [in]  lineEndingLength: The padding space between two fixed lines. (there could for example be a null char after every line)
    // [in]  pTextConverter: Pointer to a callback function which can be used to pre-process the read characters, if necessary (nullptr otherwise).
    // [out] returns true on success.
    bool ReadFixedLineLengthMessage(const uint8_t *data, const size_t length, const size_t lineLength, const size_t lineEndingLength, void (*pTextConverter)(char &) = nullptr);

    // Currently unused (and the code doesn't look very nice :)
    UINT GetSongMessage(LPSTR s, UINT cbsize, UINT linesize=32);
    UINT GetRawSongMessage(LPSTR s, UINT cbsize, UINT linesize=32);

public:
    int GetVolEnvValueFromPosition(int position, modplug::tracker::modinstrument_t* pIns) const;
    void ResetChannelEnvelopes(modplug::tracker::modchannel_t *pChn);
    void ResetChannelEnvelope(modplug::tracker::modenvstate_t &env);
    void SetDefaultInstrumentValues(modplug::tracker::modinstrument_t *pIns);
private:
    UINT  __cdecl GetChannelPlugin(UINT nChan, bool respectMutes) const;
    UINT  __cdecl GetActiveInstrumentPlugin(UINT nChan, bool respectMutes) const;
    UINT GetBestMidiChan(const modplug::tracker::modchannel_t *pChn) const;

    void HandlePatternTransitionEvents();
    void BuildDefaultInstrument();
    long GetSampleOffset();

public:
    UINT GetBestPlugin(UINT nChn, UINT priority, bool respectMutes);

// A couple of functions for handling backwards jumps and stuff to prevent infinite loops when counting the mod length or rendering to wav.
public:
    void InitializeVisitedRows(const bool bReset = true, VisitedRowsType *pRowVector = nullptr);
private:
    void SetRowVisited(const modplug::tracker::orderindex_t nOrd, const modplug::tracker::rowindex_t nRow, const bool bVisited = true, VisitedRowsType *pRowVector = nullptr);
    bool IsRowVisited(const modplug::tracker::orderindex_t nOrd, const modplug::tracker::rowindex_t nRow, const bool bAutoSet = true, VisitedRowsType *pRowVector = nullptr);
    size_t GetVisitedRowsVectorSize(const modplug::tracker::patternindex_t nPat);

public:
    // "importance" of every FX command. Table is used for importing from formats with multiple effect columns
    // and is approximately the same as in SchismTracker.
    static uint16_t module_renderer::GetEffectWeight(modplug::tracker::cmd_t cmd);
    // try to convert a an effect into a volume column effect.
    static bool ConvertVolEffect(uint8_t *e, uint8_t *p, bool bForce);

    public:
    //Return true if title was changed.
    bool SetTitle(const char*, size_t strSize);

public: //Misc
    void ChangeModTypeTo(const MODTYPE& newType);

    // Returns value in seconds. If given position won't be played at all, returns -1.
    // If updateVars is true, the state of various playback variables will be updated according to the playback position.
    double GetPlaybackTimeAt(modplug::tracker::orderindex_t ord, modplug::tracker::rowindex_t row, bool updateVars);

    uint16_t GetModFlags() const {return m_ModFlags;}
    void SetModFlags(const uint16_t v) {m_ModFlags = v;}
    bool GetModFlag(uint8_t i) const {return ((m_ModFlags & (1<<i)) != 0);}
    void SetModFlag(uint8_t i, bool val) {if(i < 8*sizeof(m_ModFlags)) {m_ModFlags = (val) ? m_ModFlags |= (1 << i) : m_ModFlags &= ~(1 << i);}}

    // Is compatible mode for a specific tracker turned on?
    // Hint 1: No need to poll for MOD_TYPE_MPT, as it will automatically be linked with MOD_TYPE_IT when using TRK_IMPULSETRACKER
    // Hint 2: Always returns true for MOD / S3M format (if that is the format of the current file)
    bool IsCompatibleMode(MODTYPE type) const
    {
        if(GetType() & type & (MOD_TYPE_MOD | MOD_TYPE_S3M))
            return true; // S3M and MOD format don't have compatibility flags, so we will always return true
        return ((GetType() & type) && GetModFlag(MSF_COMPATIBLE_PLAY)) ? true : false;
    }

    //Tuning-->
public:
    static bool LoadStaticTunings();
    static bool SaveStaticTunings();
    static void DeleteStaticdata();
    static CTuningCollection& GetBuiltInTunings() {return *s_pTuningsSharedBuiltIn;}
    static CTuningCollection& GetLocalTunings() {return *s_pTuningsSharedLocal;}
    CTuningCollection& GetTuneSpecificTunings() {return *m_pTuningsTuneSpecific;}

    std::string GetNoteName(const int16_t&, const modplug::tracker::instrumentindex_t inst = modplug::tracker::InstrumentIndexInvalid) const;
private:
    CTuningCollection* m_pTuningsTuneSpecific;
    static CTuningCollection* s_pTuningsSharedBuiltIn;
    static CTuningCollection* s_pTuningsSharedLocal;
    //<--Tuning

public: //Get 'controllers'
    CPlaybackEventer& GetPlaybackEventer() {return m_PlaybackEventer;}
    const CPlaybackEventer& GetPlaybackEventer() const {return m_PlaybackEventer;}

    CMIDIMapper& GetMIDIMapper() {return m_MIDIMapper;}
    const CMIDIMapper& GetMIDIMapper() const {return m_MIDIMapper;}

private: //Effect functions
    void PortamentoMPT(modplug::tracker::modchannel_t*, int);
    void PortamentoFineMPT(modplug::tracker::modchannel_t*, int);

private: //Misc private methods.
    static void SetModSpecsPointer(const CModSpecifications*& pModSpecs, const MODTYPE type);
    uint16_t GetModFlagMask(const MODTYPE oldtype, const MODTYPE newtype) const;

private: //'Controllers'
    CPlaybackEventer m_PlaybackEventer;
    CMIDIMapper m_MIDIMapper;

private: //Misc data
    uint16_t m_ModFlags;
    const CModSpecifications* m_pModSpecs;
    bool m_bITBidiMode;    // Process bidi loops like Impulse Tracker (see Fastmix.cpp for an explanation)

    // For handling backwards jumps and stuff to prevent infinite loops when counting the mod length or rendering to wav.
    VisitedRowsType m_VisitedRows;

public:    // Static Members
    static float m_nMaxSample;
    static UINT m_nStereoSeparation;
    static UINT m_nMaxMixChannels;
    static LONG m_nStreamVolume;
    static uint32_t deprecated_global_system_info;
    static uint32_t deprecated_global_sound_setup_bitmask;
    static uint32_t deprecated_global_mixing_freq;
    static uint32_t deprecated_global_bits_per_sample;
    static uint32_t deprecated_global_channels;
    static UINT gnVolumeRampInSamples;
    static UINT gnVolumeRampOutSamples;
    static UINT gnCPUUsage;
    static LPSNDMIXHOOKPROC sound_mix_callback;
    static PMIXPLUGINCREATEPROC gpMixPluginCreateProc;
    static uint8_t s_DefaultPlugVolumeHandling;


    //XXXih: not related to rendering
public:    // for Editing
    CModDoc* m_pModDoc;            // Can be a null pointer f.e. when previewing samples from the treeview.
    MODTYPE m_nType;
    modplug::tracker::chnindex_t m_nChannels;
    modplug::tracker::sampleindex_t m_nSamples;
    modplug::tracker::instrumentindex_t m_nInstruments;
    UINT m_nDefaultSpeed, m_nDefaultTempo, m_nDefaultGlobalVolume;
    uint32_t m_dwSongFlags;                                                    // Song flags SONG_XXXX
    bool m_bIsRendering;
    UINT m_nMixChannels, m_nMixStat, m_nBufferCount;
    double m_dBufferDiff;
    UINT m_nTickCount, m_nTotalCount;
    UINT m_nPatternDelay, m_nFrameDelay;    // m_nPatternDelay = pattern delay, m_nFrameDelay = fine pattern delay
    ULONG m_lTotalSampleCount;    // rewbs.VSTTimeInfo
    UINT m_nSamplesPerTick;            // rewbs.betterBPM
    modplug::tracker::rowindex_t m_nDefaultRowsPerBeat, m_nDefaultRowsPerMeasure;    // default rows per beat and measure for this module // rewbs.betterBPM
    modplug::tracker::rowindex_t m_nCurrentRowsPerBeat, m_nCurrentRowsPerMeasure;    // current rows per beat and measure for this module
    uint8_t m_nTempoMode;                    // rewbs.betterBPM
    uint8_t m_nMixLevels;
    UINT m_nMusicSpeed, m_nMusicTempo;
    modplug::tracker::rowindex_t m_nNextRow, m_nRow;
    modplug::tracker::rowindex_t m_nNextPatStartRow; // for FT2's E60 bug
    modplug::tracker::patternindex_t m_nPattern;
    modplug::tracker::orderindex_t m_nCurrentPattern, m_nNextPattern, m_nRestartPos, m_nSeqOverride;
    //NOTE: m_nCurrentPattern and m_nNextPattern refer to order index - not pattern index.
    bool m_bPatternTransitionOccurred;
    UINT m_nMasterVolume, m_nGlobalVolume, m_nSamplesToGlobalVolRampDest,
         m_nGlobalVolumeDestination, m_nSamplePreAmp, m_nVSTiVolume;
    long m_lHighResRampingGlobalVolume;
    UINT m_nFreqFactor, m_nTempoFactor, m_nOldGlbVolSlide;
    LONG m_nMinPeriod, m_nMaxPeriod;    // bad_min period = highest possible frequency, bad_max period = lowest possible frequency
    LONG m_nRepeatCount;    // -1 means repeat infinitely.
    uint32_t m_nGlobalFadeSamples, m_nGlobalFadeMaxSamples;
    UINT m_nMaxOrderPosition;
    LPSTR m_lpszSongComments;
    UINT ChnMix[MAX_VIRTUAL_CHANNELS];                                                    // Channels to be mixed
    modplug::tracker::modchannel_t Chn[MAX_VIRTUAL_CHANNELS];                                            // Mixing channels... First m_nChannel channels are master channels (i.e. they are never NNA channels)!
    modplug::tracker::MODCHANNELSETTINGS ChnSettings[MAX_BASECHANNELS];    // Initial channels settings
    CPatternContainer Patterns;                                                    // Patterns
    modplug::tracker::deprecated_modsequence_list_t Order;                                                            // Modsequences. Order[x] returns an index of a pattern located at order x of the current sequence.


    modplug::tracker::modsample_t Samples[MAX_SAMPLES];                                            // Sample Headers
    modplug::tracker::modinstrument_t *Instruments[MAX_INSTRUMENTS];            // Instrument Headers
    modplug::tracker::modinstrument_t m_defaultInstrument;                                    // Currently only used to get default values for extented properties.

    CHAR m_szNames[MAX_SAMPLES][MAX_SAMPLENAME];            // Song and sample names
    std::string song_name;

    MODMIDICFG m_MidiCfg;                                                            // Midi macro config table
    SNDMIXPLUGIN m_MixPlugins[MAX_MIXPLUGINS];                    // Mix plugins
    CHAR CompressionTable[16];                                                    // ADPCM compression LUT
    bool m_bChannelMuteTogglePending[MAX_BASECHANNELS];

    CSoundFilePlayConfig* m_pConfig;
    uint32_t m_dwCreatedWithVersion;
    uint32_t m_dwLastSavedWithVersion;

// -> CODE#0023
// -> DESC="IT project files (.itp)"
    CHAR m_szInstrumentPath[MAX_INSTRUMENTS][_MAX_PATH];
// -! NEW_FEATURE#0023

public:
    BOOL Create(const uint8_t * lpStream, CModDoc *pModDoc, uint32_t dwMemLength=0);
    BOOL Destroy();
    MODTYPE GetType() const { return m_nType; }
    inline bool TypeIsIT_MPT() const { return (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT)) != 0; }
    inline bool TypeIsIT_MPT_XM() const { return (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM)) != 0; }
    inline bool TypeIsS3M_IT_MPT() const { return (m_nType & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT)) != 0; }
    inline bool TypeIsXM_MOD() const { return (m_nType & (MOD_TYPE_XM | MOD_TYPE_MOD)) != 0; }
    inline bool TypeIsMOD_S3M() const { return (m_nType & (MOD_TYPE_MOD | MOD_TYPE_S3M)) != 0; }
    CModDoc* GetpModDoc() const { return m_pModDoc; }
    CModDoc* GetModDocPtr() const { return m_pModDoc; }

    void SetMasterVolume(UINT vol, bool adjustAGC = false);
    UINT GetMasterVolume() const { return m_nMasterVolume; }

    // Returns 1 + index of last valid pattern, zero if no such pattern exists.
    modplug::tracker::patternindex_t GetNumPatterns() const;
    modplug::tracker::instrumentindex_t GetNumInstruments() const { return m_nInstruments; }
    modplug::tracker::sampleindex_t GetNumSamples() const { return m_nSamples; }
    UINT GetCurrentPos() const;
    modplug::tracker::patternindex_t GetCurrentPattern() const { return m_nPattern; }
    modplug::tracker::orderindex_t GetCurrentOrder() const { return static_cast<modplug::tracker::orderindex_t>(m_nCurrentPattern); }
    UINT GetMaxPosition() const;
    modplug::tracker::chnindex_t GetNumChannels() const { return m_nChannels; }

    IMixPlugin* GetInstrumentPlugin(modplug::tracker::instrumentindex_t instr);
    const CModSpecifications& GetModSpecifications() const {return *m_pModSpecs;}
    static const CModSpecifications& GetModSpecifications(const MODTYPE type);

    double GetCurrentBPM() const;
    modplug::tracker::orderindex_t FindOrder(modplug::tracker::patternindex_t nPat, UINT startFromOrder=0, bool direction = true);    //rewbs.playSongFromCursor
    void DontLoopPattern(modplug::tracker::patternindex_t nPat, modplug::tracker::rowindex_t nRow = 0);            //rewbs.playSongFromCursor
    void SetCurrentPos(UINT nPos);
    void SetCurrentOrder(modplug::tracker::orderindex_t nOrder);
    LPCTSTR GetSampleName(UINT nSample) const;
    CString GetInstrumentName(UINT nInstr) const;
    UINT GetMusicSpeed() const { return m_nMusicSpeed; }
    UINT GetMusicTempo() const { return m_nMusicTempo; }

    //Get modlength in various cases: total length, length to
    //specific order&row etc. Return value is in seconds.
    GetLengthType GetLength(
        enmGetLengthResetMode adjustMode,
        modplug::tracker::orderindex_t ord = modplug::tracker::OrderIndexInvalid,
        modplug::tracker::rowindex_t row = modplug::tracker::RowIndexInvalid
    );

public:
    //Returns song length in seconds.
    uint32_t GetSongTime() { return static_cast<uint32_t>((m_nTempoMode == tempo_mode_alternative) ? GetLength(eNoAdjust).duration + 1.0 : GetLength(eNoAdjust).duration + 0.5); }

    // A repeat count value of -1 means infinite loop
    void SetRepeatCount(int n) { m_nRepeatCount = n; }
    int GetRepeatCount() const { return m_nRepeatCount; }
    bool IsPaused() const {    return (m_dwSongFlags & (SONG_PAUSED|SONG_STEP)) ? true : false; }        // Added SONG_STEP as it seems to be desirable in most cases to check for this as well.
    void LoopPattern(modplug::tracker::patternindex_t nPat, modplug::tracker::rowindex_t nRow = 0);
    void CheckCPUUsage(UINT nCPU);

    void SetupITBidiMode();

    bool InitChannel(modplug::tracker::chnindex_t nChn);
    void ResetChannelState(modplug::tracker::chnindex_t chn, uint8_t resetStyle);

    bool ReadIT(const uint8_t * const lpStream, const uint32_t dwMemLength);

    // Save Functions
#ifndef MODPLUG_NO_FILESAVE
    UINT WriteSample(FILE *f, modplug::tracker::modsample_t *pSmp, UINT nFlags, UINT nMaxLen=0);
    bool SaveS3M(LPCSTR lpszFileName, UINT nPacking=0);
    bool SaveMod(LPCSTR lpszFileName, UINT nPacking=0, const bool bCompatibilityExport = false);
    bool SaveIT(LPCSTR lpszFileName, UINT nPacking=0);
    bool SaveCompatIT(LPCSTR lpszFileName);
    UINT SaveMixPlugins(FILE *f=NULL, BOOL bUpdate=TRUE);
    void WriteInstrumentPropertyForAllInstruments(__int32 code,  __int16 size, FILE* f, modplug::tracker::modinstrument_t* instruments[], UINT nInstruments);
    void SaveExtendedInstrumentProperties(modplug::tracker::modinstrument_t *instruments[], UINT nInstruments, FILE* f);
    void SaveExtendedSongProperties(FILE* f);
    void LoadExtendedSongProperties(const MODTYPE modtype, const uint8_t * ptr, const uint8_t * const startpos, const size_t seachlimit, bool* pInterpretMptMade = nullptr);
#endif // MODPLUG_NO_FILESAVE

    // Reads extended instrument properties(XM/IT/MPTM).
    // If no errors occur and song extension tag is found, returns pointer to the beginning
    // of the tag, else returns NULL.
    const uint8_t * LoadExtendedInstrumentProperties(const uint8_t * const pStart, const uint8_t * const pEnd, bool* pInterpretMptMade = nullptr);

    // MOD Convert function
    MODTYPE GetBestSaveFormat() const;
    MODTYPE GetSaveFormats() const;
    void ConvertModCommand(modplug::tracker::modevent_t *) const;
    void S3MConvert(modplug::tracker::modevent_t *m, bool bIT) const;
    void S3MSaveConvert(UINT *pcmd, UINT *pprm, bool bIT, bool bCompatibilityExport = false) const;
    uint16_t ModSaveCommand(const modplug::tracker::modevent_t *m, const bool bXM, const bool bCompatibilityExport = false) const;

    static void ConvertCommand(modplug::tracker::modevent_t *m, MODTYPE nOldType, MODTYPE nNewType); // Convert a complete modplug::tracker::modcommand_t item from one format to another
    static void MODExx2S3MSxx(modplug::tracker::modevent_t *m); // Convert Exx to Sxx
    static void S3MSxx2MODExx(modplug::tracker::modevent_t *m); // Convert Sxx to Exx
    void SetupMODPanning(bool bForceSetup = false); // Setup LRRL panning, bad_max channel volume
};

#pragma warning(default : 4324) //structure was padded due to __declspec(align())


inline IMixPlugin* module_renderer::GetInstrumentPlugin(modplug::tracker::instrumentindex_t instr)
//-----------------------------------------------------------------------
{
    if(instr > 0 && instr < MAX_INSTRUMENTS && Instruments[instr] && Instruments[instr]->nMixPlug && Instruments[instr]->nMixPlug <= MAX_MIXPLUGINS)
        return m_MixPlugins[Instruments[instr]->nMixPlug-1].pMixPlugin;
    else
        return NULL;
}


#define FADESONGDELAY            100

// Calling conventions
#ifdef WIN32
#define MPPASMCALL    __cdecl
#define MPPFASTCALL    __fastcall
#else
#define MPPASMCALL
#define MPPFASTCALL
#endif

#define MOD2XMFineTune(k)    ((int)( (signed char)((k)<<4) ))
#define XM2MODFineTune(k)    ((int)( (k>>4)&0x0f ))

int _muldiv(long a, long b, long c);
int _muldivr(long a, long b, long c);

typedef struct MODFORMATINFO
{
    MODTYPE mtFormatId;            // MOD_TYPE_XXXX
    LPCSTR  lpszFormatName;    // "ProTracker"
    LPCSTR  lpszExtension;    // ".xxx"
    uint32_t   dwPadding;
} MODFORMATINFO;

extern MODFORMATINFO gModFormatInfo[];

// Used in instrument/song extension reading to make sure the size field is valid.
bool IsValidSizeField(const uint8_t * const pData, const uint8_t * const pEnd, const int16_t size);

// Read instrument property with 'code' and 'size' from 'ptr' to instrument 'pIns'.
// Note: (ptr, size) pair must be valid (e.g. can read 'size' bytes from 'ptr')
void ReadInstrumentExtensionField(modplug::tracker::modinstrument_t* pIns, const uint8_t *& ptr, const int32_t code, const int16_t size);

// Read instrument property with 'code' from 'pData' to instrument 'pIns'.
void ReadExtendedInstrumentProperty(modplug::tracker::modinstrument_t* pIns, const int32_t code, const uint8_t *& pData, const uint8_t * const pEnd);

// Read extended instrument properties from 'pDataStart' to instrument 'pIns'.
void ReadExtendedInstrumentProperties(modplug::tracker::modinstrument_t* pIns, const uint8_t * const pDataStart, const size_t nMemLength);

// Convert instrument flags which were read from 'dF..' extension to proper internal representation.
void ConvertReadExtendedFlags(modplug::tracker::modinstrument_t* pIns);


#endif
