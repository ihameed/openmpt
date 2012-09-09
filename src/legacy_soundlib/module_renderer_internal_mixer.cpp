/*
 * OpenMPT
 *
 * Sndmix.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"

#include "../mptrack.h"
#include "../moddoc.h"
#include "../MainFrm.h"
#include "../misc_util.h"

#include "../mixgraph/constants.h"
#include "../mixer/mixutil.h"
#include "sndfile.h"
#include "midi.h"
#include "tuning.h"

#ifdef MODPLUG_TRACKER
    #define ENABLE_STEREOVU
#endif

// Volume ramp length, in 1/10 ms
#define VOLUMERAMPLEN    0        // 1.46ms = 64 samples at 44.1kHz                //rewbs.soundQ exp - was 146

// VU-Meter

static const unsigned int VUMETER_DECAY = 4;

// SNDMIX: These are global flags for playback control
UINT module_renderer::m_nStereoSeparation = 128;
LONG module_renderer::m_nStreamVolume = 0x8000;
UINT module_renderer::m_nMaxMixChannels = MAX_VIRTUAL_CHANNELS;
// Mixing Configuration (SetWaveConfig)
uint32_t module_renderer::deprecated_global_system_info = 0;
uint32_t module_renderer::deprecated_global_channels = 1;
uint32_t module_renderer::deprecated_global_sound_setup_bitmask = 0;
uint32_t module_renderer::deprecated_global_mixing_freq = 44100;
uint32_t module_renderer::deprecated_global_bits_per_sample = 16;
// Mixing data initialized in
UINT module_renderer::gnVolumeRampInSamples = 0;            //default value
UINT module_renderer::gnVolumeRampOutSamples = 42;            //default value
UINT module_renderer::gnCPUUsage = 0;
LPSNDMIXHOOKPROC module_renderer::sound_mix_callback = NULL;
PMIXPLUGINCREATEPROC module_renderer::gpMixPluginCreateProc = NULL;
LONG gnDryROfsVol = 0;
LONG gnDryLOfsVol = 0;
int gbInitPlugins = 0;
bool gbInitTables = 0;

typedef size_t (MPPASMCALL * LPCONVERTPROC)(void *, int *, size_t);

extern VOID MPPASMCALL X86_Dither(int *pBuffer, UINT nSamples, UINT nBits);
extern VOID MPPASMCALL X86_MonoFromStereo(int *pMixBuf, UINT nSamples);
extern VOID SndMixInitializeTables();

extern short int ModSinusTable[64];
extern short int ModRampDownTable[64];
extern short int ModSquareTable[64];
extern short int ModRandomTable[64];
extern short int ITSinusTable[64];
extern short int ITRampDownTable[64];
extern short int ITSquareTable[64];
extern uint32_t LinearSlideUpTable[256];
extern uint32_t LinearSlideDownTable[256];
extern uint32_t FineLinearSlideUpTable[16];
extern uint32_t FineLinearSlideDownTable[16];
extern signed char ft2VibratoTable[256];    // -64 .. +64
extern int MixSoundBuffer[modplug::mixgraph::MIX_BUFFER_SIZE*4];

// Log tables for pre-amp
const UINT PreAmpTable[16] =
{
    0x60, 0x60, 0x60, 0x70,    // 0-7
    0x80, 0x88, 0x90, 0x98,    // 8-15
    0xA0, 0xA4, 0xA8, 0xAC,    // 16-23
    0xB0, 0xB4, 0xB8, 0xBC,    // 24-31
};

const UINT PreAmpAGCTable[16] =
{
    0x60, 0x60, 0x60, 0x64,
    0x68, 0x70, 0x78, 0x80,
    0x84, 0x88, 0x8C, 0x90,
    0x92, 0x94, 0x96, 0x98,
};

typedef CTuning::RATIOTYPE RATIOTYPE;

static const RATIOTYPE TwoToPowerXOver12Table[16] =
{
    1.0F               , 1.059463094359F, 1.122462048309F, 1.1892071150027F,
    1.259921049895F, 1.334839854170F, 1.414213562373F, 1.4983070768767F,
    1.587401051968F, 1.681792830507F, 1.781797436281F, 1.8877486253634F,
    2.0F               , 2.118926188719F, 2.244924096619F, 2.3784142300054F
};

inline RATIOTYPE TwoToPowerXOver12(const uint8_t i)
//-------------------------------------------
{
    return (i < 16) ? TwoToPowerXOver12Table[i] : 1;
}


// Return (a*b)/c - no divide error
int _muldiv(long a, long b, long c)
{
    int sign, result;
    _asm {
    mov eax, a
    mov ebx, b
    or eax, eax
    mov edx, eax
    jge aneg
    neg eax
aneg:
    xor edx, ebx
    or ebx, ebx
    mov ecx, c
    jge bneg
    neg ebx
bneg:
    xor edx, ecx
    or ecx, ecx
    mov sign, edx
    jge cneg
    neg ecx
cneg:
    mul ebx
    cmp edx, ecx
    jae diverr
    div ecx
    jmp ok
diverr:
    mov eax, 0x7fffffff
ok:
    mov edx, sign
    or edx, edx
    jge rneg
    neg eax
rneg:
    mov result, eax
    }
    return result;
}


// Return (a*b+c/2)/c - no divide error
int _muldivr(long a, long b, long c)
{
    int sign, result;
    _asm {
    mov eax, a
    mov ebx, b
    or eax, eax
    mov edx, eax
    jge aneg
    neg eax
aneg:
    xor edx, ebx
    or ebx, ebx
    mov ecx, c
    jge bneg
    neg ebx
bneg:
    xor edx, ecx
    or ecx, ecx
    mov sign, edx
    jge cneg
    neg ecx
cneg:
    mul ebx
    mov ebx, ecx
    shr ebx, 1
    add eax, ebx
    adc edx, 0
    cmp edx, ecx
    jae diverr
    div ecx
    jmp ok
diverr:
    mov eax, 0x7fffffff
ok:
    mov edx, sign
    or edx, edx
    jge rneg
    neg eax
rneg:
    mov result, eax
    }
    return result;
}


BOOL module_renderer::InitPlayer(BOOL bReset)
//--------------------------------------
{
    DEBUG_FUNC("");
#ifndef FASTSOUNDLIB
    if (!gbInitTables)
    {
        SndMixInitializeTables();
        gbInitTables = true;
    }
#endif
    if (m_nMaxMixChannels > MAX_VIRTUAL_CHANNELS) m_nMaxMixChannels = MAX_VIRTUAL_CHANNELS;
    if (deprecated_global_mixing_freq < 4000) deprecated_global_mixing_freq = 4000;
    if (deprecated_global_mixing_freq > MAX_SAMPLE_RATE) deprecated_global_mixing_freq = MAX_SAMPLE_RATE;
    // Start with ramping disabled to avoid clicks on first read.
    // Ramping is now set after the first read in CSoundFile::Read();
    gnVolumeRampInSamples = 0;
    gnVolumeRampOutSamples = 0;
    gnDryROfsVol = gnDryLOfsVol = 0;
    if (bReset) gnCPUUsage = 0;
    gbInitPlugins = (bReset) ? 3 : 1;
    return TRUE;
}


BOOL module_renderer::FadeSong(UINT msec)
//----------------------------------
{
    LONG nsamples = _muldiv(msec, deprecated_global_mixing_freq, 1000);
    if (nsamples <= 0) return FALSE;
    if (nsamples > 0x100000) nsamples = 0x100000;
    m_nBufferCount = nsamples;
    LONG nRampLength = m_nBufferCount;
    // Ramp everything down
    for (UINT noff=0; noff < m_nMixChannels; noff++)
    {
        modplug::tracker::modchannel_t *pramp = &Chn[ChnMix[noff]];
        if (!pramp) continue;
        pramp->nNewLeftVol = pramp->nNewRightVol = 0;
        pramp->right_ramp = (-pramp->right_volume << modplug::mixgraph::VOLUME_RAMP_PRECISION) / nRampLength;
        pramp->left_ramp = (-pramp->left_volume << modplug::mixgraph::VOLUME_RAMP_PRECISION) / nRampLength;
        pramp->nRampRightVol = pramp->right_volume << modplug::mixgraph::VOLUME_RAMP_PRECISION;
        pramp->nRampLeftVol = pramp->left_volume << modplug::mixgraph::VOLUME_RAMP_PRECISION;
        pramp->nRampLength = nRampLength;
        pramp->flags |= CHN_VOLUMERAMP;
    }
    m_dwSongFlags |= SONG_FADINGSONG;
    return TRUE;
}


BOOL module_renderer::GlobalFadeSong(UINT msec)
//----------------------------------------
{
    if (m_dwSongFlags & SONG_GLOBALFADE) return FALSE;
    m_nGlobalFadeMaxSamples = _muldiv(msec, deprecated_global_mixing_freq, 1000);
    m_nGlobalFadeSamples = m_nGlobalFadeMaxSamples;
    m_dwSongFlags |= SONG_GLOBALFADE;
    return TRUE;
}


UINT module_renderer::ReadPattern(void *out_buffer, size_t out_buffer_length) {
    //XXXih: i render here!
    //XXXih: gnBitsPerSample shouldn't exist!
    deprecated_global_bits_per_sample = 16;
    unsigned char *buffer = static_cast<unsigned char *>(out_buffer);
    LPCONVERTPROC clip_samples = modplug::mixer::clip_32_to_8;
    size_t max_samples;
    size_t sample_width;
    size_t num_samples;
    size_t uncomputed_samples;
    size_t computed_samples;
    unsigned int nStat=0;
    unsigned int last_plugin_idx;

    last_plugin_idx = MAX_MIXPLUGINS;
    while ((last_plugin_idx > 0) && (!m_MixPlugins[last_plugin_idx-1].pMixPlugin)) last_plugin_idx--;
    m_nMixStat = 0;
    sample_width = deprecated_global_channels;
    switch (deprecated_global_bits_per_sample) {
        case 16: sample_width *= 2; clip_samples = modplug::mixer::clip_32_to_16; break;
        case 24: sample_width *= 3; clip_samples = modplug::mixer::clip_32_to_24; break;
        case 32: sample_width *= 4; clip_samples = modplug::mixer::clip_32_to_32; break;
    }

    max_samples = out_buffer_length / sample_width;
    if ((!max_samples) || (!buffer) || (!m_nChannels)) return 0;
    uncomputed_samples = max_samples;
    if (m_dwSongFlags & SONG_ENDREACHED)
        goto MixDone;
    while (uncomputed_samples > 0)
    {
        // Update Channel Data
        if (!m_nBufferCount)
        {
#ifndef FASTSOUNDLIB
            if (m_dwSongFlags & SONG_FADINGSONG) {
                m_dwSongFlags |= SONG_ENDREACHED;
                m_nBufferCount = uncomputed_samples;
            } else
#endif
            if (ReadNote()) {
                // Save pattern cue points for WAV rendering here (if we reached a new pattern, that is.)
                if (m_bIsRendering && (m_PatternCuePoints.empty() || m_nCurrentPattern != m_PatternCuePoints.back().order)) {
                    PatternCuePoint cue;
                    cue.offset = max_samples - uncomputed_samples;
                    cue.order = m_nCurrentPattern;
                    cue.processed = false;    // We don't know the base offset in the file here. It has to be added in the main conversion loop.
                    m_PatternCuePoints.push_back(cue);
                }
            } else {
                #ifdef MODPLUG_TRACKER
                    if ((m_nMaxOrderPosition) && (m_nCurrentPattern >= m_nMaxOrderPosition)) {
                        m_dwSongFlags |= SONG_ENDREACHED;
                        break;
                    }
                #endif
                #ifndef FASTSOUNDLIB
                    if (!FadeSong(FADESONGDELAY) || m_bIsRendering)    //rewbs: disable song fade when rendering.
                #endif
                    {
                        m_dwSongFlags |= SONG_ENDREACHED;
                        if (uncomputed_samples == max_samples || m_bIsRendering)            //rewbs: don't complete buffer when rendering
                            goto MixDone;
                        m_nBufferCount = uncomputed_samples;
                    }
            }
        }
        computed_samples = m_nBufferCount;
        if (computed_samples > modplug::mixgraph::MIX_BUFFER_SIZE) computed_samples = modplug::mixgraph::MIX_BUFFER_SIZE;
        if (computed_samples > uncomputed_samples) computed_samples = uncomputed_samples;
        if (!computed_samples)
            break;
        num_samples = computed_samples;
        // Resetting sound buffer
        modplug::mixer::stereo_fill(MixSoundBuffer, num_samples, &gnDryROfsVol, &gnDryLOfsVol);

        // ensure modplug::mixer::MIX_BUFFER_SIZE really is our max buffer size
        ASSERT (computed_samples <= modplug::mixgraph::MIX_BUFFER_SIZE);

        num_samples *= (deprecated_global_channels >= 2) ? 2 : 1;
        mixgraph.pre_process(computed_samples);
        m_nMixStat += CreateStereoMix(computed_samples);

        //XXXih: JUICY FRUITS
        /*
        #ifndef NO_REVERB
            ProcessReverb(computed_samples);
        #endif
        if (last_plugin_idx) {
            ProcessPlugins(computed_samples);
        }
        */
        mixgraph.process(MixSoundBuffer, computed_samples, m_pConfig->getFloatToInt(), m_pConfig->getIntToFloat());

        if (deprecated_global_channels < 2) {
            X86_MonoFromStereo(MixSoundBuffer, computed_samples);
        }
        nStat++;

        size_t total_num_samples = num_samples;

        // Noise Shaping
        if (deprecated_global_bits_per_sample <= 16)
        {
            if ((deprecated_global_sound_setup_bitmask & SNDMIX_HQRESAMPLER)
             && ((gnCPUUsage < 25) || (deprecated_global_sound_setup_bitmask & SNDMIX_DIRECTTODISK)))
                X86_Dither(MixSoundBuffer, total_num_samples, deprecated_global_bits_per_sample);
        }

        //Apply global volume
        if (m_pConfig->getGlobalVolumeAppliesToMaster()) {
            ApplyGlobalVolume(MixSoundBuffer, total_num_samples);
        }

        // Hook Function
        if (sound_mix_callback) {    //Currently only used for VU Meter, so it's OK to do it after global Vol.
            sound_mix_callback(MixSoundBuffer, total_num_samples, deprecated_global_channels);
        }

        // Perform clipping
        buffer += clip_samples(buffer, MixSoundBuffer, total_num_samples);

        // Buffer ready
        uncomputed_samples -= computed_samples;
        m_nBufferCount -= computed_samples;
        m_lTotalSampleCount += computed_samples;            // increase sample count for VSTTimeInfo.
        // Turn on ramping after first read (fix http://forum.openmpt.org/index.php?topic=523.0 )
        gnVolumeRampInSamples = CMainFrame::glVolumeRampInSamples;
        gnVolumeRampOutSamples = CMainFrame::glVolumeRampOutSamples;
    }
MixDone:
    if (uncomputed_samples) memset(buffer, (deprecated_global_bits_per_sample == 8) ? 0x80 : 0, uncomputed_samples * sample_width);
    if (nStat) { m_nMixStat += nStat-1; m_nMixStat /= nStat; }
    return max_samples - uncomputed_samples;;
}

/////////////////////////////////////////////////////////////////////////////
// Handles navigation/effects

BOOL module_renderer::ProcessRow()
//---------------------------
{
    if (++m_nTickCount >= m_nMusicSpeed * (m_nPatternDelay + 1) + m_nFrameDelay)
    {
        HandlePatternTransitionEvents();
        m_nPatternDelay = 0;
        m_nFrameDelay = 0;
        m_nTickCount = 0;
        m_nRow = m_nNextRow;
        // Reset Pattern Loop Effect
        if (m_nCurrentPattern != m_nNextPattern)
            m_nCurrentPattern = m_nNextPattern;
        // Check if pattern is valid
        if (!(m_dwSongFlags & SONG_PATTERNLOOP))
        {
            m_nPattern = (m_nCurrentPattern < Order.size()) ? Order[m_nCurrentPattern] : Order.GetInvalidPatIndex();
            if ((m_nPattern < Patterns.Size()) && (!Patterns[m_nPattern])) m_nPattern = Order.GetIgnoreIndex();
            while (m_nPattern >= Patterns.Size())
            {
                // End of song?
                if ((m_nPattern == Order.GetInvalidPatIndex()) || (m_nCurrentPattern >= Order.size()))
                {

                    //if (!m_nRepeatCount) return FALSE;

                    modplug::tracker::orderindex_t nRestartPosOverride = m_nRestartPos;
                    if(!m_nRestartPos && m_nCurrentPattern <= Order.size() && m_nCurrentPattern > 0)
                    {
                        /* Subtune detection. Subtunes are separated by "---" order items, so if we're in a
                           subtune and there's no restart position, we go to the first order of the subtune
                           (i.e. the first order after the previous "---" item) */
                        for(modplug::tracker::orderindex_t iOrd = m_nCurrentPattern - 1; iOrd > 0; iOrd--)
                        {
                            if(Order[iOrd] == Order.GetInvalidPatIndex())
                            {
                                // Jump back to first order of this subtune
                                nRestartPosOverride = iOrd + 1;
                                break;
                            }
                        }
                    }

                    // If channel resetting is disabled in MPT, we will emulate a pattern break (and we always do it if we're not in MPT)
#ifdef MODPLUG_TRACKER
                    if(CMainFrame::GetMainFrame() && !(CMainFrame::GetMainFrame()->m_dwPatternSetup & PATTERN_RESETCHANNELS))
#endif // MODPLUG_TRACKER
                    {
                        m_dwSongFlags |= SONG_BREAKTOROW;
                    }

                    if (!nRestartPosOverride && !(m_dwSongFlags & SONG_BREAKTOROW))
                    {
                        //rewbs.instroVSTi: stop all VSTi at end of song, if looping.
                        StopAllVsti();
                        m_nMusicSpeed = m_nDefaultSpeed;
                        m_nMusicTempo = m_nDefaultTempo;
                        m_nGlobalVolume = m_nDefaultGlobalVolume;
                        for (UINT i=0; i<MAX_VIRTUAL_CHANNELS; i++)
                        {
                            Chn[i].flags |= CHN_NOTEFADE | CHN_KEYOFF;
                            Chn[i].nFadeOutVol = 0;

                            if (i < m_nChannels)
                            {
                                Chn[i].nGlobalVol = ChnSettings[i].nVolume;
                                Chn[i].nVolume = ChnSettings[i].nVolume;
                                Chn[i].nPan = ChnSettings[i].nPan;
                                Chn[i].nPanSwing = Chn[i].nVolSwing = 0;
                                Chn[i].nCutSwing = Chn[i].nResSwing = 0;
                                Chn[i].nOldVolParam = 0;
                                Chn[i].nOldOffset = 0;
                                Chn[i].nOldHiOffset = 0;
                                Chn[i].nPortamentoDest = 0;

                                if (!Chn[i].length)
                                {
                                    Chn[i].flags = ChnSettings[i].dwFlags;
                                    Chn[i].loop_start = 0;
                                    Chn[i].loop_end = 0;
                                    Chn[i].instrument = nullptr;
                                    Chn[i].sample_data = nullptr;
                                    Chn[i].sample = nullptr;
                                }
                            }
                        }


                    }

                    //Handle Repeat position
                    if (m_nRepeatCount > 0) m_nRepeatCount--;
                    m_nCurrentPattern = nRestartPosOverride;
                    m_dwSongFlags &= ~SONG_BREAKTOROW;
                    //If restart pos points to +++, move along
                    while (Order[m_nCurrentPattern] == Order.GetIgnoreIndex())
                    {
                        m_nCurrentPattern++;
                    }
                    //Check for end of song or bad pattern
                    if (m_nCurrentPattern >= Order.size()
                        || (Order[m_nCurrentPattern] >= Patterns.Size())
                        || (!Patterns[Order[m_nCurrentPattern]]) )
                    {
                        InitializeVisitedRows(true);
                        return FALSE;
                    }

                } else
                {
                    m_nCurrentPattern++;
                }

                if (m_nCurrentPattern < Order.size())
                    m_nPattern = Order[m_nCurrentPattern];
                else
                    m_nPattern = Order.GetInvalidPatIndex();

                if ((m_nPattern < Patterns.Size()) && (!Patterns[m_nPattern]))
                    m_nPattern = Order.GetIgnoreIndex();
            }
            m_nNextPattern = m_nCurrentPattern;

#ifdef MODPLUG_TRACKER
            if ((m_nMaxOrderPosition) && (m_nCurrentPattern >= m_nMaxOrderPosition)) return FALSE;
#endif // MODPLUG_TRACKER
        }

        // Weird stuff?
        if (!Patterns.IsValidPat(m_nPattern)) return FALSE;
        // Should never happen
        if (m_nRow >= Patterns[m_nPattern].GetNumRows()) m_nRow = 0;

        // Has this row been visited before? We might want to stop playback now.
        // But: We will not mark the row as modified if the song is not in loop mode but
        // the pattern loop (editor flag, not to be confused with the pattern loop effect)
        // flag is set - because in that case, the module would stop after the first pattern loop...
        const bool overrideLoopCheck = (m_nRepeatCount != -1) && (m_dwSongFlags & SONG_PATTERNLOOP);
        if(!overrideLoopCheck && IsRowVisited(m_nCurrentPattern, m_nRow, true))
        {
            if(m_nRepeatCount)
            {
                // repeat count == -1 means repeat infinitely.
                if (m_nRepeatCount > 0)
                {
                    m_nRepeatCount--;
                }
                // Forget all but the current row.
                InitializeVisitedRows(true);
                SetRowVisited(m_nCurrentPattern, m_nRow);
            } else
            {
#ifdef MODPLUG_TRACKER
                // Let's check again if this really is the end of the song.
                // The visited rows vector might have been screwed up while editing...
                // This is of course not possible during rendering to WAV, so we ignore that case.
                GetLengthType t = GetLength(eNoAdjust);
                if((deprecated_global_sound_setup_bitmask & SNDMIX_DIRECTTODISK) || (t.lastOrder == m_nCurrentPattern && t.lastRow == m_nRow))
#else
                if(1)
#endif // MODPLUG_TRACKER
                {
                    // This is really the song's end!
                    InitializeVisitedRows(true);
                    return FALSE;
                } else
                {
                    // Ok, this is really dirty, but we have to update the visited rows vector...
                    GetLength(eAdjustOnSuccess, m_nCurrentPattern, m_nRow);
                }
            }
        }

        m_nNextRow = m_nRow + 1;
        if (m_nNextRow >= Patterns[m_nPattern].GetNumRows())
        {
            if (!(m_dwSongFlags & SONG_PATTERNLOOP)) m_nNextPattern = m_nCurrentPattern + 1;
            m_bPatternTransitionOccurred = true;
            m_nNextRow = 0;

            // FT2 idiosyncrasy: When E60 is used on a pattern row x, the following pattern also starts from row x
            // instead of the beginning of the pattern, unless there was a Dxx or Cxx effect.
            if(IsCompatibleMode(TRK_FASTTRACKER2))
            {
                m_nNextRow = m_nNextPatStartRow;
                m_nNextPatStartRow = 0;
            }
        }
        // Reset channel values
        modplug::tracker::modchannel_t *pChn = Chn;
        modplug::tracker::modevent_t *m = Patterns[m_nPattern] + m_nRow * m_nChannels;
        for (UINT nChn=0; nChn<m_nChannels; pChn++, nChn++, m++)
        {
            pChn->nRowNote = m->note;
            pChn->nRowInstr = m->instr;
            pChn->nRowVolCmd = m->volcmd;
            pChn->nRowVolume = m->vol;
            pChn->nRowCommand = m->command;
            pChn->nRowParam = m->param;

            pChn->left_volume = pChn->nNewLeftVol;
            pChn->right_volume = pChn->nNewRightVol;
            pChn->flags &= ~(CHN_PORTAMENTO | CHN_VIBRATO | CHN_TREMOLO | CHN_PANBRELLO);
            pChn->nCommand = 0;
            pChn->m_nPlugParamValueStep = 0;
        }

        // Now that we know which pattern we're on, we can update time signatures (global or pattern-specific)
        UpdateTimeSignature();

    }
    // Should we process tick0 effects?
    if (!m_nMusicSpeed) m_nMusicSpeed = 1;
    m_dwSongFlags |= SONG_FIRSTTICK;

    //End of row? stop pattern step (aka "play row").
#ifdef MODPLUG_TRACKER
    if (m_nTickCount >= m_nMusicSpeed * (m_nPatternDelay + 1) + m_nFrameDelay - 1)
    {
        if (m_dwSongFlags & SONG_STEP)
        {
            m_dwSongFlags &= ~SONG_STEP;
            m_dwSongFlags |= SONG_PAUSED;
        }
    }
#endif // MODPLUG_TRACKER

    if (m_nTickCount)
    {
        m_dwSongFlags &= ~SONG_FIRSTTICK;
        if ((!(m_nType & MOD_TYPE_XM)) && (m_nTickCount < m_nMusicSpeed * (1 + m_nPatternDelay)))
        {
            if (!(m_nTickCount % m_nMusicSpeed)) m_dwSongFlags |= SONG_FIRSTTICK;
        }
    }

    // Update Effects
    return ProcessEffects();
}


////////////////////////////////////////////////////////////////////////////////////////////
// Handles envelopes & mixer setup

BOOL module_renderer::ReadNote()
//-------------------------
{
#ifdef MODPLUG_TRACKER
    // Checking end of row ?
    if (m_dwSongFlags & SONG_PAUSED)
    {
        m_nTickCount = 0;
        if (!m_nMusicSpeed) m_nMusicSpeed = 6;
        if (!m_nMusicTempo) m_nMusicTempo = 125;
    } else
#endif // MODPLUG_TRACKER
    {
        if (!ProcessRow())
            return FALSE;
    }
    ////////////////////////////////////////////////////////////////////////////////////
    m_nTotalCount++;
    if (!m_nMusicTempo) return FALSE;

    switch(m_nTempoMode)
    {

        case tempo_mode_alternative:
            m_nBufferCount = deprecated_global_mixing_freq / m_nMusicTempo;
            break;

        case tempo_mode_modern:
            {
                double accurateBufferCount = (double)deprecated_global_mixing_freq * (60.0 / (double)m_nMusicTempo / ((double)m_nMusicSpeed * (double)m_nCurrentRowsPerBeat));
                m_nBufferCount = static_cast<int>(accurateBufferCount);
                m_dBufferDiff += accurateBufferCount-m_nBufferCount;
                //tick-to-tick tempo correction:
                if (m_dBufferDiff >= 1)
                {
                    m_nBufferCount++;
                    m_dBufferDiff--;
                } else if (m_dBufferDiff <= -1)
                {
                    m_nBufferCount--;
                    m_dBufferDiff++;
                }
                ASSERT(abs(m_dBufferDiff) < 1);
                break;
            }

        case tempo_mode_classic:
        default:
            m_nBufferCount = (deprecated_global_mixing_freq * 5 * m_nTempoFactor) / (m_nMusicTempo << 8);
    }

    m_nSamplesPerTick = m_nBufferCount; //rewbs.flu

    // Master Volume + Pre-Amplification / Attenuation setup
    uint32_t nMasterVol;
    {
        /*int nchn32 = 0;
        modplug::tracker::modchannel_t *current_vchan = Chn;
        for (UINT vchan_idx=0; vchan_idx<m_nChannels; vchan_idx++,current_vchan++)
        {
            //if(!(current_vchan->flags & CHN_MUTE))    //removed by rewbs: fix http://www.modplug.com/forum/viewtopic.php?t=3358
                nchn32++;
        }
        nchn32 = CLAMP(nchn32, 1, 31);*/
        int nchn32 = CLAMP(m_nChannels, 1, 31);

        uint32_t mastervol;

        if (m_pConfig->getUseGlobalPreAmp())
        {
            int realmastervol = m_nMasterVolume;
            if (realmastervol > 0x80) {
                //Attenuate global pre-amp depending on num channels
                realmastervol = 0x80 + ((realmastervol - 0x80) * (nchn32+4)) / 16;
            }
            mastervol = (realmastervol * (m_nSamplePreAmp)) >> 6;
        } else {
            //Preferred option: don't use global pre-amp at all.
            mastervol = m_nSamplePreAmp;
        }

        if ((m_dwSongFlags & SONG_GLOBALFADE) && (m_nGlobalFadeMaxSamples))
        {
            mastervol = _muldiv(mastervol, m_nGlobalFadeSamples, m_nGlobalFadeMaxSamples);
        }

        if (m_pConfig->getUseGlobalPreAmp()) {
            UINT attenuation = (deprecated_global_sound_setup_bitmask & SNDMIX_AGC) ? PreAmpAGCTable[nchn32>>1] : PreAmpTable[nchn32>>1];
            if(attenuation < 1) attenuation = 1;
            nMasterVol = (mastervol << 7) / attenuation;
        } else {
            nMasterVol = mastervol;
        }
    }
    ////////////////////////////////////////////////////////////////////////////////////
    // Update channels data
    m_nMixChannels = 0;
    modplug::tracker::modchannel_t *current_vchan = Chn;
    for (size_t vchan_idx = 0; vchan_idx < MAX_VIRTUAL_CHANNELS; vchan_idx++, current_vchan++)
    {
    skipchn:

        // XM Compatibility: Prevent notes to be stopped after a fadeout. This way, a portamento effect can pick up a faded instrument which is long enough.
        // This occours for example in the bassline (channel 11) of jt_burn.xm. I hope this won't break anything else...
        // I also suppose this could decrease mixing performance a bit, but hey, which CPU can't handle 32 muted channels these days... :-)
        if ((current_vchan->flags & CHN_NOTEFADE) && (!(current_vchan->nFadeOutVol|current_vchan->right_volume|current_vchan->left_volume)) && (!IsCompatibleMode(TRK_FASTTRACKER2)))
        {
            current_vchan->length = 0;
            current_vchan->nROfs = current_vchan->nLOfs = 0;
        }
        // Check for unused channel
        if (current_vchan->muted() || ((vchan_idx >= m_nChannels) && current_vchan->inactive()))
        {
            current_vchan->nVUMeter = 0;
#ifdef ENABLE_STEREOVU
            current_vchan->nLeftVU = current_vchan->nRightVU = 0;
#endif

            vchan_idx++;
            current_vchan++;
            if (vchan_idx >= m_nChannels)
            {
                while ((vchan_idx < MAX_VIRTUAL_CHANNELS) && current_vchan->inactive())
                {
                    vchan_idx++;
                    current_vchan++;
                }
            }
            if (vchan_idx < MAX_VIRTUAL_CHANNELS)
                goto skipchn;    // >:(
            break;
        }
        // Reset channel data
        current_vchan->position_delta = 0;
        current_vchan->nRealVolume = 0;

        if(GetModFlag(MSF_OLDVOLSWING))
        {
            current_vchan->nRealPan = current_vchan->nPan + current_vchan->nPanSwing;
        }
        else
        {
            current_vchan->nPan += current_vchan->nPanSwing;
            current_vchan->nPan = CLAMP(current_vchan->nPan, 0, 256);
            current_vchan->nPanSwing = 0;
            current_vchan->nRealPan = current_vchan->nPan;
        }

        current_vchan->nRealPan = CLAMP(current_vchan->nRealPan, 0, 256);
        current_vchan->nRampLength = 0;

        //Aux variables
        CTuning::RATIOTYPE vibratoFactor = 1;
        CTuning::NOTEINDEXTYPE arpeggioSteps = 0;

        modplug::tracker::modinstrument_t *pIns = current_vchan->instrument;

        // Calc Frequency
        if ((current_vchan->nPeriod)    && (current_vchan->length))
        {
            int vol = current_vchan->nVolume;

            if(current_vchan->nVolSwing)
            {
                if(GetModFlag(MSF_OLDVOLSWING))
                {
                    vol += current_vchan->nVolSwing;
                }
                else
                {
                    current_vchan->nVolume += current_vchan->nVolSwing;
                    current_vchan->nVolume = CLAMP(current_vchan->nVolume, 0, 256);
                    vol = current_vchan->nVolume;
                    current_vchan->nVolSwing = 0;
                }
            }

            vol = CLAMP(vol, 0, 256);

            // Tremolo
            if (current_vchan->flags & CHN_TREMOLO)
            {
                UINT trempos = current_vchan->nTremoloPos;
                // IT compatibility: Why would you not want to execute tremolo at volume 0?
                if (vol > 0 || IsCompatibleMode(TRK_IMPULSETRACKER))
                {
                    // IT compatibility: We don't need a different attenuation here because of the different tables we're going to use
                    const int tremattn = (m_nType & MOD_TYPE_XM || IsCompatibleMode(TRK_IMPULSETRACKER)) ? 5 : 6;
                    switch (current_vchan->nTremoloType & 0x03)
                    {
                    case 1:
                        // IT compatibility: IT has its own, more precise tables
                        vol += ((IsCompatibleMode(TRK_IMPULSETRACKER) ? ITRampDownTable[trempos] : ModRampDownTable[trempos]) * (int)current_vchan->nTremoloDepth) >> tremattn;
                        break;
                    case 2:
                        // IT compatibility: IT has its own, more precise tables
                        vol += ((IsCompatibleMode(TRK_IMPULSETRACKER) ? ITSquareTable[trempos] : ModSquareTable[trempos]) * (int)current_vchan->nTremoloDepth) >> tremattn;
                        break;
                    case 3:
                        //IT compatibility 19. Use random values
                        if(IsCompatibleMode(TRK_IMPULSETRACKER))
                            vol += (((rand() & 0x7F) - 0x40) * (int)current_vchan->nTremoloDepth) >> tremattn;
                        else
                            vol += (ModRandomTable[trempos] * (int)current_vchan->nTremoloDepth) >> tremattn;
                        break;
                    default:
                        // IT compatibility: IT has its own, more precise tables
                        vol += ((IsCompatibleMode(TRK_IMPULSETRACKER) ? ITSinusTable[trempos] : ModSinusTable[trempos]) * (int)current_vchan->nTremoloDepth) >> tremattn;
                    }
                }
                if ((m_nTickCount) || ((m_nType & (MOD_TYPE_STM|MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT)) && (!(m_dwSongFlags & SONG_ITOLDEFFECTS))))
                {
                    // IT compatibility: IT has its own, more precise tables
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        current_vchan->nTremoloPos = (current_vchan->nTremoloPos + 4 * current_vchan->nTremoloSpeed) & 0xFF;
                    else
                        current_vchan->nTremoloPos = (current_vchan->nTremoloPos + current_vchan->nTremoloSpeed) & 0x3F;
                }
            }

            // Tremor
            if(current_vchan->nCommand == CmdTremor)
            {
                // IT compatibility 12. / 13.: Tremor
                if(IsCompatibleMode(TRK_IMPULSETRACKER))
                {
                    if ((current_vchan->nTremorCount & 128) && current_vchan->length)
                    {
                        if (current_vchan->nTremorCount == 128)
                            current_vchan->nTremorCount = (current_vchan->nTremorParam >> 4) | 192;
                        else if (current_vchan->nTremorCount == 192)
                            current_vchan->nTremorCount = (current_vchan->nTremorParam & 0xf) | 128;
                        else
                            current_vchan->nTremorCount--;
                    }

                    if ((current_vchan->nTremorCount & 192) == 128)
                        vol = 0;
                }
                else
                {
                    UINT n = (current_vchan->nTremorParam >> 4) + (current_vchan->nTremorParam & 0x0F);
                    UINT ontime = current_vchan->nTremorParam >> 4;
                    if ((!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) || (m_dwSongFlags & SONG_ITOLDEFFECTS))
                    {
                        n += 2;
                        ontime++;
                    }
                    UINT tremcount = (UINT)current_vchan->nTremorCount;
                    if (tremcount >= n) tremcount = 0;
                    if ((m_nTickCount) || (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT)))
                    {
                        if (tremcount >= ontime) vol = 0;
                        current_vchan->nTremorCount = (uint8_t)(tremcount + 1);
                    }
                }
                current_vchan->flags |= CHN_FASTVOLRAMP;
            }

            // Clip volume and multiply
            vol = CLAMP(vol, 0, 256) << 6;

            // Process Envelopes
            if (pIns)
            {
                // Volume Envelope
                // IT Compatibility: S77 does not disable the volume envelope, it just pauses the counter
                // Problem: This pauses on the wrong tick at the moment...
                if (((current_vchan->flags & CHN_VOLENV) || ((pIns->volume_envelope.flags & ENV_ENABLED) && IsCompatibleMode(TRK_IMPULSETRACKER))) && (pIns->volume_envelope.num_nodes))
                {
                    int envvol = GetVolEnvValueFromPosition(current_vchan->volume_envelope.position, pIns);

                    // if we are in the release portion of the envelope,
                    // rescale envelope factor so that it is proportional to the release point
                    // and release envelope beginning.
                    if (pIns->volume_envelope.release_node != ENV_RELEASE_NODE_UNSET
                        && current_vchan->volume_envelope.position>=pIns->volume_envelope.Ticks[pIns->volume_envelope.release_node]
                        && current_vchan->volume_envelope.release_value != NOT_YET_RELEASED)
                    {
                        int envValueAtReleaseJump = current_vchan->volume_envelope.release_value;
                        int envValueAtReleaseNode = pIns->volume_envelope.Values[pIns->volume_envelope.release_node] << 2;

                        //If we have just hit the release node, force the current env value
                        //to be that of the release node. This works around the case where
                        // we have another node at the same position as the release node.
                        if (current_vchan->volume_envelope.position == pIns->volume_envelope.Ticks[pIns->volume_envelope.release_node])
                            envvol = envValueAtReleaseNode;

                        int relativeVolumeChange = (envvol - envValueAtReleaseNode) * 2;
                        envvol = envValueAtReleaseJump + relativeVolumeChange;
                    }
                    vol = (vol * CLAMP(envvol, 0, 512)) >> 8;
                }
                // Panning Envelope
                // IT Compatibility: S79 does not disable the panning envelope, it just pauses the counter
                // Problem: This pauses on the wrong tick at the moment...
                if (((current_vchan->flags & CHN_PANENV) || ((pIns->panning_envelope.flags & ENV_ENABLED) && IsCompatibleMode(TRK_IMPULSETRACKER))) && (pIns->panning_envelope.num_nodes))
                {
                    int envpos = current_vchan->panning_envelope.position;
                    UINT pt = pIns->panning_envelope.num_nodes - 1;
                    for (UINT i=0; i<(UINT)(pIns->panning_envelope.num_nodes-1); i++)
                    {
                        if (envpos <= pIns->panning_envelope.Ticks[i])
                        {
                            pt = i;
                            break;
                        }
                    }
                    int x2 = pIns->panning_envelope.Ticks[pt], y2 = pIns->panning_envelope.Values[pt];
                    int x1, envpan;
                    if (envpos >= x2)
                    {
                        envpan = y2;
                        x1 = x2;
                    } else
                    if (pt)
                    {
                        envpan = pIns->panning_envelope.Values[pt-1];
                        x1 = pIns->panning_envelope.Ticks[pt-1];
                    } else
                    {
                        envpan = 128;
                        x1 = 0;
                    }
                    if ((x2 > x1) && (envpos > x1))
                    {
                        envpan += ((envpos - x1) * (y2 - envpan)) / (x2 - x1);
                    }

                    envpan = CLAMP(envpan, 0, 64);
                    int pan = current_vchan->nPan;
                    if (pan >= 128)
                    {
                        pan += ((envpan - 32) * (256 - pan)) / 32;
                    } else
                    {
                        pan += ((envpan - 32) * (pan)) / 32;
                    }

                    current_vchan->nRealPan = CLAMP(pan, 0, 256);
                }
                // FadeOut volume
                if (current_vchan->flags & CHN_NOTEFADE)
                {
                    UINT fadeout = pIns->fadeout;
                    if (fadeout)
                    {
                        current_vchan->nFadeOutVol -= fadeout << 1;
                        if (current_vchan->nFadeOutVol <= 0) current_vchan->nFadeOutVol = 0;
                        vol = (vol * current_vchan->nFadeOutVol) >> 16;
                    } else
                    if (!current_vchan->nFadeOutVol)
                    {
                        vol = 0;
                    }
                }
                // Pitch/Pan separation
                if ((pIns->pitch_pan_separation) && (current_vchan->nRealPan) && (current_vchan->nNote))
                {
                    // PPS value is 1/512, i.e. PPS=1 will adjust by 8/512 = 1/64 for each 8 semitones
                    // with PPS = 32 / PPC = C-5, E-6 will pan hard right (and D#6 will not)
                    // IT compatibility: IT has a wider pan range here
                    int pandelta = (int)current_vchan->nRealPan + (int)((int)(current_vchan->nNote - pIns->pitch_pan_center - 1) * (int)pIns->pitch_pan_separation) / (int)(IsCompatibleMode(TRK_IMPULSETRACKER) ? 4 : 8);
                    current_vchan->nRealPan = CLAMP(pandelta, 0, 256);
                }
            } else
            {
                // No Envelope: key off => note cut
                if (current_vchan->flags & CHN_NOTEFADE) // 1.41-: CHN_KEYOFF|CHN_NOTEFADE
                {
                    current_vchan->nFadeOutVol = 0;
                    vol = 0;
                }
            }
            // vol is 14-bits
            if (vol)
            {
                // IMPORTANT: current_vchan->nRealVolume is 14 bits !!!
                // -> _muldiv( 14+8, 6+6, 18); => RealVolume: 14-bit result (22+12-20)

                //UINT nPlugin = GetBestPlugin(vchan_idx, PRIORITISE_INSTRUMENT, RESPECT_MUTES);

                //Don't let global volume affect level of sample if
                //global volume is going to be applied to master output anyway.
                if (current_vchan->flags & CHN_SYNCMUTE)
                {
                    current_vchan->nRealVolume = 0;
                } else if (m_pConfig->getGlobalVolumeAppliesToMaster())
                {
                    current_vchan->nRealVolume = _muldiv(vol * MAX_GLOBAL_VOLUME, current_vchan->nGlobalVol * current_vchan->nInsVol, 1 << 20);
                } else
                {
                    current_vchan->nRealVolume = _muldiv(vol * m_nGlobalVolume, current_vchan->nGlobalVol * current_vchan->nInsVol, 1 << 20);
                }
            }
            if (current_vchan->nPeriod < m_nMinPeriod) current_vchan->nPeriod = m_nMinPeriod;
            int period = current_vchan->nPeriod;
            if ((current_vchan->flags & (CHN_GLISSANDO|CHN_PORTAMENTO)) ==    (CHN_GLISSANDO|CHN_PORTAMENTO))
            {
                period = GetPeriodFromNote(GetNoteFromPeriod(period), current_vchan->nFineTune, current_vchan->nC5Speed);
            }

            // Arpeggio ?
            if (current_vchan->nCommand == CmdArpeggio)
            {
                if(m_nType == MOD_TYPE_MPT && current_vchan->instrument && current_vchan->instrument->pTuning)
                {
                    switch(m_nTickCount % 3)
                    {
                        case 0:
                            arpeggioSteps = 0;
                            break;
                        case 1:
                            arpeggioSteps = current_vchan->nArpeggio >> 4; // >> 4 <-> division by 16. This gives the first number in the parameter.
                            break;
                        case 2:
                            arpeggioSteps = current_vchan->nArpeggio % 16; //Gives the latter number in the parameter.
                            break;
                    }
                    current_vchan->m_CalculateFreq = true;
                    current_vchan->m_ReCalculateFreqOnFirstTick = true;
                }
                else
                {
                    //IT playback compatibility 01 & 02
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                    {
                        if(current_vchan->nArpeggio >> 4 != 0 || (current_vchan->nArpeggio & 0x0F) != 0)
                        {
                            switch(m_nTickCount % 3)
                            {
                                case 1:    period = period / TwoToPowerXOver12(current_vchan->nArpeggio >> 4); break;
                                case 2:    period = period / TwoToPowerXOver12(current_vchan->nArpeggio & 0x0F); break;
                            }
                        }
                    }
                    // FastTracker 2: Swedish tracker logic (TM) arpeggio
                    else if(IsCompatibleMode(TRK_FASTTRACKER2))
                    {
                        uint8_t note = current_vchan->nNote;
                        int arpPos = 0;

                        if (!(m_dwSongFlags & SONG_FIRSTTICK))
                        {
                            arpPos = ((int)m_nMusicSpeed - (int)m_nTickCount) % 3;
                            if((m_nMusicSpeed > 18) && (m_nMusicSpeed - m_nTickCount > 16)) arpPos = 2; // swedish tracker logic, I love it
                            switch(arpPos)
                            {
                                case 1:    note += (current_vchan->nArpeggio >> 4); break;
                                case 2:    note += (current_vchan->nArpeggio & 0x0F); break;
                            }
                        }

                        if (note > 109 && arpPos != 0)
                            note = 109; // FT2's note limit

                        period = GetPeriodFromNote(note, current_vchan->nFineTune, current_vchan->nC5Speed);

                    }
                    // Other trackers
                    else
                    {
                        switch(m_nTickCount % 3)
                        {
                            case 1:    period = GetPeriodFromNote(current_vchan->nNote + (current_vchan->nArpeggio >> 4), current_vchan->nFineTune, current_vchan->nC5Speed); break;
                            case 2:    period = GetPeriodFromNote(current_vchan->nNote + (current_vchan->nArpeggio & 0x0F), current_vchan->nFineTune, current_vchan->nC5Speed); break;
                        }
                    }
                }
            }

            // Preserve Amiga freq limits
            if (m_dwSongFlags & (SONG_AMIGALIMITS|SONG_PT1XMODE))
                period = CLAMP(period, 113 * 4, 856 * 4);

            // Pitch/Filter Envelope
            // IT Compatibility: S7B does not disable the pitch envelope, it just pauses the counter
            // Problem: This pauses on the wrong tick at the moment...
            if ((pIns) && ((current_vchan->flags & CHN_PITCHENV) || ((pIns->pitch_envelope.flags & ENV_ENABLED) && IsCompatibleMode(TRK_IMPULSETRACKER))) && (current_vchan->instrument->pitch_envelope.num_nodes))
            {
                int envpos = current_vchan->pitch_envelope.position;
                UINT pt = pIns->pitch_envelope.num_nodes - 1;
                for (UINT i=0; i<(UINT)(pIns->pitch_envelope.num_nodes-1); i++)
                {
                    if (envpos <= pIns->pitch_envelope.Ticks[i])
                    {
                        pt = i;
                        break;
                    }
                }
                int x2 = pIns->pitch_envelope.Ticks[pt];
                int x1, envpitch;
                if (envpos >= x2)
                {
                    envpitch = (((int)pIns->pitch_envelope.Values[pt]) - ENVELOPE_MID) * 8;
                    x1 = x2;
                } else
                if (pt)
                {
                    envpitch = (((int)pIns->pitch_envelope.Values[pt-1]) - ENVELOPE_MID) * 8;
                    x1 = pIns->pitch_envelope.Ticks[pt-1];
                } else
                {
                    envpitch = 0;
                    x1 = 0;
                }
                if (envpos > x2) envpos = x2;
                if ((x2 > x1) && (envpos > x1))
                {
                    int envpitchdest = (((int)pIns->pitch_envelope.Values[pt]) - 32) * 8;
                    envpitch += ((envpos - x1) * (envpitchdest - envpitch)) / (x2 - x1);
                }
                envpitch = CLAMP(envpitch, -256, 256);
                // Filter Envelope: controls cutoff frequency
                //if (pIns->pitch_envelope.flags & ENV_FILTER)
                if (current_vchan->flags & CHN_FILTERENV)
                {
#ifndef NO_FILTER
                    SetupChannelFilter(current_vchan, (current_vchan->flags & CHN_FILTER) ? false : true, envpitch);
#endif // NO_FILTER
                } else
                // Pitch Envelope
                {
                    if(m_nType == MOD_TYPE_MPT && current_vchan->instrument && current_vchan->instrument->pTuning)
                    {
                        if(current_vchan->nFineTune != envpitch)
                        {
                            current_vchan->nFineTune = envpitch;
                            current_vchan->m_CalculateFreq = true;
                            //Preliminary tests indicated that this behavior
                            //is very close to original(with 12TET) when finestep count
                            //is 15.
                        }
                    }
                    else //Original behavior
                    {
                        int l = envpitch;
                        if (l < 0)
                        {
                            l = -l;
                            if (l > 255) l = 255;
                            period = _muldiv(period, LinearSlideUpTable[l], 0x10000);
                        } else
                        {
                            if (l > 255) l = 255;
                            period = _muldiv(period, LinearSlideDownTable[l], 0x10000);
                        }
                    } //End: Original behavior.
                }
            }

            // Vibrato
            if (current_vchan->flags & CHN_VIBRATO)
            {
                UINT vibpos = current_vchan->nVibratoPos;
                LONG vdelta;
                switch (current_vchan->nVibratoType & 0x03)
                {
                case 1:
                    // IT compatibility: IT has its own, more precise tables
                    vdelta = IsCompatibleMode(TRK_IMPULSETRACKER) ? ITRampDownTable[vibpos] : ModRampDownTable[vibpos];
                    break;
                case 2:
                    // IT compatibility: IT has its own, more precise tables
                    vdelta = IsCompatibleMode(TRK_IMPULSETRACKER) ? ITSquareTable[vibpos] : ModSquareTable[vibpos];
                    break;
                case 3:
                    //IT compatibility 19. Use random values
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        vdelta = (rand() & 0x7F) - 0x40;
                    else
                        vdelta = ModRandomTable[vibpos];
                    break;
                default:
                    // IT compatibility: IT has its own, more precise tables
                    vdelta = IsCompatibleMode(TRK_IMPULSETRACKER) ? ITSinusTable[vibpos] : ModSinusTable[vibpos];
                }

                if(m_nType == MOD_TYPE_MPT && current_vchan->instrument && current_vchan->instrument->pTuning)
                {
                    //Hack implementation: Scaling vibratofactor to [0.95; 1.05]
                    //using figure from above tables and vibratodepth parameter
                    vibratoFactor += 0.05F * vdelta * current_vchan->m_VibratoDepth / 128.0F;
                    current_vchan->m_CalculateFreq = true;
                    current_vchan->m_ReCalculateFreqOnFirstTick = false;

                    if(m_nTickCount + 1 == m_nMusicSpeed)
                        current_vchan->m_ReCalculateFreqOnFirstTick = true;
                }
                else //Original behavior
                {
                    UINT vdepth;
                    // IT compatibility: correct vibrato depth
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                    {
                        // Yes, vibrato goes backwards with old effects enabled!
                        if(m_dwSongFlags & SONG_ITOLDEFFECTS)
                        {
                            vdepth = 5;
                            vdelta = -vdelta;
                        } else
                        {
                            vdepth = 6;
                        }
                    }
                    else
                    {
                        vdepth = ((!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) || (m_dwSongFlags & SONG_ITOLDEFFECTS)) ? 6 : 7;
                    }
                    vdelta = (vdelta * (int)current_vchan->nVibratoDepth) >> vdepth;
                    if ((m_dwSongFlags & SONG_LINEARSLIDES) && (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT)))
                    {
                        LONG l = vdelta;
                        if (l < 0)
                        {
                            l = -l;
                            vdelta = _muldiv(period, LinearSlideDownTable[l >> 2], 0x10000) - period;
                            if (l & 0x03) vdelta += _muldiv(period, FineLinearSlideDownTable[l & 0x03], 0x10000) - period;
                        } else
                        {
                            vdelta = _muldiv(period, LinearSlideUpTable[l >> 2], 0x10000) - period;
                            if (l & 0x03) vdelta += _muldiv(period, FineLinearSlideUpTable[l & 0x03], 0x10000) - period;
                        }
                    }
                    period += vdelta;
                }
                if ((m_nTickCount) || ((m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT)) && (!(m_dwSongFlags & SONG_ITOLDEFFECTS))))
                {
                    // IT compatibility: IT has its own, more precise tables
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        current_vchan->nVibratoPos = (vibpos + 4 * current_vchan->nVibratoSpeed) & 0xFF;
                    else
                        current_vchan->nVibratoPos = (vibpos + current_vchan->nVibratoSpeed) & 0x3F;
                }
            }
            // Panbrello
            if (current_vchan->flags & CHN_PANBRELLO)
            {
                UINT panpos;
                // IT compatibility: IT has its own, more precise tables
                if(IsCompatibleMode(TRK_IMPULSETRACKER))
                    panpos = current_vchan->nPanbrelloPos & 0xFF;
                else
                    panpos = ((current_vchan->nPanbrelloPos + 0x10) >> 2) & 0x3F;
                LONG pdelta;
                switch (current_vchan->nPanbrelloType & 0x03)
                {
                case 1:
                    // IT compatibility: IT has its own, more precise tables
                    pdelta = IsCompatibleMode(TRK_IMPULSETRACKER) ? ITRampDownTable[panpos] : ModRampDownTable[panpos];
                    break;
                case 2:
                    // IT compatibility: IT has its own, more precise tables
                    pdelta = IsCompatibleMode(TRK_IMPULSETRACKER) ? ITSquareTable[panpos] : ModSquareTable[panpos];
                    break;
                case 3:
                    //IT compatibility 19. Use random values
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        pdelta = (rand() & 0x7f) - 0x40;
                    else
                        pdelta = ModRandomTable[panpos];
                    break;
                default:
                    // IT compatibility: IT has its own, more precise tables
                    pdelta = IsCompatibleMode(TRK_IMPULSETRACKER) ? ITSinusTable[panpos] : ModSinusTable[panpos];
                }
                current_vchan->nPanbrelloPos += current_vchan->nPanbrelloSpeed;
                pdelta = ((pdelta * (int)current_vchan->nPanbrelloDepth) + 2) >> 3;
                pdelta += current_vchan->nRealPan;

                current_vchan->nRealPan = CLAMP(pdelta, 0, 256);
                //if(IsCompatibleMode(TRK_IMPULSETRACKER)) current_vchan->nPan = current_vchan->nRealPan; // TODO
            }
            int nPeriodFrac = 0;
            // Sample Auto-Vibrato
            if ((current_vchan->sample) && (current_vchan->sample->vibrato_depth))
            {
                modplug::tracker::modsample_t *pSmp = current_vchan->sample;
                const bool alternativeTuning = current_vchan->instrument && current_vchan->instrument->pTuning;

                // IT compatibility: Autovibrato is so much different in IT that I just put this in a separate code block, to get rid of a dozen IsCompatibilityMode() calls.
                if(IsCompatibleMode(TRK_IMPULSETRACKER) && !alternativeTuning)
                {
                    // Schism's autovibrato code

                    /*
                    X86 Assembler from ITTECH.TXT:
                    1) Mov AX, [SomeVariableNameRelatingToVibrato]
                    2) Add AL, Rate
                    3) AdC AH, 0
                    4) AH contains the depth of the vibrato as a fine-linear slide.
                    5) Mov [SomeVariableNameRelatingToVibrato], AX  ; For the next cycle.
                    */
                    const int vibpos = current_vchan->nAutoVibPos & 0xFF;
                    int adepth = current_vchan->nAutoVibDepth; // (1)
                    adepth += pSmp->vibrato_sweep & 0xFF; // (2 & 3)
                    adepth = min(adepth, (int)(pSmp->vibrato_depth << 8));
                    current_vchan->nAutoVibDepth = adepth; // (5)
                    adepth >>= 8; // (4)

                    current_vchan->nAutoVibPos += pSmp->vibrato_rate;

                    int vdelta;
                    switch(pSmp->vibrato_type)
                    {
                    case VIB_RANDOM:
                        vdelta = (rand() & 0x7F) - 0x40;
                        break;
                    case VIB_RAMP_DOWN:
                        vdelta = ITRampDownTable[vibpos];
                        break;
                    case VIB_RAMP_UP:
                        vdelta = -ITRampDownTable[vibpos];
                        break;
                    case VIB_SQUARE:
                        vdelta = ITSquareTable[vibpos];
                        break;
                    case VIB_SINE:
                    default:
                        vdelta = ITSinusTable[vibpos];
                        break;
                    }

                    vdelta = (vdelta * adepth) >> 6;
                    int l = abs(vdelta);
                    if(vdelta < 0)
                    {
                        vdelta = _muldiv(period, LinearSlideDownTable[l >> 2], 0x10000) - period;
                        if (l & 0x03)
                        {
                            vdelta += _muldiv(period, FineLinearSlideDownTable[l & 0x03], 0x10000) - period;
                        }
                    } else
                    {
                        vdelta = _muldiv(period, LinearSlideUpTable[l >> 2], 0x10000) - period;
                        if (l & 0x03)
                        {
                            vdelta += _muldiv(period, FineLinearSlideUpTable[l & 0x03], 0x10000) - period;
                        }
                    }
                    period -= vdelta;

                } else
                {
                    // MPT's autovibrato code
                    if (pSmp->vibrato_sweep == 0)
                    {
                        current_vchan->nAutoVibDepth = pSmp->vibrato_depth << 8;
                    } else
                    {
                        // Calculate current autovibrato depth using vibsweep
                        if (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT))
                        {
                            // Note: changed bitshift from 3 to 1 as the variable is not divided by 4 in the IT loader anymore
                            // - so we divide sweep by 4 here.
                            current_vchan->nAutoVibDepth += pSmp->vibrato_sweep << 1;
                        } else
                        {
                            if (!(current_vchan->flags & CHN_KEYOFF))
                            {
                                current_vchan->nAutoVibDepth += (pSmp->vibrato_depth << 8) /    pSmp->vibrato_sweep;
                            }
                        }
                        if ((current_vchan->nAutoVibDepth >> 8) > pSmp->vibrato_depth)
                            current_vchan->nAutoVibDepth = pSmp->vibrato_depth << 8;
                    }
                    current_vchan->nAutoVibPos += pSmp->vibrato_rate;
                    int vdelta;
                    switch(pSmp->vibrato_type)
                    {
                    case VIB_RANDOM:
                        vdelta = ModRandomTable[current_vchan->nAutoVibPos & 0x3F];
                        current_vchan->nAutoVibPos++;
                        break;
                    case VIB_RAMP_DOWN:
                        vdelta = ((0x40 - (current_vchan->nAutoVibPos >> 1)) & 0x7F) - 0x40;
                        break;
                    case VIB_RAMP_UP:
                        vdelta = ((0x40 + (current_vchan->nAutoVibPos >> 1)) & 0x7f) - 0x40;
                        break;
                    case VIB_SQUARE:
                        vdelta = (current_vchan->nAutoVibPos & 128) ? +64 : -64;
                        break;
                    case VIB_SINE:
                    default:
                        vdelta = ft2VibratoTable[current_vchan->nAutoVibPos & 0xFF];
                    }
                    int n;
                    n =    ((vdelta * current_vchan->nAutoVibDepth) >> 8);

                    if(alternativeTuning)
                    {
                        //Vib sweep is not taken into account here.
                        vibratoFactor += 0.05F * pSmp->vibrato_depth * vdelta / 4096.0F; //4096 == 64^2
                        //See vibrato for explanation.
                        current_vchan->m_CalculateFreq = true;
                        /*
                        Finestep vibrato:
                        const float autoVibDepth = pSmp->vibrato_depth * val / 4096.0F; //4096 == 64^2
                        vibratoFineSteps += static_cast<CTuning::FINESTEPTYPE>(current_vchan->instrument->pTuning->GetFineStepCount() *  autoVibDepth);
                        current_vchan->m_CalculateFreq = true;
                        */
                    }
                    else //Original behavior
                    {
                        if (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT))
                        {
                            int df1, df2;
                            if (n < 0)
                            {
                                n = -n;
                                UINT n1 = n >> 8;
                                df1 = LinearSlideUpTable[n1];
                                df2 = LinearSlideUpTable[n1+1];
                            } else
                            {
                                UINT n1 = n >> 8;
                                df1 = LinearSlideDownTable[n1];
                                df2 = LinearSlideDownTable[n1+1];
                            }
                            n >>= 2;
                            period = _muldiv(period, df1 + ((df2 - df1) * (n & 0x3F) >> 6), 256);
                            nPeriodFrac = period & 0xFF;
                            period >>= 8;
                        } else
                        {
                            period += (n >> 6);
                        }
                    } //Original MPT behavior
                }

            } //End: AutoVibrato
            // Final Period
            if (period <= m_nMinPeriod)
            {
                if (m_nType & MOD_TYPE_S3M) current_vchan->length = 0;
                period = m_nMinPeriod;
            }
            //rewbs: temporarily commenting out block to allow notes below A-0.
            /*if (period > m_nMaxPeriod)
            {
                if ((m_nType & MOD_TYPE_IT) || (period >= 0x100000))
                {
                    current_vchan->nFadeOutVol = 0;
                    current_vchan->flags |= CHN_NOTEFADE;
                    current_vchan->nRealVolume = 0;
                }
                period = m_nMaxPeriod;
                nPeriodFrac = 0;
            }*/
            UINT freq = 0;

            if(m_nType != MOD_TYPE_MPT || !pIns || pIns->pTuning == nullptr)
            {
                freq = GetFreqFromPeriod(period, current_vchan->nC5Speed, nPeriodFrac);
            }
            else //In this case: m_nType == MOD_TYPE_MPT and using custom tunings.
            {
                if(current_vchan->m_CalculateFreq || (current_vchan->m_ReCalculateFreqOnFirstTick && m_nTickCount == 0))
                {
                    current_vchan->m_Freq = current_vchan->nC5Speed * vibratoFactor * pIns->pTuning->GetRatio(current_vchan->nNote - NoteMiddleC + arpeggioSteps, current_vchan->nFineTune+current_vchan->m_PortamentoFineSteps);
                    if(!current_vchan->m_CalculateFreq)
                        current_vchan->m_ReCalculateFreqOnFirstTick = false;
                    else
                        current_vchan->m_CalculateFreq = false;
                }

                freq = current_vchan->m_Freq;
            }

            //Applying Pitch/Tempo lock.
            if(m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT) && pIns && pIns->pitch_to_tempo_lock)
                freq *= (float)m_nMusicTempo / (float)pIns->pitch_to_tempo_lock;


            if ((m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT)) && (freq < 256))
            {
                current_vchan->nFadeOutVol = 0;
                current_vchan->flags |= CHN_NOTEFADE;
                current_vchan->nRealVolume = 0;
            }

            UINT ninc = _muldiv(freq, 0x10000, deprecated_global_mixing_freq);
            if ((ninc >= 0xFFB0) && (ninc <= 0x10090)) ninc = 0x10000;
            if (m_nFreqFactor != 128) ninc = (ninc * m_nFreqFactor) >> 7;
            if (ninc > 0xFF0000) ninc = 0xFF0000;
            current_vchan->position_delta = (ninc+1) & ~3;
        }

        // Increment envelope position
        if (pIns)
        {
            // Volume Envelope
            if (current_vchan->flags & CHN_VOLENV)
            {
                // Increase position
                current_vchan->volume_envelope.position++;
                // Volume Loop ?
                if (pIns->volume_envelope.flags & ENV_LOOP)
                {
                    UINT volloopend = pIns->volume_envelope.Ticks[pIns->volume_envelope.loop_end];
                    if (m_nType != MOD_TYPE_XM) volloopend++;
                    if (current_vchan->volume_envelope.position == volloopend)
                    {
                        current_vchan->volume_envelope.position = pIns->volume_envelope.Ticks[pIns->volume_envelope.loop_start];
                        if ((pIns->volume_envelope.loop_end == pIns->volume_envelope.loop_start) && (!pIns->volume_envelope.Values[pIns->volume_envelope.loop_start])
                         && ((!(m_nType & MOD_TYPE_XM)) || (pIns->volume_envelope.loop_end+1 == (int)pIns->volume_envelope.num_nodes)))
                        {
                            current_vchan->flags |= CHN_NOTEFADE;
                            current_vchan->nFadeOutVol = 0;
                        }
                    }
                }
                // Volume Sustain ?
                if ((pIns->volume_envelope.flags & ENV_SUSTAIN) && (!(current_vchan->flags & CHN_KEYOFF)))
                {
                    if (current_vchan->volume_envelope.position == (UINT)pIns->volume_envelope.Ticks[pIns->volume_envelope.sustain_end] + 1)
                        current_vchan->volume_envelope.position = pIns->volume_envelope.Ticks[pIns->volume_envelope.sustain_start];
                } else
                // End of Envelope ?
                if (current_vchan->volume_envelope.position > pIns->volume_envelope.Ticks[pIns->volume_envelope.num_nodes - 1])
                {
                    if ((m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT)) || (current_vchan->flags & CHN_KEYOFF)) current_vchan->flags |= CHN_NOTEFADE;
                    current_vchan->volume_envelope.position = pIns->volume_envelope.Ticks[pIns->volume_envelope.num_nodes - 1];
                    if ((!pIns->volume_envelope.Values[pIns->volume_envelope.num_nodes-1]) && ((vchan_idx >= m_nChannels) || (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT))))
                    {
                        current_vchan->flags |= CHN_NOTEFADE;
                        current_vchan->nFadeOutVol = 0;
                        current_vchan->nRealVolume = 0;
                    }
                }
            }
            // Panning Envelope
            if (current_vchan->flags & CHN_PANENV)
            {
                current_vchan->panning_envelope.position++;
                if (pIns->panning_envelope.flags & ENV_LOOP)
                {
                    UINT panloopend = pIns->panning_envelope.Ticks[pIns->panning_envelope.loop_end];
                    if (m_nType != MOD_TYPE_XM) panloopend++;
                    if (current_vchan->panning_envelope.position == panloopend)
                        current_vchan->panning_envelope.position = pIns->panning_envelope.Ticks[pIns->panning_envelope.loop_start];
                }
                // Panning Sustain ?
                if ((pIns->panning_envelope.flags & ENV_SUSTAIN) && (current_vchan->panning_envelope.position == (UINT)pIns->panning_envelope.Ticks[pIns->panning_envelope.sustain_end]+1)
                 && (!(current_vchan->flags & CHN_KEYOFF)))
                {
                    // Panning sustained
                    current_vchan->panning_envelope.position = pIns->panning_envelope.Ticks[pIns->panning_envelope.sustain_start];
                } else
                {
                    if (current_vchan->panning_envelope.position > pIns->panning_envelope.Ticks[pIns->panning_envelope.num_nodes - 1])
                        current_vchan->panning_envelope.position = pIns->panning_envelope.Ticks[pIns->panning_envelope.num_nodes - 1];
                }
            }
            // Pitch Envelope
            if (current_vchan->flags & CHN_PITCHENV)
            {
                // Increase position
                current_vchan->pitch_envelope.position++;
                // Pitch Loop ?
                if (pIns->pitch_envelope.flags & ENV_LOOP)
                {
                    UINT pitchloopend = pIns->pitch_envelope.Ticks[pIns->pitch_envelope.loop_end];
                    //IT compatibility 24. Short envelope loops
                    if (IsCompatibleMode(TRK_IMPULSETRACKER)) pitchloopend++;
                    if (current_vchan->pitch_envelope.position >= pitchloopend)
                        current_vchan->pitch_envelope.position = pIns->pitch_envelope.Ticks[pIns->pitch_envelope.loop_start];
                }
                // Pitch Sustain ?
                if ((pIns->pitch_envelope.flags & ENV_SUSTAIN) && (!(current_vchan->flags & CHN_KEYOFF)))
                {
                    if (current_vchan->pitch_envelope.position == (UINT)pIns->pitch_envelope.Ticks[pIns->pitch_envelope.sustain_end]+1)
                        current_vchan->pitch_envelope.position = pIns->pitch_envelope.Ticks[pIns->pitch_envelope.sustain_start];
                } else
                {
                    if (current_vchan->pitch_envelope.position > pIns->pitch_envelope.Ticks[pIns->pitch_envelope.num_nodes - 1])
                        current_vchan->pitch_envelope.position = pIns->pitch_envelope.Ticks[pIns->pitch_envelope.num_nodes - 1];
                }
            }
        }

        // Volume ramping
        current_vchan->flags &= ~CHN_VOLUMERAMP;
        if ((current_vchan->nRealVolume) || (current_vchan->left_volume) || (current_vchan->right_volume))
            current_vchan->flags |= CHN_VOLUMERAMP;
#ifdef ENABLE_STEREOVU
        if (current_vchan->nLeftVU > VUMETER_DECAY) current_vchan->nLeftVU -= VUMETER_DECAY; else current_vchan->nLeftVU = 0;
        if (current_vchan->nRightVU > VUMETER_DECAY) current_vchan->nRightVU -= VUMETER_DECAY; else current_vchan->nRightVU = 0;
#endif
        // Check for too big position_delta
        if (((current_vchan->position_delta >> 16) + 1) >= (LONG)(current_vchan->loop_end - current_vchan->loop_start)) current_vchan->flags &= ~CHN_LOOP;
        current_vchan->nNewRightVol = current_vchan->nNewLeftVol = 0;
        current_vchan->active_sample_data = ((current_vchan->sample_data) && (current_vchan->length) && (current_vchan->position_delta)) ? current_vchan->sample_data : NULL;
        if (current_vchan->active_sample_data)
        {
            // Update VU-Meter (nRealVolume is 14-bit)
#ifdef ENABLE_STEREOVU
            UINT vul = (current_vchan->nRealVolume * current_vchan->nRealPan) >> 14;
            if (vul > 127) vul = 127;
            if (current_vchan->nLeftVU > 127) current_vchan->nLeftVU = (uint8_t)vul;
            vul >>= 1;
            if (current_vchan->nLeftVU < vul) current_vchan->nLeftVU = (uint8_t)vul;
            UINT vur = (current_vchan->nRealVolume * (256-current_vchan->nRealPan)) >> 14;
            if (vur > 127) vur = 127;
            if (current_vchan->nRightVU > 127) current_vchan->nRightVU = (uint8_t)vur;
            vur >>= 1;
            if (current_vchan->nRightVU < vur) current_vchan->nRightVU = (uint8_t)vur;
#endif
            UINT kChnMasterVol = (current_vchan->flags & CHN_EXTRALOUD) ? 0x100 : nMasterVol;
            // Adjusting volumes
            if (deprecated_global_channels >= 2)
            {
                int pan = ((int)current_vchan->nRealPan) - 128;
                pan *= (int)m_nStereoSeparation;
                pan /= 128;
                pan += 128;
                pan = CLAMP(pan, 0, 256);
#ifndef FASTSOUNDLIB
                if (deprecated_global_sound_setup_bitmask & SNDMIX_REVERSESTEREO) pan = 256 - pan;
#endif

                LONG realvol;
                if (m_pConfig->getUseGlobalPreAmp())
                {
                    realvol = (current_vchan->nRealVolume * kChnMasterVol) >> 7;
                } else
                {
                    //Extra attenuation required here if we're bypassing pre-amp.
                    realvol = (current_vchan->nRealVolume * kChnMasterVol) >> 8;
                }

                const forcePanningMode panningMode = m_pConfig->getForcePanningMode();
                if (panningMode == forceSoftPanning || (panningMode == dontForcePanningMode && (deprecated_global_sound_setup_bitmask & SNDMIX_SOFTPANNING)))
                {
                    if (pan < 128)
                    {
                        current_vchan->nNewLeftVol = (realvol * pan) >> 8;
                        current_vchan->nNewRightVol = (realvol * 128) >> 8;
                    } else
                    {
                        current_vchan->nNewLeftVol = (realvol * 128) >> 8;
                        current_vchan->nNewRightVol = (realvol * (256 - pan)) >> 8;
                    }
                } else
                {
                    current_vchan->nNewLeftVol = (realvol * pan) >> 8;
                    current_vchan->nNewRightVol = (realvol * (256 - pan)) >> 8;
                }

            } else
            {
                current_vchan->nNewRightVol = (current_vchan->nRealVolume * kChnMasterVol) >> 8;
                current_vchan->nNewLeftVol = current_vchan->nNewRightVol;
            }
            // Clipping volumes
            //if (current_vchan->nNewRightVol > 0xFFFF) current_vchan->nNewRightVol = 0xFFFF;
            //if (current_vchan->nNewLeftVol > 0xFFFF) current_vchan->nNewLeftVol = 0xFFFF;
            // Check IDO
            if (deprecated_global_sound_setup_bitmask & SNDMIX_NORESAMPLING)
            {
                current_vchan->flags |= CHN_NOIDO;
            } else
            {
                current_vchan->flags &= ~(CHN_NOIDO|CHN_HQSRC);
#ifndef FASTSOUNDLIB
                if (current_vchan->position_delta == 0x10000)
                {
                    current_vchan->flags |= CHN_NOIDO;
                } else
                if ((deprecated_global_sound_setup_bitmask & SNDMIX_HQRESAMPLER) && ((gnCPUUsage < 80) || (deprecated_global_sound_setup_bitmask & SNDMIX_DIRECTTODISK) || (m_nMixChannels < 8)))
                {
                    if ((!(deprecated_global_sound_setup_bitmask & SNDMIX_DIRECTTODISK)) && (!(deprecated_global_sound_setup_bitmask & SNDMIX_ULTRAHQSRCMODE)))
                    {
                        int fmax = 0x20000;
                        if (deprecated_global_system_info & SYSMIX_SLOWCPU)
                        {
                            fmax = 0xFE00;
                        } else
                        if (!(deprecated_global_system_info & (SYSMIX_ENABLEMMX|SYSMIX_FASTCPU)))
                        {
                            fmax = 0x18000;
                        }
                        if ((current_vchan->nNewLeftVol < 0x80) && (current_vchan->nNewRightVol < 0x80)
                         && (current_vchan->left_volume < 0x80) && (current_vchan->right_volume < 0x80))
                        {
                            if (current_vchan->position_delta >= 0xFF00) current_vchan->flags |= CHN_NOIDO;
                        } else
                        {
                            current_vchan->flags |= (current_vchan->position_delta >= fmax) ? CHN_NOIDO : CHN_HQSRC;
                        }
                    } else
                    {
                        current_vchan->flags |= CHN_HQSRC;
                    }

                } else
                {
                    if ((current_vchan->position_delta >= 0x14000)
                    || ((current_vchan->position_delta >= 0xFF00) && ((current_vchan->position_delta < 0x10100) || (deprecated_global_system_info & SYSMIX_SLOWCPU)))
                    || ((gnCPUUsage > 80) && (current_vchan->nNewLeftVol < 0x100) && (current_vchan->nNewRightVol < 0x100)))
                        current_vchan->flags |= CHN_NOIDO;
                }
#else
                if (current_vchan->position_delta >= 0xFE00) current_vchan->flags |= CHN_NOIDO;
                if (true) current_vchan->flags |= CHN_NOIDO;
#endif // FASTSOUNDLIB
            }

            /*if (m_pConfig->getUseGlobalPreAmp())
            {
                current_vchan->nNewRightVol >>= modplug::mixer::MIXING_ATTENUATION;
                current_vchan->nNewLeftVol >>= modplug::mixer::MIXING_ATTENUATION;
            }*/
            const int extraAttenuation = m_pConfig->getExtraSampleAttenuation();
            current_vchan->nNewRightVol >>= extraAttenuation;
            current_vchan->nNewLeftVol >>= extraAttenuation;

            current_vchan->right_ramp = current_vchan->left_ramp = 0;
            // Dolby Pro-Logic Surround
            if ((current_vchan->flags & CHN_SURROUND) && (deprecated_global_channels == 2)) current_vchan->nNewLeftVol = - current_vchan->nNewLeftVol;
            // Checking Ping-Pong Loops
            if (current_vchan->flags & CHN_PINGPONGFLAG) current_vchan->position_delta = -current_vchan->position_delta;

            // Setting up volume ramp
             // && gnVolumeRampSamples //rewbs: this allows us to use non ramping mix functions if ramping is 0
            if ((current_vchan->flags & CHN_VOLUMERAMP) && ((current_vchan->right_volume != current_vchan->nNewRightVol) || (current_vchan->left_volume != current_vchan->nNewLeftVol)))    {
                bool rampUp = (current_vchan->nNewRightVol > current_vchan->right_volume) || (current_vchan->nNewLeftVol > current_vchan->left_volume);
                LONG rampLength, globalRampLength, instrRampLength = 0;
                rampLength = globalRampLength = rampUp ? gnVolumeRampInSamples : gnVolumeRampOutSamples;
                //XXXih: add real support for bidi ramping here
// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
                //XXXih: jesus.
                bool enableCustomRamp = current_vchan->instrument && (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM));
                if (enableCustomRamp) {
                    instrRampLength = rampUp ? current_vchan->instrument->volume_ramp_up : current_vchan->instrument->volume_ramp_down;
                    rampLength = instrRampLength ? (deprecated_global_mixing_freq * instrRampLength / 100000) : globalRampLength;
                }
                if (!rampLength) {
                    rampLength = 1;
                }
// -! NEW_FEATURE#0027
                LONG nRightDelta = ((current_vchan->nNewRightVol - current_vchan->right_volume) << modplug::mixgraph::VOLUME_RAMP_PRECISION);
                LONG nLeftDelta  = ((current_vchan->nNewLeftVol - current_vchan->left_volume) << modplug::mixgraph::VOLUME_RAMP_PRECISION);
#ifndef FASTSOUNDLIB
// -> CODE#0027
// -> DESC="per-instrument volume ramping setup "
//                            if ((gdwSoundSetup & SNDMIX_DIRECTTODISK)
//                             || ((gdwSysInfo & (SYSMIX_ENABLEMMX|SYSMIX_FASTCPU))
//                              && (gdwSoundSetup & SNDMIX_HQRESAMPLER) && (gnCPUUsage <= 50)))
                if ((deprecated_global_sound_setup_bitmask & SNDMIX_DIRECTTODISK) || ((deprecated_global_system_info & (SYSMIX_ENABLEMMX | SYSMIX_FASTCPU)) && (deprecated_global_sound_setup_bitmask & SNDMIX_HQRESAMPLER) && (gnCPUUsage <= 50) && !(enableCustomRamp && instrRampLength))) {
// -! NEW_FEATURE#0027
                    if ((current_vchan->right_volume | current_vchan->left_volume) && (current_vchan->nNewRightVol | current_vchan->nNewLeftVol) && (!(current_vchan->flags & CHN_FASTVOLRAMP)))    {
                        rampLength = m_nBufferCount;
                        if (rampLength > (1 << (modplug::mixgraph::VOLUME_RAMP_PRECISION-1))) {
                            rampLength = (1 << (modplug::mixgraph::VOLUME_RAMP_PRECISION-1));
                        }
                        if (rampLength < (LONG)globalRampLength) {
                            rampLength = globalRampLength;
                        }
                    }
                }
#endif // FASTSOUNDLIB
                current_vchan->right_ramp = nRightDelta / rampLength;
                current_vchan->left_ramp = nLeftDelta / rampLength;
                current_vchan->right_volume = current_vchan->nNewRightVol - ((current_vchan->right_ramp * rampLength) >> modplug::mixgraph::VOLUME_RAMP_PRECISION);
                current_vchan->left_volume = current_vchan->nNewLeftVol - ((current_vchan->left_ramp * rampLength) >> modplug::mixgraph::VOLUME_RAMP_PRECISION);
                if (current_vchan->right_ramp|current_vchan->left_ramp)
                {
                    current_vchan->nRampLength = rampLength;
                } else
                {
                    current_vchan->flags &= ~CHN_VOLUMERAMP;
                    current_vchan->right_volume = current_vchan->nNewRightVol;
                    current_vchan->left_volume = current_vchan->nNewLeftVol;
                }
            } else
            {
                current_vchan->flags &= ~CHN_VOLUMERAMP;
                current_vchan->right_volume = current_vchan->nNewRightVol;
                current_vchan->left_volume = current_vchan->nNewLeftVol;
            }
            current_vchan->nRampRightVol = current_vchan->right_volume << modplug::mixgraph::VOLUME_RAMP_PRECISION;
            current_vchan->nRampLeftVol = current_vchan->left_volume << modplug::mixgraph::VOLUME_RAMP_PRECISION;
            // Adding the channel in the channel list
            ChnMix[m_nMixChannels] = vchan_idx;
            ++m_nMixChannels;
        } else
        {
#ifdef ENABLE_STEREOVU
            // Note change but no sample
            if (current_vchan->nLeftVU > 128) current_vchan->nLeftVU = 0;
            if (current_vchan->nRightVU > 128) current_vchan->nRightVU = 0;
#endif // ENABLE_STEREOVU
            if (current_vchan->nVUMeter > 0xFF) current_vchan->nVUMeter = 0;
            current_vchan->left_volume = current_vchan->right_volume = 0;
            current_vchan->length = 0;
        }
    }

    // Checking Max Mix Channels reached: ordering by volume
    if ((m_nMixChannels >= m_nMaxMixChannels) && (!(deprecated_global_sound_setup_bitmask & SNDMIX_DIRECTTODISK)))
    {
        for (UINT i=0; i<m_nMixChannels; i++)
        {
            UINT j=i;
            while ((j+1<m_nMixChannels) && (Chn[ChnMix[j]].nRealVolume < Chn[ChnMix[j+1]].nRealVolume))
            {
                UINT n = ChnMix[j];
                ChnMix[j] = ChnMix[j+1];
                ChnMix[j+1] = n;
                j++;
            }
        }
    }
    if (m_dwSongFlags & SONG_GLOBALFADE)
    {
        if (!m_nGlobalFadeSamples)
        {
            m_dwSongFlags |= SONG_ENDREACHED;
            return FALSE;
        }
        if (m_nGlobalFadeSamples > m_nBufferCount)
            m_nGlobalFadeSamples -= m_nBufferCount;
        else
            m_nGlobalFadeSamples = 0;
    }
    return TRUE;
}


#ifdef MODPLUG_TRACKER

VOID module_renderer::ProcessMidiOut(UINT nChn, modplug::tracker::modchannel_t *pChn)    //rewbs.VSTdelay: added arg
//----------------------------------------------------------
{
    // Do we need to process midi?
    // For now there is no difference between mute and sync mute with VSTis.
    if (pChn->flags & (CHN_MUTE|CHN_SYNCMUTE)) return;
    if ((!m_nInstruments) || (m_nPattern >= Patterns.Size())
         || (m_nRow >= Patterns[m_nPattern].GetNumRows()) || (!Patterns[m_nPattern])) return;

    const modplug::tracker::note_t note = pChn->nRowNote;
    const modplug::tracker::instr_t instr = pChn->nRowInstr;
    const modplug::tracker::vol_t vol = pChn->nRowVolume;

    //XXXih: gross!
    const modplug::tracker::volcmd_t volcmd = (modplug::tracker::volcmd_t) pChn->nRowVolCmd;
    // Debug
    {
        // Previously this function took modcommand directly from pattern. ASSERT is there
        // to detect possible behaviour change now that the data is accessed from channel.
        const modplug::tracker::modevent_t mc = *Patterns[m_nPattern].GetpModCommand(m_nRow, static_cast<modplug::tracker::chnindex_t>(nChn));
        ASSERT( mc.IsPcNote() ||
            (note == mc.note && instr == mc.instr && volcmd == mc.volcmd && vol == mc.vol));
    }

    // Get instrument info and plugin reference
    modplug::tracker::modinstrument_t *pIns = pChn->instrument;
    IMixPlugin *pPlugin = nullptr;

    if ((instr) && (instr < MAX_INSTRUMENTS))
        pIns = Instruments[instr];

    if (pIns)
    {
        // Check instrument plugins
        if ((pIns->midi_channel >= 1) && (pIns->midi_channel <= 16))
        {
            UINT nPlugin = GetBestPlugin(nChn, PRIORITISE_INSTRUMENT, RESPECT_MUTES);
            if ((nPlugin) && (nPlugin <= MAX_MIXPLUGINS))
            {
                pPlugin = m_MixPlugins[nPlugin-1].pMixPlugin;
            }
        }

        // Muted instrument?
        if (pIns && (pIns->flags & INS_MUTE)) return;
    }

    // Do couldn't find a valid plugin
    if (pPlugin == nullptr) return;

    if(GetModFlag(MSF_MIDICC_BUGEMULATION))
    {
        if(note)
        {
            modplug::tracker::note_t realNote = note;
            if((note >= NOTE_MIN) && (note <= NOTE_MAX))
                realNote = pIns->NoteMap[note - 1];
            pPlugin->MidiCommand(pIns->midi_channel, pIns->midi_program, pIns->midi_bank, realNote, pChn->nVolume, nChn);
        } else if (volcmd == modplug::tracker::VolCmdVol)
        {
            pPlugin->MidiCC(pIns->midi_channel, MIDICC_Volume_Fine, vol, nChn);
        }
        return;
    }


    const bool hasVolCommand = (volcmd == modplug::tracker::VolCmdVol);
    const UINT defaultVolume = pIns->global_volume;


    //If new note, determine notevelocity to use.
    if(note)
    {
        UINT velocity = 4*defaultVolume;
        switch(pIns->nPluginVelocityHandling)
        {
            case PLUGIN_VELOCITYHANDLING_CHANNEL:
                velocity = pChn->nVolume;
            break;
        }

        modplug::tracker::note_t realNote = note;
        if((note >= NOTE_MIN) && (note <= NOTE_MAX))
            realNote = pIns->NoteMap[note - 1];
        pPlugin->MidiCommand(pIns->midi_channel, pIns->midi_program, pIns->midi_bank, realNote, velocity, nChn);
    }


    const bool processVolumeAlsoOnNote = (pIns->nPluginVelocityHandling == PLUGIN_VELOCITYHANDLING_VOLUME);

    if((hasVolCommand && !note) || (note && processVolumeAlsoOnNote))
    {
        switch(pIns->nPluginVolumeHandling)
        {
            case PLUGIN_VOLUMEHANDLING_DRYWET:
                if(hasVolCommand) pPlugin->SetDryRatio(2*vol);
                else pPlugin->SetDryRatio(2*defaultVolume);
                break;

            case PLUGIN_VOLUMEHANDLING_MIDI:
                if(hasVolCommand) pPlugin->MidiCC(pIns->midi_channel, MIDICC_Volume_Coarse, min(127, 2*vol), nChn);
                else pPlugin->MidiCC(pIns->midi_channel, MIDICC_Volume_Coarse, min(127, 2*defaultVolume), nChn);
                break;

        }
    }
}

int module_renderer::GetVolEnvValueFromPosition(int position, modplug::tracker::modinstrument_t* pIns) const
//---------------------------------------------------------------------------------
{
    UINT pt = pIns->volume_envelope.num_nodes - 1;

    //Checking where current 'tick' is relative to the
    //envelope points.
    for (UINT i=0; i<(UINT)(pIns->volume_envelope.num_nodes-1); i++)
    {
        if (position <= pIns->volume_envelope.Ticks[i])
        {
            pt = i;
            break;
        }
    }

    int x2 = pIns->volume_envelope.Ticks[pt];
    int x1, envvol;
    if (position >= x2) //Case: current 'tick' is on a envelope point.
    {
        envvol = pIns->volume_envelope.Values[pt] << 2;
        x1 = x2;
    }
    else //Case: current 'tick' is between two envelope points.
    {
        if (pt)
        {
            envvol = pIns->volume_envelope.Values[pt-1] << 2;
            x1 = pIns->volume_envelope.Ticks[pt-1];
        } else
        {
            envvol = 0;
            x1 = 0;
        }

        if(x2 > x1 && position > x1)
        {
            //Linear approximation between the points;
            //f(x+d) ~ f(x) + f'(x)*d, where f'(x) = (y2-y1)/(x2-x1)
            envvol += ((position - x1) * (((int)pIns->volume_envelope.Values[pt]<<2) - envvol)) / (x2 - x1);
        }
    }
    return envvol;
}

#endif


VOID module_renderer::ApplyGlobalVolume(int SoundBuffer[], long lTotalSampleCount)
//---------------------------------------------------------------------------
{
        long delta       = 0;
        long step        = 0;
        long ramp_length = 0;

        if (m_nGlobalVolumeDestination != m_nGlobalVolume) { //user has provided new global volume
            bool rampUp = m_nGlobalVolumeDestination > m_nGlobalVolume;
            m_nGlobalVolumeDestination = m_nGlobalVolume;
            ramp_length = m_nSamplesToGlobalVolRampDest = rampUp ? gnVolumeRampInSamples : gnVolumeRampOutSamples;
        }

        if (m_nSamplesToGlobalVolRampDest > 0) {    // still some ramping left to do.
            long highResGlobalVolumeDestination = static_cast<long>(m_nGlobalVolumeDestination)<<modplug::mixgraph::VOLUME_RAMP_PRECISION;

            delta = highResGlobalVolumeDestination - m_lHighResRampingGlobalVolume;
            step = delta / static_cast<long>(m_nSamplesToGlobalVolRampDest);

            //XXXih: divide by zero >:l
            UINT maxStep = max(50, (10000 / (ramp_length + 1))); //define max step size as some factor of user defined ramping value: the lower the value, the more likely the click.
            while (abs(step) > maxStep) {                                     //if step is too big (might cause click), extend ramp length.
                m_nSamplesToGlobalVolRampDest += ramp_length;
                step = delta / static_cast<long>(m_nSamplesToGlobalVolRampDest);
            }
        }

        for (int pos = 0; pos < lTotalSampleCount; pos++) {
            if (m_nSamplesToGlobalVolRampDest > 0) { //ramping required
                m_lHighResRampingGlobalVolume += step;
                SoundBuffer[pos] = _muldiv(SoundBuffer[pos], m_lHighResRampingGlobalVolume, MAX_GLOBAL_VOLUME<<modplug::mixgraph::VOLUME_RAMP_PRECISION);
                m_nSamplesToGlobalVolRampDest--;
            } else {
                SoundBuffer[pos] = _muldiv(SoundBuffer[pos], m_nGlobalVolume, MAX_GLOBAL_VOLUME);
                m_lHighResRampingGlobalVolume = m_nGlobalVolume<<modplug::mixgraph::VOLUME_RAMP_PRECISION;
            }

        }
}