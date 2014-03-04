/*
 * OpenMPT
 *
 * Snd_fx.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"
#include "sndfile.h"
// -> CODE#0022
// -> DESC="alternative BPM/Speed interpretation method"
#include "../moddoc.h"
#include "../misc_util.h"
// -! NEW_FEATURE#0022
#include "tuning.h"
#include "../tracker/types.hpp"

using namespace modplug::tracker;

#pragma warning(disable:4244)

// Tables defined in tables.cpp
extern uint8_t ImpulseTrackerPortaVolCmd[16];
extern uint16_t S3MFineTuneTable[16];
extern uint16_t ProTrackerPeriodTable[6*12];
extern uint16_t ProTrackerTunedPeriods[15*12];
extern uint16_t FreqS3MTable[];
extern uint16_t XMPeriodTable[96+8];
extern UINT XMLinearTable[768];
extern uint32_t FineLinearSlideUpTable[16];
extern uint32_t FineLinearSlideDownTable[16];
extern uint32_t LinearSlideUpTable[256];
extern uint32_t LinearSlideDownTable[256];
extern signed char retrigTable1[16];
extern signed char retrigTable2[16];
extern short int ModRandomTable[64];
extern uint8_t ModEFxTable[16];


////////////////////////////////////////////////////////////
// Length


/*
void CSoundFile::GenerateSamplePosMap() {





    double accurateBufferCount =

    long lSample = 0;
    nPattern

    //for each order
        //get pattern for this order
        //if pattern if duff: break, we're done.
        //for each row in this pattern
            //get ticks for this row
            //get ticks per row and tempo for this row
            //for each tick

            //Recalc if ticks per row or tempo has changed

        samplesPerTick = (double)gdwMixingFreq * (60.0/(double)m_nMusicTempo / ((double)m_nMusicSpeed * (double)m_nRowsPerBeat));
        lSample += samplesPerTick;


        nPattern = ++nOrder;
    }

}
*/


// Get mod length in various cases. Parameters:
// [in]  adjustMode: See enmGetLengthResetMode for possible adjust modes.
// [in]  endOrder: Order which should be reached (modplug::tracker::ORDERINDEX_INVALID means whole song)
// [in]  endRow: Row in that order that should be reached
// [out] duration: total time in seconds
// [out] targetReached: true if the specified order/row combination has been reached while going through the module.
// [out] lastOrder: last parsed order (if no target is specified, this is the first order that is parsed twice, i.e. not the *last* played order)
// [out] lastRow: last parsed row (dito)
// [out] endOrder: last order before module loops (UNDEFINED if a target is specified)
// [out] endRow: last row before module loops (dito)
GetLengthType module_renderer::GetLength(enmGetLengthResetMode adjustMode, modplug::tracker::orderindex_t endOrder, modplug::tracker::rowindex_t endRow)
//---------------------------------------------------------------------------------------------------------
{
    GetLengthType retval;
    retval.duration = 0.0;
    retval.targetReached = false;
    retval.lastOrder = retval.endOrder = modplug::tracker::OrderIndexInvalid;
    retval.lastRow = retval.endRow = RowIndexInvalid;

// -> CODE#0022
// -> DESC="alternative BPM/Speed interpretation method"
//    UINT dwElapsedTime=0, nRow=0, nCurrentPattern=0, nNextPattern=0, nPattern=Order[0];
    modplug::tracker::rowindex_t nRow = 0;
    modplug::tracker::rowindex_t nNextPatStartRow = 0; // FT2 E60 bug
    modplug::tracker::orderindex_t nCurrentPattern = 0;
    modplug::tracker::orderindex_t nNextPattern = 0;
    modplug::tracker::patternindex_t nPattern = Order[0];
    double dElapsedTime=0.0;
// -! NEW_FEATURE#0022
    UINT nMusicSpeed = m_nDefaultSpeed, nMusicTempo = m_nDefaultTempo, nNextRow = 0;
    LONG nGlbVol = m_nDefaultGlobalVolume, nOldGlbVolSlide = 0;
    vector<uint8_t> instr(m_nChannels, 0);
    vector<UINT> notes(m_nChannels, 0);
    vector<uint8_t> vols(m_nChannels, 0xFF);
    vector<uint8_t> oldparam(m_nChannels, 0);
    vector<UINT> chnvols(m_nChannels, 64);
    vector<double> patloop(m_nChannels, 0);
    vector<modplug::tracker::rowindex_t> patloopstart(m_nChannels, 0);
    VisitedRowsType visitedRows;    // temporary visited rows vector (so that GetLength() won't interfere with the player code if the module is playing at the same time)

    InitializeVisitedRows(true, &visitedRows);

    for(modplug::tracker::chnindex_t icv = 0; icv < m_nChannels; icv++) chnvols[icv] = ChnSettings[icv].nVolume;
    nCurrentPattern = nNextPattern = 0;
    nPattern = Order[0];
    nRow = nNextRow = 0;

    for (;;)
    {
        UINT nSpeedCount = 0;
        nRow = nNextRow;
        nCurrentPattern = nNextPattern;

        if(nCurrentPattern >= Order.size())
            break;

        // Check if pattern is valid
        nPattern = Order[nCurrentPattern];
        bool positionJumpOnThisRow = false;
        bool patternBreakOnThisRow = false;

        while(nPattern >= Patterns.Size())
        {
            // End of song?
            if((nPattern == Order.GetInvalidPatIndex()) || (nCurrentPattern >= Order.size()))
            {
                if(nCurrentPattern == m_nRestartPos)
                    break;
                else
                    nCurrentPattern = m_nRestartPos;
            } else
            {
                nCurrentPattern++;
            }
            nPattern = (nCurrentPattern < Order.size()) ? Order[nCurrentPattern] : Order.GetInvalidPatIndex();
            nNextPattern = nCurrentPattern;
            if((!Patterns.IsValidPat(nPattern)) && IsRowVisited(nCurrentPattern, 0, true, &visitedRows))
                break;
        }
        // Skip non-existing patterns
        if ((nPattern >= Patterns.Size()) || (!Patterns[nPattern]))
        {
            // If there isn't even a tune, we should probably stop here.
            if(nCurrentPattern == m_nRestartPos)
                break;
            nNextPattern = nCurrentPattern + 1;
            continue;
        }
        // Should never happen
        if (nRow >= Patterns[nPattern].GetNumRows())
            nRow = 0;

        //Check whether target reached.
        if(nCurrentPattern == endOrder && nRow == endRow)
        {
            retval.targetReached = true;
            break;
        }

        if(IsRowVisited(nCurrentPattern, nRow, true, &visitedRows))
            break;

        retval.endOrder = nCurrentPattern;
        retval.endRow = nRow;

        // Update next position
        nNextRow = nRow + 1;

        if (nNextRow >= Patterns[nPattern].GetNumRows())
        {
            nNextPattern = nCurrentPattern + 1;
            nNextRow = 0;
            if(IsCompatibleMode(TRK_FASTTRACKER2)) nNextRow = nNextPatStartRow;  // FT2 E60 bug
            nNextPatStartRow = 0;
        }
        if (!nRow)
        {
            for(UINT ipck = 0; ipck < m_nChannels; ipck++)
                patloop[ipck] = dElapsedTime;
        }

        modplug::tracker::voice_ty *pChn = Chn.data();
        modplug::tracker::modevent_t *p = Patterns[nPattern] + nRow * m_nChannels;
        modplug::tracker::modevent_t *nextRow = NULL;
        for (modplug::tracker::chnindex_t nChn = 0; nChn < m_nChannels; p++, pChn++, nChn++) if (*((uint32_t *)p))
        {
            if((GetType() == MOD_TYPE_S3M) && (ChnSettings[nChn].dwFlags & CHN_MUTE) != 0)    // not even effects are processed on muted S3M channels
                continue;
            UINT command = p->command;
            UINT param = p->param;
            UINT note = p->note;
            if (p->instr) { instr[nChn] = p->instr; notes[nChn] = 0; vols[nChn] = 0xFF; }
            if ((note) && (note <= NoteMax)) notes[nChn] = note;
            if (p->volcmd == VolCmdVol)    { vols[nChn] = p->vol; }
            if (command) switch (command)
            {
            // Position Jump
            case CmdPositionJump:
                positionJumpOnThisRow = true;
                nNextPattern = (modplug::tracker::orderindex_t)param;
                nNextPatStartRow = 0;  // FT2 E60 bug
                // see http://forum.openmpt.org/index.php?topic=2769.0 - FastTracker resets Dxx if Bxx is called _after_ Dxx
                if(!patternBreakOnThisRow || (GetType() == MOD_TYPE_XM))
                    nNextRow = 0;

                if ((adjustMode & eAdjust))
                {
                    pChn->nPatternLoopCount = 0;
                    pChn->nPatternLoop = 0;
                }
                break;
            // Pattern Break
            case CmdPatternBreak:
                if(param >= 64 && (GetType() & MOD_TYPE_S3M))
                {
                    // ST3 ignores invalid pattern breaks.
                    break;
                }
                patternBreakOnThisRow = true;
                //Try to check next row for XPARAM
                nextRow = nullptr;
                nNextPatStartRow = 0;  // FT2 E60 bug
                if (nRow < Patterns[nPattern].GetNumRows() - 1)
                {
                    nextRow = Patterns[nPattern] + (nRow + 1) * m_nChannels + nChn;
                }
                if (nextRow && nextRow->command == CmdExtendedParameter)
                {
                    nNextRow = (param << 8) + nextRow->param;
                } else
                {
                    nNextRow = param;
                }

                if (!positionJumpOnThisRow)
                {
                    nNextPattern = nCurrentPattern + 1;
                }
                if ((adjustMode & eAdjust))
                {
                    pChn->nPatternLoopCount = 0;
                    pChn->nPatternLoop = 0;
                }
                break;
            // Set Speed
            case CmdSpeed:
                if (!param) break;
                // Allow high speed values here for VBlank MODs. (Maybe it would be better to have a "VBlank MOD" flag somewhere? Is it worth the effort?)
                if ((param <= GetModSpecifications().speedMax) || (m_nType & MOD_TYPE_MOD))
                {
                    nMusicSpeed = param;
                }
                break;
            // Set Tempo
            case CmdTempo:
                if ((adjustMode & eAdjust) && (m_nType & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT)))
                {
                    if (param) pChn->nOldTempo = (uint8_t)param; else param = pChn->nOldTempo;
                }
                if (param >= 0x20) nMusicTempo = param; else
                // Tempo Slide
                if ((param & 0xF0) == 0x10)
                {
                    nMusicTempo += (param & 0x0F)  * (nMusicSpeed - 1);  //rewbs.tempoSlideFix
                } else
                {
                    nMusicTempo -= (param & 0x0F) * (nMusicSpeed - 1); //rewbs.tempoSlideFix
                }
// -> CODE#0010
// -> DESC="add extended parameter mechanism to pattern effects"
                if(IsCompatibleMode(TRK_ALLTRACKERS))    // clamp tempo correctly in compatible mode
                    nMusicTempo = CLAMP(nMusicTempo, 32, 255);
                else
                    nMusicTempo = CLAMP(nMusicTempo, GetModSpecifications().tempoMin, GetModSpecifications().tempoMax);
// -! NEW_FEATURE#0010
                break;
            // Pattern Delay
            case CmdS3mCmdEx:
                if ((param & 0xF0) == 0x60) { nSpeedCount = param & 0x0F; break; } else
                if ((param & 0xF0) == 0xA0) { pChn->nOldHiOffset = param & 0x0F; break; } else
                if ((param & 0xF0) == 0xB0) { param &= 0x0F; param |= 0x60; }
            case CmdModCmdEx:
                if ((param & 0xF0) == 0xE0) nSpeedCount = (param & 0x0F) * nMusicSpeed; else
                if ((param & 0xF0) == 0x60)
                {
                    if (param & 0x0F)
                    {
                        dElapsedTime += (dElapsedTime - patloop[nChn]) * (double)(param & 0x0F);
                        nNextPatStartRow = patloopstart[nChn]; // FT2 E60 bug
                    } else
                    {
                        patloop[nChn] = dElapsedTime;
                        patloopstart[nChn] = nRow;
                    }
                }
                break;
            case CmdExtraFinePortaUpDown:
                // ignore high offset in compatible mode
                if (((param & 0xF0) == 0xA0) && !IsCompatibleMode(TRK_FASTTRACKER2)) pChn->nOldHiOffset = param & 0x0F;
                break;
            }
            if ((adjustMode & eAdjust) == 0) continue;
            switch(command)
            {
            // Portamento Up/Down
            case CmdPortaUp:
            case CmdPortaDown:
                if (param) pChn->nOldPortaUpDown = param;
                break;
            // Tone-Portamento
            case CmdPorta:
                if (param) pChn->nPortamentoSlide = param << 2;
                break;
            // Offset
            case CmdOffset:
                if (param) pChn->nOldOffset = param;
                break;
            // Volume Slide
            case CmdVolSlide:
            case CmdPortaVolSlide:
            case CmdVibratoVolSlide:
                if (param) pChn->nOldVolumeSlide = param;
                break;
            // Set Volume
            case CmdVol:
                vols[nChn] = param;
                break;
            // Global Volume
            case CmdGlobalVol:
                // ST3 applies global volume on tick 1 and does other weird things, but we won't emulate this for now.
//                             if((GetType() & MOD_TYPE_S3M) && nMusicSpeed <= 1)
//                             {
//                                     break;
//                             }

                if (!(GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT))) param <<= 1;
                if(IsCompatibleMode(TRK_IMPULSETRACKER | TRK_FASTTRACKER2 | TRK_SCREAMTRACKER))
                {
                    //IT compatibility 16. FT2, ST3 and IT ignore out-of-range values
                    if (param <= 128)
                        nGlbVol = param << 1;
                }
                else
                {
                    if (param > 128) param = 128;
                    nGlbVol = param << 1;
                }
                break;
            // Global Volume Slide
            case CmdGlobalVolSlide:
                if(IsCompatibleMode(TRK_IMPULSETRACKER | TRK_FASTTRACKER2))
                {
                    //IT compatibility 16. Global volume slide params are stored per channel (FT2/IT)
                    if (param) pChn->nOldGlobalVolSlide = param; else param = pChn->nOldGlobalVolSlide;
                }
                else
                {
                    if (param) nOldGlbVolSlide = param; else param = nOldGlbVolSlide;

                }
                if (((param & 0x0F) == 0x0F) && (param & 0xF0))
                {
                    param >>= 4;
                    if (!(GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT))) param <<= 1;
                    nGlbVol += param << 1;
                } else
                if (((param & 0xF0) == 0xF0) && (param & 0x0F))
                {
                    param = (param & 0x0F) << 1;
                    if (!(GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT))) param <<= 1;
                    nGlbVol -= param;
                } else
                if (param & 0xF0)
                {
                    param >>= 4;
                    param <<= 1;
                    if (!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) param <<= 1;
                    nGlbVol += param * nMusicSpeed;
                } else
                {
                    param = (param & 0x0F) << 1;
                    if (!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) param <<= 1;
                    nGlbVol -= param * nMusicSpeed;
                }
                nGlbVol = CLAMP(nGlbVol, 0, 256);
                break;
            case CmdChannelVol:
                if (param <= 64) chnvols[nChn] = param;
                break;
            case CmdChannelVolSlide:
                if (param) oldparam[nChn] = param; else param = oldparam[nChn];
                pChn->nOldChnVolSlide = param;
                if (((param & 0x0F) == 0x0F) && (param & 0xF0))
                {
                    param = (param >> 4) + chnvols[nChn];
                } else
                if (((param & 0xF0) == 0xF0) && (param & 0x0F))
                {
                    if (chnvols[nChn] > (int)(param & 0x0F)) param = chnvols[nChn] - (param & 0x0F);
                    else param = 0;
                } else
                if (param & 0x0F)
                {
                    param = (param & 0x0F) * nMusicSpeed;
                    param = (chnvols[nChn] > param) ? chnvols[nChn] - param : 0;
                } else param = ((param & 0xF0) >> 4) * nMusicSpeed + chnvols[nChn];
                param = bad_min(param, 64);
                chnvols[nChn] = param;
                break;
            }
        }
        nSpeedCount += nMusicSpeed;
        switch(m_nTempoMode)
        {
            case tempo_mode_alternative:
                dElapsedTime +=  60000.0 / (1.65625 * (double)(nMusicSpeed * nMusicTempo)); break;
            case tempo_mode_modern:
                dElapsedTime += 60000.0 / (double)nMusicTempo / (double)m_nCurrentRowsPerBeat; break;
            case tempo_mode_classic: default:
                dElapsedTime += (2500.0 * (double)nSpeedCount) / (double)nMusicTempo;
        }
    }

    if(retval.targetReached || endOrder == modplug::tracker::OrderIndexInvalid || endRow == RowIndexInvalid)
    {
        retval.lastOrder = nCurrentPattern;
        retval.lastRow = nRow;
    }
    retval.duration = dElapsedTime / 1000.0;

    // Store final variables
    if ((adjustMode & eAdjust))
    {
        if (retval.targetReached || endOrder == modplug::tracker::OrderIndexInvalid || endRow == RowIndexInvalid)
        {
            // Target found, or there is no target (i.e. play whole song)...
            m_nGlobalVolume = nGlbVol;
            m_nOldGlbVolSlide = nOldGlbVolSlide;
            m_nMusicSpeed = nMusicSpeed;
            m_nMusicTempo = nMusicTempo;
            for (modplug::tracker::chnindex_t n = 0; n < m_nChannels; n++)
            {
                Chn[n].nGlobalVol = chnvols[n];
                if (notes[n]) Chn[n].nNewNote = notes[n];
                if (instr[n]) Chn[n].nNewIns = instr[n];
                if (vols[n] != 0xFF)
                {
                    if (vols[n] > 64) vols[n] = 64;
                    Chn[n].nVolume = vols[n] * 4;
                }
            }
        } else if(adjustMode != eAdjustOnSuccess)
        {
            // Target not found (f.e. when jumping to a hidden sub song), reset global variables...
            m_nMusicSpeed = m_nDefaultSpeed;
            m_nMusicTempo = m_nDefaultTempo;
            m_nGlobalVolume = m_nDefaultGlobalVolume;
        }
        // When adjusting the playback status, we will also want to update the visited rows vector according to the current position.
        m_VisitedRows = visitedRows;
    }

    return retval;

}


//////////////////////////////////////////////////////////////////////////////////////////////////
// Effects

void module_renderer::InstrumentChange(modplug::tracker::voice_ty *pChn, UINT instr, bool bPorta, bool bUpdVol, bool bResetEnv)
//--------------------------------------------------------------------------------------------------------
{
    bool bInstrumentChanged = false;

    if (instr >= MAX_INSTRUMENTS) return;
    modplug::tracker::modinstrument_t *pIns = Instruments[instr];
    modplug::tracker::modsample_t *pSmp = &Samples[instr];
    UINT note = pChn->nNewNote;

    if(note == NoteNone && IsCompatibleMode(TRK_IMPULSETRACKER)) return;

    if ((pIns) && (note) && (note <= 128))
    {
        if(bPorta && pIns == pChn->instrument && (pChn->sample != nullptr && pChn->sample->sample_data.generic != nullptr) && IsCompatibleMode(TRK_IMPULSETRACKER))
        {
#ifdef DEBUG
            {
                // Check if original behaviour would have been used here
                if (pIns->NoteMap[note-1] >= NoteMinSpecial)
                {
                    ASSERT(false);
                    return;
                }
                UINT n = pIns->Keyboard[note-1];
                pSmp = ((n) && (n < MAX_SAMPLES)) ? &Samples[n] : nullptr;
                if(pSmp != pChn->sample)
                {
                    ASSERT(false);
                }
            }
#endif // DEBUG
            // Impulse Tracker doesn't seem to look up the sample for new notes when in instrument mode and it encounters a situation like this:
            // G-6 01 ... ... <-- G-6 is bound to sample 01
            // F-6 01 ... GFF <-- F-6 is bound to sample 02, but sample 01 will be played
            // This behaviour is not used if sample 01 has no actual sample data (hence the "pChn->sample->sample_data != nullptr")
            // and it is also ignored when the instrument number changes. This fixes f.e. the guitars in "Ultima Ratio" by Nebularia
            // and some slides in spx-shuttledeparture.it.
            pSmp = pChn->sample;
        } else
        {
            // Original behaviour
            if(pIns->NoteMap[note-1] > NoteMax) return;
            UINT n = pIns->Keyboard[note-1];
            pSmp = ((n) && (n < MAX_SAMPLES)) ? &Samples[n] : nullptr;
        }
    } else
    if (m_nInstruments)
    {
        if (note >= NoteMinSpecial) return;
        pSmp = NULL;
    }

    const bool bNewTuning = (m_nType == MOD_TYPE_MPT && pIns && pIns->pTuning);
    //Playback behavior change for MPT: With portamento don't change sample if it is in
    //the same instrument as previous sample.
    if(bPorta && bNewTuning && pIns == pChn->instrument)
        return;

    bool returnAfterVolumeAdjust = false;
    // bInstrumentChanged is used for IT carry-on env option
    if (pIns != pChn->instrument)
    {
        bInstrumentChanged = true;
        // we will set the new instrument later.
    }
    else
    {
        // Special XM hack
        if ((bPorta) && (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2)) && (pIns)
        && (pChn->sample) && (pSmp != pChn->sample))
        {
            // FT2 doesn't change the sample in this case,
            // but still uses the sample info from the old one (bug?)
            returnAfterVolumeAdjust = true;
        }
    }

    // XM compatibility: new instrument + portamento = ignore new instrument number, but reload old instrument settings (the world of XM is upside down...)
    if(bInstrumentChanged && bPorta && IsCompatibleMode(TRK_FASTTRACKER2))
    {
        pIns = pChn->instrument;
        pSmp = pChn->sample;
        bInstrumentChanged = false;
    } else
    {
        pChn->instrument = pIns;
    }

    // Update Volume
    if (bUpdVol)
    {
        pChn->nVolume = 0;
        if(pSmp)
            pChn->nVolume = pSmp->default_volume;
        else
        {
            if(pIns && pIns->nMixPlug)
                pChn->nVolume = pChn->GetVSTVolume();
        }
    }

    if(returnAfterVolumeAdjust) return;


    // Instrument adjust
    pChn->nNewIns = 0;

    if (pIns && (pIns->nMixPlug || pSmp))            //rewbs.VSTiNNA
        pChn->nNNA = pIns->new_note_action;

    if (pSmp)
    {
        if (pIns)
        {
            pChn->nInsVol = (pSmp->global_volume * pIns->global_volume) >> 6;
            if (pIns->flags & INS_SETPANNING) pChn->nPan = pIns->default_pan;
        } else
        {
            pChn->nInsVol = pSmp->global_volume;
        }
        if (bitset_is_set(pSmp->flags, sflag_ty::ForcedPanning)) pChn->nPan = pSmp->default_pan;
    }


    // Reset envelopes
    if (bResetEnv)
    {
        if ((!bPorta) || (!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) || (m_dwSongFlags & SONG_ITCOMPATGXX)
         || (!pChn->length) || (bitset_is_set(pChn->flags, vflag_ty::NoteFade) && (!pChn->nFadeOutVol))
         //IT compatibility tentative fix: Reset envelopes when instrument changes.
         || (IsCompatibleMode(TRK_IMPULSETRACKER) && bInstrumentChanged))
        {
            bitset_add(pChn->flags, vflag_ty::FastVolRamp);
            if ( (m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT)) &&
                 (!bInstrumentChanged) && (pIns) &&
                 (!bitset_is_set(pChn->flags, vflag_ty::KeyOff, vflag_ty::NoteFade)) )
            {
                if (!(pIns->volume_envelope.flags & ENV_CARRY)) ResetChannelEnvelope(pChn->volume_envelope);
                if (!(pIns->panning_envelope.flags & ENV_CARRY)) ResetChannelEnvelope(pChn->panning_envelope);
                if (!(pIns->pitch_envelope.flags & ENV_CARRY)) ResetChannelEnvelope(pChn->pitch_envelope);
            } else
            {
                ResetChannelEnvelopes(pChn);
            }
            // IT Compatibility: Autovibrato reset
            if(!IsCompatibleMode(TRK_IMPULSETRACKER))
            {
                pChn->nAutoVibDepth = 0;
                pChn->nAutoVibPos = 0;
            }
        } else if ((pIns) && (!(pIns->volume_envelope.flags & ENV_ENABLED)))
        {
            if(IsCompatibleMode(TRK_IMPULSETRACKER))
            {
                ResetChannelEnvelope(pChn->volume_envelope);
            } else
            {
                ResetChannelEnvelopes(pChn);
            }
        }
    }
    // Invalid sample ?
    if (!pSmp)
    {
        pChn->sample = nullptr;
        pChn->nInsVol = 0;
        return;
    }

    pChn->sample_tag = pSmp->sample_tag;
    // Tone-Portamento doesn't reset the pingpong direction flag
    if ((bPorta) && (pSmp == pChn->sample))
    {
        if(m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT)) return;
        bitset_remove(pChn->flags, vflag_ty::KeyOff);
        bitset_remove(pChn->flags, vflag_ty::NoteFade);
        pChn->flags = bitset_union(
            voicef_unset_smpf(pChn->flags),
            voicef_from_smpf(pSmp->flags));
    } else
    {
        bitset_remove(pChn->flags, vflag_ty::KeyOff);
        bitset_remove(pChn->flags, vflag_ty::NoteFade);
        bitset_remove(pChn->flags, vflag_ty::VolEnvOn);
        bitset_remove(pChn->flags, vflag_ty::PanEnvOn);
        bitset_remove(pChn->flags, vflag_ty::PitchEnvOn);

        //IT compatibility tentative fix: Don't change bidi loop direction when
        //no sample nor instrument is changed.
        const auto fresh_flags = voicef_unset_smpf(pChn->flags);
        const auto voice_flags = voicef_from_smpf(pSmp->flags);
        if (IsCompatibleMode(TRK_IMPULSETRACKER) && pSmp == pChn->sample && !bInstrumentChanged) {
            pChn->flags = bitset_union(fresh_flags, voice_flags);
        } else {
            pChn->flags = bitset_union(
                bitset_unset(fresh_flags, vflag_ty::ScrubBackwards), voice_flags);
        }


        if (pIns)
        {
            if (pIns->volume_envelope.flags & ENV_ENABLED) bitset_add(pChn->flags, vflag_ty::VolEnvOn);
            if (pIns->panning_envelope.flags & ENV_ENABLED) bitset_add(pChn->flags, vflag_ty::PanEnvOn);
            if (pIns->pitch_envelope.flags & ENV_ENABLED) bitset_add(pChn->flags, vflag_ty::PitchEnvOn);
            if ((pIns->pitch_envelope.flags & ENV_ENABLED) && (pIns->pitch_envelope.flags & ENV_FILTER))
            {
                bitset_add(pChn->flags, vflag_ty::PitchEnvAsFilter);
                if (!pChn->nCutOff) pChn->nCutOff = 0x7F;
            } else
            {
                // Special case: Reset filter envelope flag manually (because of S7D/S7E effects).
                // This way, the S7x effects can be applied to several notes, as long as they don't come with an instrument number.
                bitset_remove(pChn->flags, vflag_ty::PitchEnvAsFilter);
            }
            if (pIns->default_filter_cutoff & 0x80) pChn->nCutOff = pIns->default_filter_cutoff & 0x7F;
            if (pIns->default_filter_resonance & 0x80) pChn->nResonance = pIns->default_filter_resonance & 0x7F;
        }
        pChn->nVolSwing = pChn->nPanSwing = 0;
        pChn->nResSwing = pChn->nCutSwing = 0;
    }
    pChn->sample = pSmp;
    pChn->length = pSmp->length;
    pChn->loop_start = pSmp->loop_start;
    pChn->loop_end = pSmp->loop_end;

    // IT Compatibility: Autovibrato reset
    if(IsCompatibleMode(TRK_IMPULSETRACKER))
    {
        pChn->nAutoVibDepth = 0;
        pChn->nAutoVibPos = 0;
    }

    if(bNewTuning)
    {
        pChn->nC5Speed = pSmp->c5_samplerate;
        pChn->m_CalculateFreq = true;
        pChn->nFineTune = 0;
    }
    else
    {
        pChn->nC5Speed = pSmp->c5_samplerate;
        pChn->nFineTune = pSmp->nFineTune;
    }


    pChn->sample_data = pSmp->sample_data;
    pChn->nTranspose = pSmp->RelativeTone;

    pChn->m_PortamentoFineSteps = 0;
    pChn->nPortamentoDest = 0;

    if (bitset_is_set(pChn->flags, vflag_ty::SustainLoop))
    {
        pChn->loop_start = pSmp->sustain_start;
        pChn->loop_end = pSmp->sustain_end;
        bitset_add(pChn->flags, vflag_ty::Loop);
        if (bitset_is_set(pChn->flags, vflag_ty::BidiSustainLoop)) {
            bitset_add(pChn->flags, vflag_ty::BidiLoop);
        }
    }

    if (bitset_is_set(pChn->flags, vflag_ty::Loop) && (pChn->loop_end < pChn->length)) pChn->length = pChn->loop_end;

    // Fix sample position on instrument change. This is needed for PT1x MOD and IT "on the fly" sample change.
    if(pChn->sample_position >= pChn->length)
    {
        if((m_nType & MOD_TYPE_IT))
        {
            pChn->sample_position = pChn->fractional_sample_position = 0;
        } else if((m_nType & MOD_TYPE_MOD))    // TODO does not always seem to work, especially with short chip samples?
        {
            pChn->sample_position = pChn->loop_start;
            pChn->fractional_sample_position = 0;
        }
    }
}


void module_renderer::NoteChange(UINT nChn, int note, bool bPorta, bool bResetEnv, bool bManual)
//-----------------------------------------------------------------------------------------
{
    if (note < NoteMin) return;
    auto * const pChn = &Chn[nChn];
    auto *pSmp = pChn->sample;
    auto *pIns = pChn->instrument;

    const bool bNewTuning = (m_nType == MOD_TYPE_MPT && pIns && pIns->pTuning);
    // save the note that's actually used, as it's necessary to properly calculate PPS and stuff
    const auto realnote = note;

    if ((pIns) && (note <= 0x80))
    {
        UINT n = pIns->Keyboard[note - 1];
        if ((n) && (n < MAX_SAMPLES)) pSmp = &Samples[n];
        note = pIns->NoteMap[note-1];
    }
    // Key Off
    if (note > NoteMax)
    {
        // Key Off (+ Invalid Note for XM - TODO is this correct?)
        if (note == NoteKeyOff || !(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) {
            KeyOff(nChn);
        } else {// Invalid Note -> Note Fade
        //if (note == NOTE_FADE)
            if(m_nInstruments) bitset_add(pChn->flags, vflag_ty::NoteFade);
        }

        // Note Cut
        if (note == NoteNoteCut)
        {
            bitset_add(pChn->flags, vflag_ty::NoteFade);
            bitset_add(pChn->flags, vflag_ty::FastVolRamp);
            if ((!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) || (m_nInstruments)) pChn->nVolume = 0;
            pChn->nFadeOutVol = 0;
        }

        //IT compatibility tentative fix: Clear channel note memory.
        if(IsCompatibleMode(TRK_IMPULSETRACKER))
        {
            pChn->nNote = pChn->nNewNote = NoteNone;
        }
        return;
    }

    if(bNewTuning)
    {
        if(!bPorta || pChn->nNote == NoteNone)
            pChn->nPortamentoDest = 0;
        else
        {
            pChn->nPortamentoDest = pIns->pTuning->GetStepDistance(pChn->nNote, pChn->m_PortamentoFineSteps, note, 0);
            //Here pCnh->nPortamentoDest means 'steps to slide'.
            pChn->m_PortamentoFineSteps = -pChn->nPortamentoDest;
        }
    }

    if ((!bPorta) && (m_nType & (MOD_TYPE_XM|MOD_TYPE_MED|MOD_TYPE_MT2)))
    {
        if (pSmp)
        {
            pChn->nTranspose = pSmp->RelativeTone;
            pChn->nFineTune = pSmp->nFineTune;
        }
    }
    // IT Compatibility: Update multisample instruments frequency even if instrument is not specified (fixes the guitars in spx-shuttledeparture.it)
    if(!bPorta && pSmp && IsCompatibleMode(TRK_IMPULSETRACKER)) pChn->nC5Speed = pSmp->c5_samplerate;

    // XM Compatibility: Ignore notes with portamento if there was no note playing.
    if(bPorta && (pChn->position_delta == 0) && IsCompatibleMode(TRK_FASTTRACKER2))
    {
        pChn->nPeriod = 0;
        return;
    }

    if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2|MOD_TYPE_MED))
    {
        note += pChn->nTranspose;
        note = CLAMP(note, NoteMin, 131);    // why 131? 120+11, how does this make sense?
    } else
    {
        note = CLAMP(note, NoteMin, NoteMax);
    }
    if(IsCompatibleMode(TRK_IMPULSETRACKER))
    {
        // need to memorize the original note for various effects (f.e. PPS)
        pChn->nNote = CLAMP(realnote, NoteMin, NoteMax);
    } else
    {
        pChn->nNote = note;
    }
    pChn->m_CalculateFreq = true;

    if ((!bPorta) || (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT)))
        pChn->nNewIns = 0;

    UINT period = GetPeriodFromNote(note, pChn->nFineTune, pChn->nC5Speed);

    if (!pSmp) return;
    if (period)
    {
        if ((!bPorta) || (!pChn->nPeriod)) pChn->nPeriod = period;
        if(!bNewTuning) pChn->nPortamentoDest = period;
        if ((!bPorta) || ((!pChn->length) && (!(m_nType & MOD_TYPE_S3M))))
        {
            pChn->sample = pSmp;
            pChn->sample_data = pSmp->sample_data;
            pChn->length = pSmp->length;
            pChn->loop_end = pSmp->length;
            pChn->loop_start = 0;
            pChn->sample_tag = pSmp->sample_tag;
            pChn->flags = bitset_union(
                bitset_unset(voicef_unset_smpf(pChn->flags), vflag_ty::ScrubBackwards),
                voicef_from_smpf(pSmp->flags));
            if (bitset_is_set(pChn->flags, vflag_ty::SustainLoop)) {
                pChn->loop_start = pSmp->sustain_start;
                pChn->loop_end = pSmp->sustain_end;
                bitset_remove(pChn->flags, vflag_ty::BidiLoop);
                bitset_add(pChn->flags, vflag_ty::Loop);
                if (bitset_is_set(pChn->flags, vflag_ty::BidiSustainLoop)) {
                    bitset_add(pChn->flags, vflag_ty::BidiLoop);
                }
                if (pChn->length > pChn->loop_end) pChn->length = pChn->loop_end;
            } else
            if (bitset_is_set(pChn->flags, vflag_ty::Loop)) {
                pChn->loop_start = pSmp->loop_start;
                pChn->loop_end = pSmp->loop_end;
                if (pChn->length > pChn->loop_end) pChn->length = pChn->loop_end;
            }
            pChn->sample_position = 0;
            pChn->fractional_sample_position = 0;
            // Handle "retrigger" waveform type
            if (pChn->nVibratoType < 4)
            {
                // IT Compatibilty: Slightly different waveform offsets (why does MPT have two different offsets here with IT old effects enabled and disabled?)
                if(!IsCompatibleMode(TRK_IMPULSETRACKER) && (GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT)) && (!(m_dwSongFlags & SONG_ITOLDEFFECTS)))
                    pChn->nVibratoPos = 0x10;
                else
                    pChn->nVibratoPos = 0;
            }
            // IT Compatibility: No "retrigger" waveform here
            if(!IsCompatibleMode(TRK_IMPULSETRACKER) && pChn->nTremoloType < 4)
            {
                pChn->nTremoloPos = 0;
            }
        }
        if (pChn->sample_position >= pChn->length) pChn->sample_position = pChn->loop_start;
    }
    else
        bPorta = false;

    if ( (!bPorta) ||
         (!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) ||
         (bitset_is_set(pChn->flags, vflag_ty::NoteFade) && (!pChn->nFadeOutVol)) ||
         ((m_dwSongFlags & SONG_ITCOMPATGXX) && (pChn->nRowInstr)) )
    {
        if ( (m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT)) &&
             bitset_is_set(pChn->flags, vflag_ty::NoteFade) &&
             (!pChn->nFadeOutVol))
        {
            ResetChannelEnvelopes(pChn);
            // IT Compatibility: Autovibrato reset
            if(!IsCompatibleMode(TRK_IMPULSETRACKER))
            {
                pChn->nAutoVibDepth = 0;
                pChn->nAutoVibPos = 0;
            }
            bitset_remove(pChn->flags, vflag_ty::NoteFade);
            pChn->nFadeOutVol = 65536;
        }
        if ((!bPorta) || (!(m_dwSongFlags & SONG_ITCOMPATGXX)) || (pChn->nRowInstr))
        {
            if ((!(m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))) || (pChn->nRowInstr))
            {
                bitset_remove(pChn->flags, vflag_ty::NoteFade);
                pChn->nFadeOutVol = 65536;
            }
        }
    }
    bitset_remove(pChn->flags, vflag_ty::ExtraLoud);
    bitset_remove(pChn->flags, vflag_ty::KeyOff);
    // Enable Ramping
    if (!bPorta)
    {
        pChn->nVUMeter = 0x100;
        bitset_remove(pChn->flags, vflag_ty::Filter);
        bitset_add(pChn->flags, vflag_ty::FastVolRamp);
        // IT Compatibility: Autovibrato reset
        if(bResetEnv && IsCompatibleMode(TRK_IMPULSETRACKER))
        {
            pChn->nAutoVibDepth = 0;
            pChn->nAutoVibPos = 0;
            pChn->nVibratoPos = 0;
        }
        //IT compatibility 15. Retrigger will not be reset (Tremor doesn't store anything here, so we just don't reset this as well)
        if(!IsCompatibleMode(TRK_IMPULSETRACKER))
        {
            // XM compatibility: FT2 also doesn't reset retrigger
            if(!IsCompatibleMode(TRK_FASTTRACKER2)) pChn->nRetrigCount = 0;
            pChn->nTremorCount = 0;
        }
        if (bResetEnv)
        {
            pChn->nVolSwing = pChn->nPanSwing = 0;
            pChn->nResSwing = pChn->nCutSwing = 0;
            if (pIns)
            {
                // IT compatibility tentative fix: Reset NNA action on every new note, even without instrument number next to note (fixes spx-farspacedance.it, but is this actually 100% correct?)
                if(IsCompatibleMode(TRK_IMPULSETRACKER)) pChn->nNNA = pIns->new_note_action;
                if (!(pIns->volume_envelope.flags & ENV_CARRY)) pChn->volume_envelope.position = 0;
                if (!(pIns->panning_envelope.flags & ENV_CARRY)) pChn->panning_envelope.position = 0;
                if (!(pIns->pitch_envelope.flags & ENV_CARRY)) pChn->pitch_envelope.position = 0;
                if (m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))
                {
                    // Volume Swing
                    if (pIns->random_volume_weight)
                    {
                        // IT compatibility: MPT has a weird vol swing algorithm....
                        if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        {
                            double d = 2 * (((double) rand()) / RAND_MAX) - 1;
                            pChn->nVolSwing = d * pIns->random_volume_weight / 100.0 * pChn->nVolume;
                        } else
                        {
                            int d = ((LONG)pIns->random_volume_weight * (LONG)((rand() & 0xFF) - 0x7F)) / 128;
                            pChn->nVolSwing = (signed short)((d * pChn->nVolume + 1)/128);
                        }
                    }
                    // Pan Swing
                    if (pIns->random_pan_weight)
                    {
                        // IT compatibility: MPT has a weird pan swing algorithm....
                        if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        {
                            double d = 2 * (((double) rand()) / RAND_MAX) - 1;
                            pChn->nPanSwing = d * pIns->random_pan_weight * 4;
                        } else
                        {
                            int d = ((LONG)pIns->random_pan_weight * (LONG)((rand() & 0xFF) - 0x7F)) / 128;
                            pChn->nPanSwing = (signed short)d;
                            pChn->nRestorePanOnNewNote = pChn->nPan + 1;
                        }
                    }
                    // Cutoff Swing
                    if (pIns->random_cutoff_weight)
                    {
                        int d = ((LONG)pIns->random_cutoff_weight * (LONG)((rand() & 0xFF) - 0x7F)) / 128;
                        pChn->nCutSwing = (signed short)((d * pChn->nCutOff + 1)/128);
                        pChn->nRestoreCutoffOnNewNote = pChn->nCutOff + 1;
                    }
                    // Resonance Swing
                    if (pIns->random_resonance_weight)
                    {
                        int d = ((LONG)pIns->random_resonance_weight * (LONG)((rand() & 0xFF) - 0x7F)) / 128;
                        pChn->nResSwing = (signed short)((d * pChn->nResonance + 1)/128);
                        pChn->nRestoreResonanceOnNewNote = pChn->nResonance + 1;
                    }
                }
            }
            // IT Compatibility: Autovibrato reset
            if(!IsCompatibleMode(TRK_IMPULSETRACKER))
            {
                pChn->nAutoVibDepth = 0;
                pChn->nAutoVibPos = 0;
            }
        }
        pChn->left_volume = pChn->right_volume = 0;
        bool bFlt = (m_dwSongFlags & SONG_MPTFILTERMODE) ? false : true;
        // Setup Initial Filter for this note
        if (pIns)
        {
            if (pIns->default_filter_resonance & 0x80) { pChn->nResonance = pIns->default_filter_resonance & 0x7F; bFlt = true; }
            if (pIns->default_filter_cutoff & 0x80) { pChn->nCutOff = pIns->default_filter_cutoff & 0x7F; bFlt = true; }
            if (bFlt && (pIns->default_filter_mode != FLTMODE_UNCHANGED))
            {
                pChn->nFilterMode = pIns->default_filter_mode;
            }
        } else
        {
            pChn->nVolSwing = pChn->nPanSwing = 0;
            pChn->nCutSwing = pChn->nResSwing = 0;
        }
        if ((pChn->nCutOff < 0x7F) && (bFlt)) SetupChannelFilter(pChn, true);
    }
    // Special case for MPT
    if (bManual) bitset_remove(pChn->flags, vflag_ty::Mute);
    if ( (bitset_is_set(pChn->flags, vflag_ty::Mute) && (deprecated_global_sound_setup_bitmask & SNDMIX_MUTECHNMODE)) ||
         ((pChn->instrument) && (pChn->instrument->flags & INS_MUTE) && (!bManual)) )
    {
        if (!bManual) pChn->nPeriod = 0;
    }

}


UINT module_renderer::GetNNAChannel(UINT nChn) const
//---------------------------------------------
{
    const modplug::tracker::voice_ty *pChn = &Chn[nChn];
    // Check for empty channel
    const modplug::tracker::voice_ty *pi = &Chn[m_nChannels];
    for (UINT i=m_nChannels; i<MAX_VIRTUAL_CHANNELS; i++, pi++) if (!pi->length) return i;
    if (!pChn->nFadeOutVol) return 0;
    // All channels are used: check for lowest volume
    UINT result = 0;
    uint32_t vol = 64*65536;    // 25%
    uint32_t envpos = 0xFFFFFF;
    const modplug::tracker::voice_ty *pj = &Chn[m_nChannels];
    for (UINT j=m_nChannels; j<MAX_VIRTUAL_CHANNELS; j++, pj++)
    {
        if (!pj->nFadeOutVol) return j;
        uint32_t v = pj->nVolume;
        if (bitset_is_set(pj->flags, vflag_ty::NoteFade))
            v = v * pj->nFadeOutVol;
        else
            v <<= 16;
        if (bitset_is_set(pj->flags, vflag_ty::Loop)) v >>= 1;
        if ((v < vol) || ((v == vol) && (pj->volume_envelope.position > envpos)))
        {
            envpos = pj->volume_envelope.position;
            vol = v;
            result = j;
        }
    }
    return result;
}


void module_renderer::CheckNNA(UINT nChn, UINT instr, int note, BOOL bForceCut)
//------------------------------------------------------------------------
{
    modplug::tracker::voice_ty *pChn = &Chn[nChn];
    modplug::tracker::modinstrument_t* pHeader = 0;
    LPSTR pSample;
    if (note > 0x80) note = NoteNone;
    if (note < 1) return;
    // Always NNA cut - using
    if ((!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT|MOD_TYPE_MT2))) || (!m_nInstruments) || (bForceCut))
    {
        if ( (m_dwSongFlags & SONG_CPUVERYHIGH) ||
             (!pChn->length) ||
             bitset_is_set(pChn->flags, vflag_ty::Mute) ||
             ((!pChn->left_volume) && (!pChn->right_volume)) )
        {
            return;
        }
        UINT n = GetNNAChannel(nChn);
        if (!n) return;
        modplug::tracker::voice_ty *p = &Chn[n];
        // Copy Channel
        *p = *pChn;
        bitset_remove(p->flags, vflag_ty::Vibrato);
        bitset_remove(p->flags, vflag_ty::Tremolo);
        bitset_remove(p->flags, vflag_ty::Panbrello);
        bitset_remove(p->flags, vflag_ty::Mute);
        bitset_remove(p->flags, vflag_ty::Portamento);
        p->parent_channel = nChn+1;
        p->nCommand = 0;
        // Cut the note
        p->nFadeOutVol = 0;
        bitset_remove(p->flags, vflag_ty::NoteFade);
        bitset_remove(p->flags, vflag_ty::FastVolRamp);
        // Stop this channel
        pChn->length = pChn->sample_position = pChn->fractional_sample_position = 0;
        pChn->nROfs = pChn->nLOfs = 0;
        pChn->left_volume = pChn->right_volume = 0;
        return;
    }
    if (instr >= MAX_INSTRUMENTS) instr = 0;
    pSample = pChn->sample_data.generic;
    pHeader = pChn->instrument;
    if ((instr) && (note))
    {
        pHeader = Instruments[instr];
        if (pHeader)
        {
            UINT n = 0;
            if (note <= 0x80)
            {
                n = pHeader->Keyboard[note-1];
                note = pHeader->NoteMap[note-1];
                if ((n) && (n < MAX_SAMPLES)) pSample = Samples[n].sample_data.generic;
            }
        } else pSample = nullptr;
    }
    modplug::tracker::voice_ty *p = pChn;
    //if (!pIns) return;
    if (bitset_is_set(pChn->flags, vflag_ty::Mute)) return;

    bool applyDNAtoPlug;    //rewbs.VSTiNNA
    for (UINT i=nChn; i<MAX_VIRTUAL_CHANNELS; p++, i++)
    if ((i >= m_nChannels) || (p == pChn))
    {
        applyDNAtoPlug = false; //rewbs.VSTiNNA
        if (((p->parent_channel == nChn+1) || (p == pChn)) && (p->instrument))
        {
            bool bOk = false;
            // Duplicate Check Type
            switch(p->instrument->duplicate_check_type)
            {
            // Note
            case DCT_NOTE:
                if ((note) && (p->nNote == note) && (pHeader == p->instrument)) bOk = true;
                if (pHeader && pHeader->nMixPlug) applyDNAtoPlug = true; //rewbs.VSTiNNA
                break;
            // Sample
            case DCT_SAMPLE:
                if ((pSample) && (pSample == p->sample_data.generic)) bOk = true;
                break;
            // Instrument
            case DCT_INSTRUMENT:
                if (pHeader == p->instrument) bOk = true;
                //rewbs.VSTiNNA
                if (pHeader && pHeader->nMixPlug) applyDNAtoPlug = true;
                break;
            // Plugin
            case DCT_PLUGIN:
                if (pHeader && (pHeader->nMixPlug) && (pHeader->nMixPlug == p->instrument->nMixPlug))
                {
                    applyDNAtoPlug = true;
                    bOk = true;
                }
                //end rewbs.VSTiNNA
                break;

            }
            // Duplicate Note Action
            if (bOk)
            {
                //rewbs.VSTiNNA
                if (applyDNAtoPlug)
                {
                    switch(p->instrument->duplicate_note_action)
                    {
                    case DNA_NOTECUT:
                    case DNA_NOTEOFF:
                    case DNA_NOTEFADE:
                        //switch off duplicated note played on this plugin
                        IMixPlugin *pPlugin =  m_MixPlugins[pHeader->nMixPlug-1].pMixPlugin;
                        if (pPlugin && p->nNote)
                            pPlugin->MidiCommand(p->instrument->midi_channel, p->instrument->midi_program, p->instrument->midi_bank, p->nNote + NoteKeyOff, 0, i);
                        break;
                    }
                }
                //end rewbs.VSTiNNA

                switch(p->instrument->duplicate_note_action)
                {
                // Cut
                case DNA_NOTECUT:
                    KeyOff(i);
                    p->nVolume = 0;
                    break;
                // Note Off
                case DNA_NOTEOFF:
                    KeyOff(i);
                    break;
                // Note Fade
                case DNA_NOTEFADE:
                    bitset_add(p->flags, vflag_ty::NoteFade);
                    break;
                }
                if (!p->nVolume)
                {
                    p->nFadeOutVol = 0;
                    bitset_add(p->flags, vflag_ty::NoteFade);
                    bitset_add(p->flags, vflag_ty::FastVolRamp);
                }
            }
        }
    }

    //rewbs.VSTiNNA
    // Do we need to apply New/Duplicate Note Action to a VSTi?
    bool applyNNAtoPlug = false;
    IMixPlugin *pPlugin = NULL;
    if (pChn->instrument && pChn->instrument->midi_channel > 0 && pChn->instrument->midi_channel < 17 && pChn->nNote>0 && pChn->nNote<128) // instro sends to a midi chan
    {
        UINT nPlugin = GetBestPlugin(nChn, PRIORITISE_INSTRUMENT, RESPECT_MUTES);
        /*
        UINT nPlugin = 0;
        nPlugin = pChn->instrument->nMixPlug;                 // first try intrument VST
        if ((!nPlugin) || (nPlugin > MAX_MIXPLUGINS))  // Then try Channel VST
            nPlugin = ChnSettings[nChn].nMixPlugin;
        */
        if ((nPlugin) && (nPlugin <= MAX_MIXPLUGINS))
        {
            pPlugin =  m_MixPlugins[nPlugin-1].pMixPlugin;
            if (pPlugin)
            {
                // apply NNA to this Plug iff this plug is currently playing a note on this tracking chan
                // (and if it is playing a note, we know that would be the last note played on this chan).
                modplug::tracker::note_t note = pChn->nNote;
                // Caution: When in compatible mode, modplug::tracker::modchannel_t::nNote stores the "real" note, not the mapped note!
                if(IsCompatibleMode(TRK_IMPULSETRACKER) && note < CountOf(pChn->instrument->NoteMap))
                {
                    note = pChn->instrument->NoteMap[note - 1];
                }
                applyNNAtoPlug = pPlugin->isPlaying(note, pChn->instrument->midi_channel, nChn);
            }
        }
    }
    //end rewbs.VSTiNNA

    // New Note Action
    //if ((pChn->nVolume) && (pChn->length))
    if (((pChn->nVolume) && (pChn->length)) || applyNNAtoPlug) //rewbs.VSTiNNA
    {
        UINT n = GetNNAChannel(nChn);
        if (n)
        {
            modplug::tracker::voice_ty *p = &Chn[n];
            // Copy Channel
            *p = *pChn;
            bitset_remove(p->flags, vflag_ty::Vibrato);
            bitset_remove(p->flags, vflag_ty::Tremolo);
            bitset_remove(p->flags, vflag_ty::Panbrello);
            bitset_remove(p->flags, vflag_ty::Mute);
            bitset_remove(p->flags, vflag_ty::Portamento);

            //rewbs: Copy mute and FX status from master chan.
            //I'd like to copy other flags too, but this would change playback behaviour.
            const auto req = bitset_set(
                bitset_set(vflags_ty(), vflag_ty::Mute),
                vflag_ty::DisableDsp);
            p->flags = bitset_union(p->flags, bitset_intersect(pChn->flags, req));

            p->parent_channel = nChn+1;
            p->nCommand = 0;
            //rewbs.VSTiNNA
            if (applyNNAtoPlug && pPlugin)
            {
                //Move note to the NNA channel (odd, but makes sense with DNA stuff).
                //Actually a bad idea since it then become very hard to kill some notes.
                //pPlugin->MoveNote(pChn->nNote, pChn->instrument->midi_channel, nChn, n);
                switch(pChn->nNNA)
                {
                case NNA_NOTEOFF:
                case NNA_NOTECUT:
                case NNA_NOTEFADE:
                    //switch off note played on this plugin, on this tracker channel and midi channel
                    //pPlugin->MidiCommand(pChn->instrument->midi_channel, pChn->instrument->midi_program, pChn->nNote+0xFF, 0, n);
                    pPlugin->MidiCommand(pChn->instrument->midi_channel, pChn->instrument->midi_program, pChn->instrument->midi_bank, /*pChn->nNote+*/NoteKeyOff, 0, nChn);
                    break;
                }
            }
            //end rewbs.VSTiNNA
            // Key Off the note
            switch(pChn->nNNA)
            {
            case NNA_NOTEOFF:    KeyOff(n); break;
            case NNA_NOTECUT:
                p->nFadeOutVol = 0;
            case NNA_NOTEFADE:    bitset_add(p->flags, vflag_ty::NoteFade); break;
            }
            if (!p->nVolume)
            {
                p->nFadeOutVol = 0;
                bitset_add(p->flags, vflag_ty::NoteFade);
                bitset_add(p->flags, vflag_ty::FastVolRamp);
            }
            // Stop this channel
            pChn->length = pChn->sample_position = pChn->fractional_sample_position = 0;
            pChn->nROfs = pChn->nLOfs = 0;
        }
    }
}


BOOL module_renderer::ProcessEffects()
//-------------------------------
{
    modplug::tracker::voice_ty *pChn = Chn.data();
    modplug::tracker::rowindex_t nBreakRow = RowIndexInvalid, nPatLoopRow = RowIndexInvalid;
    modplug::tracker::orderindex_t nPosJump = modplug::tracker::OrderIndexInvalid;

// -> CODE#0010
// -> DESC="add extended parameter mechanism to pattern effects"
    modplug::tracker::modevent_t* m = nullptr;
// -! NEW_FEATURE#0010
    for (modplug::tracker::chnindex_t nChn = 0; nChn < m_nChannels; nChn++, pChn++)
    {
        UINT instr = pChn->nRowInstr;
        UINT volcmd = pChn->nRowVolCmd;
        UINT vol = pChn->nRowVolume;
        UINT cmd = pChn->nRowCommand;
        UINT param = pChn->nRowParam;
        bool bPorta = ((cmd != CmdPorta) && (cmd != CmdPortaVolSlide) && (volcmd != VolCmdPortamento)) ? false : true;

        UINT nStartTick = 0;

        bitset_remove(pChn->flags, vflag_ty::FastVolRamp);

        // Process parameter control note.
        if(pChn->nRowNote == NotePc)
        {
            const PLUGINDEX plug = pChn->nRowInstr;
            const PlugParamIndex plugparam = modplug::tracker::modevent_t::GetValueVolCol(pChn->nRowVolCmd, pChn->nRowVolume);
            const PlugParamValue value = modplug::tracker::modevent_t::GetValueEffectCol(pChn->nRowCommand, pChn->nRowParam) / PlugParamValue(modplug::tracker::modevent_t::MaxColumnValue);

            if(plug > 0 && plug <= MAX_MIXPLUGINS && m_MixPlugins[plug-1].pMixPlugin)
                m_MixPlugins[plug-1].pMixPlugin->SetParameter(plugparam, value);
        }

        // Process continuous parameter control note.
        // Row data is cleared after first tick so on following
        // ticks using channels m_nPlugParamValueStep to identify
        // the need for parameter control. The condition cmd == 0
        // is to make sure that m_nPlugParamValueStep != 0 because
        // of NOTE_PCS, not because of macro.
        if(pChn->nRowNote == NotePcSmooth || (cmd == 0 && pChn->m_nPlugParamValueStep != 0))
        {
            const bool isFirstTick = (m_dwSongFlags & SONG_FIRSTTICK) != 0;
            if(isFirstTick)
                pChn->m_RowPlug = pChn->nRowInstr;
            const PLUGINDEX nPlug = pChn->m_RowPlug;
            const bool hasValidPlug = (nPlug > 0 && nPlug <= MAX_MIXPLUGINS && m_MixPlugins[nPlug-1].pMixPlugin);
            if(hasValidPlug)
            {
                if(isFirstTick)
                    pChn->m_RowPlugParam = modplug::tracker::modevent_t::GetValueVolCol(pChn->nRowVolCmd, pChn->nRowVolume);
                const PlugParamIndex plugparam = pChn->m_RowPlugParam;
                if(isFirstTick)
                {
                    PlugParamValue targetvalue = modplug::tracker::modevent_t::GetValueEffectCol(pChn->nRowCommand, pChn->nRowParam) / PlugParamValue(modplug::tracker::modevent_t::MaxColumnValue);
                    // Hack: Use m_nPlugInitialParamValue to store the target value, not initial.
                    pChn->m_nPlugInitialParamValue = targetvalue;
                    pChn->m_nPlugParamValueStep = (targetvalue - m_MixPlugins[nPlug-1].pMixPlugin->GetParameter(plugparam)) / float(m_nMusicSpeed);
                }
                if(m_nTickCount + 1 == m_nMusicSpeed)
                {    // On last tick, set parameter exactly to target value.
                    // Note: m_nPlugInitialParamValue is used to store the target value,
                    //             not the initial value as the name suggests.
                    m_MixPlugins[nPlug-1].pMixPlugin->SetParameter(plugparam, pChn->m_nPlugInitialParamValue);
                }
                else
                    m_MixPlugins[nPlug-1].pMixPlugin->ModifyParameter(plugparam, pChn->m_nPlugParamValueStep);
            }
        }

        // Apart from changing parameters, parameter control notes are intended to be 'invisible'.
        // To achieve this, clearing the note data so that rest of the process sees the row as empty row.
        if(modplug::tracker::modevent_t::IsPcNote(pChn->nRowNote))
        {
            pChn->ClearRowCmd();
            instr = 0;
            volcmd = 0;
            vol = 0;
            cmd = 0;
            param = 0;
            bPorta = false;
        }

        // Process Invert Loop (MOD Effect, called every row)
        if((m_dwSongFlags & SONG_FIRSTTICK) == 0) InvertLoop(&Chn[nChn]);

        // Process special effects (note delay, pattern delay, pattern loop)
        if (cmd == CmdDelayCut)
        {
            //:xy --> note delay until tick x, note cut at tick x+y
            nStartTick = (param & 0xF0) >> 4;
            const UINT cutAtTick = nStartTick + (param & 0x0F);
            NoteCut(nChn, cutAtTick);
        } else
        if ((cmd == CmdModCmdEx) || (cmd == CmdS3mCmdEx))
        {
            if ((!param) && (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT))) param = pChn->nOldCmdEx; else pChn->nOldCmdEx = param;
            // Note Delay ?
            if ((param & 0xF0) == 0xD0)
            {
                nStartTick = param & 0x0F;
                if(nStartTick == 0)
                {
                    //IT compatibility 22. SD0 == SD1
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        nStartTick = 1;
                    //ST3 ignores notes with SD0 completely
                    else if(GetType() == MOD_TYPE_S3M)
                        nStartTick = UINT_MAX;
                }

                //IT compatibility 08. Handling of out-of-range delay command.
                if(nStartTick >= m_nMusicSpeed && IsCompatibleMode(TRK_IMPULSETRACKER))
                {
                    if(instr)
                    {
                        if(GetNumInstruments() < 1 && instr < MAX_SAMPLES)
                        {
                            pChn->sample = &Samples[instr];
                        }
                        else
                        {
                            if(instr < MAX_INSTRUMENTS)
                                pChn->instrument = Instruments[instr];
                        }
                    }
                    continue;
                }
            } else
            if(m_dwSongFlags & SONG_FIRSTTICK)
            {
                // Pattern Loop ?
                if ((((param & 0xF0) == 0x60) && (cmd == CmdModCmdEx))
                 || (((param & 0xF0) == 0xB0) && (cmd == CmdS3mCmdEx)))
                {
                    int nloop = PatternLoop(pChn, param & 0x0F);
                    if (nloop >= 0) nPatLoopRow = nloop;
                } else
                // Pattern Delay
                if ((param & 0xF0) == 0xE0)
                {
                    m_nPatternDelay = param & 0x0F;
                }
            }
        }

        // Handles note/instrument/volume changes
        if (m_nTickCount == nStartTick) // can be delayed by a note delay effect
        {
            UINT note = pChn->nRowNote;
            if (instr) pChn->nNewIns = instr;
            bool retrigEnv = (!note) && (instr);

            // Now it's time for some FT2 crap...
            if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
            {
                // XM: FT2 ignores a note next to a K00 effect, and a fade-out seems to be done when no volume envelope is present (not exactly the Kxx behaviour)
                if(cmd == CmdKeyOff && param == 0 && IsCompatibleMode(TRK_FASTTRACKER2))
                {
                    note = instr = 0;
                    retrigEnv = false;
                }

                // XM: Key-Off + Sample == Note Cut (BUT: Only if no instr number or volume effect is present!)
                if ((note == NoteKeyOff) && ((!instr && !vol && cmd != CmdVol) || !IsCompatibleMode(TRK_FASTTRACKER2)) && ((!pChn->instrument) || (!(pChn->instrument->volume_envelope.flags & ENV_ENABLED))))
                {
                    bitset_add(pChn->flags, vflag_ty::FastVolRamp);
                    pChn->nVolume = 0;
                    note = instr = 0;
                    retrigEnv = false;
                } else if(IsCompatibleMode(TRK_FASTTRACKER2) && !(m_dwSongFlags & SONG_FIRSTTICK))
                {
                    // XM Compatibility: Some special hacks for rogue note delays... (EDx with x > 0)
                    // Apparently anything that is next to a note delay behaves totally unpredictable in FT2. Swedish tracker logic. :)

                    // If there's a note delay but no note, retrig the last note.
                    if(note == NoteNone)
                    {
                        note = pChn->nNote - pChn->nTranspose;
                    } else if(note >= NoteMinSpecial)
                    {
                        // Gah! Note Off + Note Delay will cause envelopes to *retrigger*! How stupid is that?
                        retrigEnv = true;
                    }
                    // Stupid HACK to retrieve the last used instrument *number* for rouge note delays in XM
                    // This is only applied if the instrument column is empty and if there is either no note or a "normal" note (e.g. no note off)
                    if(instr == 0 && note <= NoteMax)
                    {
                        for(modplug::tracker::instrumentindex_t nIns = 1; nIns <= m_nInstruments; nIns++)
                        {
                            if(Instruments[nIns] == pChn->instrument)
                            {
                                instr = nIns;
                                break;
                            }
                        }
                    }
                }
            }
            if (retrigEnv) //Case: instrument with no note data.
            {
                //IT compatibility: Instrument with no note.
                if(IsCompatibleMode(TRK_IMPULSETRACKER) || (m_dwSongFlags & SONG_PT1XMODE))
                {
                    if(m_nInstruments)
                    {
                        if(instr < MAX_INSTRUMENTS && pChn->instrument != Instruments[instr])
                            note = pChn->nNote;
                    }
                    else //Case: Only samples used
                    {
                        if(instr < MAX_SAMPLES && pChn->sample_data.generic != Samples[instr].sample_data.generic)
                            note = pChn->nNote;
                    }
                }

                if (m_nInstruments)
                {
                    if (pChn->sample) pChn->nVolume = pChn->sample->default_volume;
                    if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
                    {
                        bitset_add(pChn->flags, vflag_ty::FastVolRamp);
                        ResetChannelEnvelopes(pChn);
                        // IT Compatibility: Autovibrato reset
                        if(!IsCompatibleMode(TRK_IMPULSETRACKER))
                        {
                            pChn->nAutoVibDepth = 0;
                            pChn->nAutoVibPos = 0;
                        }
                        bitset_remove(pChn->flags, vflag_ty::NoteFade);
                        pChn->nFadeOutVol = 65536;
                    }
                } else //Case: Only samples are used; no instruments.
                {
                    if (instr < MAX_SAMPLES) pChn->nVolume = Samples[instr].default_volume;
                }
                if (!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) instr = 0;
            }
            // Invalid Instrument ?
            if (instr >= MAX_INSTRUMENTS) instr = 0;

            // Note Cut/Off/Fade => ignore instrument
            if (note >= NoteMinSpecial) instr = 0;

            if ((note) && (note <= NoteMax)) pChn->nNewNote = note;

            // New Note Action ?
            if ((note) && (note <= NoteMax) && (!bPorta))
            {
                CheckNNA(nChn, instr, note, FALSE);
            }

        //rewbs.VSTnoteDelay
        #ifdef MODPLUG_TRACKER
//                    if (m_nInstruments) ProcessMidiOut(nChn, pChn);
        #endif // MODPLUG_TRACKER
        //end rewbs.VSTnoteDelay

            if(note)
            {
                if(pChn->nRestorePanOnNewNote > 0)
                {
                    pChn->nPan = pChn->nRestorePanOnNewNote - 1;
                    pChn->nRestorePanOnNewNote = 0;
                }
                if(pChn->nRestoreResonanceOnNewNote > 0)
                {
                    pChn->nResonance = pChn->nRestoreResonanceOnNewNote - 1;
                    pChn->nRestoreResonanceOnNewNote = 0;
                }
                if(pChn->nRestoreCutoffOnNewNote > 0)
                {
                    pChn->nCutOff = pChn->nRestoreCutoffOnNewNote - 1;
                    pChn->nRestoreCutoffOnNewNote = 0;
                }

            }

            // Instrument Change ?
            if (instr)
            {
                modplug::tracker::modsample_t *psmp = pChn->sample;
                InstrumentChange(pChn, instr, bPorta, true);
                pChn->nNewIns = 0;
                // Special IT case: portamento+note causes sample change -> ignore portamento
                if ((m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT))
                 && (psmp != pChn->sample) && (note) && (note < 0x80))
                {
                    bPorta = false;
                }
            }
            // New Note ?
            if (note)
            {
                if ((!instr) && (pChn->nNewIns) && (note < 0x80))
                {
                    InstrumentChange(pChn, pChn->nNewIns, bPorta, false, (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2)) ? false : true);
                    pChn->nNewIns = 0;
                }
                NoteChange(nChn, note, bPorta, (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2)) ? false : true);
                if ((bPorta) && (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2)) && (instr))
                {
                    bitset_add(pChn->flags, vflag_ty::FastVolRamp);
                    ResetChannelEnvelopes(pChn);
                    // IT Compatibility: Autovibrato reset
                    if(!IsCompatibleMode(TRK_IMPULSETRACKER))
                    {
                        pChn->nAutoVibDepth = 0;
                        pChn->nAutoVibPos = 0;
                    }
                }
            }
            // Tick-0 only volume commands
            if (volcmd == VolCmdVol)
            {
                if (vol > 64) vol = 64;
                pChn->nVolume = vol << 2;
                bitset_add(pChn->flags, vflag_ty::FastVolRamp);
            } else
            if (volcmd == VolCmdPan)
            {
                if (vol > 64) vol = 64;
                pChn->nPan = vol << 2;
                bitset_add(pChn->flags, vflag_ty::FastVolRamp);
                pChn->nRestorePanOnNewNote = 0;
                //IT compatibility 20. Set pan overrides random pan
                if(IsCompatibleMode(TRK_IMPULSETRACKER))
                    pChn->nPanSwing = 0;
            }

        //rewbs.VSTnoteDelay
        #ifdef MODPLUG_TRACKER
            if (m_nInstruments) ProcessMidiOut(nChn, pChn);
        #endif // MODPLUG_TRACKER
        }

        if((GetType() == MOD_TYPE_S3M) && (ChnSettings[nChn].dwFlags & CHN_MUTE) != 0)    // not even effects are processed on muted S3M channels
            continue;

        // Volume Column Effect (except volume & panning)
        /*    A few notes, paraphrased from ITTECH.TXT by Storlek (creator of schismtracker):
            Ex/Fx/Gx are shared with Exx/Fxx/Gxx; Ex/Fx are 4x the 'normal' slide value
            Gx is linked with Ex/Fx if Compat Gxx is off, just like Gxx is with Exx/Fxx
            Gx values: 1, 4, 8, 16, 32, 64, 96, 128, 255
            Ax/Bx/Cx/Dx values are used directly (i.e. D9 == D09), and are NOT shared with Dxx
            (value is stored into nOldVolParam and used by A0/B0/C0/D0)
            Hx uses the same value as Hxx and Uxx, and affects the *depth*
            so... hxx = (hx | (oldhxx & 0xf0))  ???
            TODO is this done correctly?
        */
        if ((volcmd > VolCmdPan) && (m_nTickCount >= nStartTick))
        {
            if (volcmd == VolCmdPortamento)
            {
                if (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT))
                    TonePortamento(pChn, ImpulseTrackerPortaVolCmd[vol & 0x0F]);
                else
                    TonePortamento(pChn, vol * 16);
            } else
            {
                // XM Compatibility: FT2 ignores some voluem commands with parameter = 0.
                if(IsCompatibleMode(TRK_FASTTRACKER2) && vol == 0)
                {
                    switch(volcmd)
                    {
                    case VolCmdVol:
                    case VolCmdPan:
                    case VolCmdVibratoDepth:
                    case VolCmdPortamento:
                        break;
                    default:
                        // no memory here.
                        volcmd = VolCmdNone;
                    }

                } else
                {
                    if(vol) pChn->nOldVolParam = vol; else vol = pChn->nOldVolParam;
                }

                switch(volcmd)
                {
                case VolCmdSlideUp:
                    VolumeSlide(pChn, vol << 4);
                    break;

                case VolCmdSlideDown:
                    VolumeSlide(pChn, vol);
                    break;

                case VolCmdFineUp:
                    if (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT))
                    {
                        if (m_nTickCount == nStartTick) VolumeSlide(pChn, (vol << 4) | 0x0F);
                    } else
                        FineVolumeUp(pChn, vol);
                    break;

                case VolCmdFineDown:
                    if (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT))
                    {
                        if (m_nTickCount == nStartTick) VolumeSlide(pChn, 0xF0 | vol);
                    } else
                        FineVolumeDown(pChn, vol);
                    break;

                case VolCmdVibratoSpeed:
                    // FT2 does not automatically enable vibrato with the "set vibrato speed" command
                    if(IsCompatibleMode(TRK_FASTTRACKER2))
                        pChn->nVibratoSpeed = vol & 0x0F;
                    else
                        Vibrato(pChn, vol << 4);
                    break;

                case VolCmdVibratoDepth:
                    Vibrato(pChn, vol);
                    break;

                case VolCmdPanSlideLeft:
                    PanningSlide(pChn, vol);
                    break;

                case VolCmdPanSlideRight:
                    PanningSlide(pChn, vol << 4);
                    break;

                case VolCmdPortamentoUp:
                    //IT compatibility (one of the first testcases - link effect memory)
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        PortamentoUp(pChn, vol << 2, true);
                    else
                        PortamentoUp(pChn, vol << 2, false);
                    break;

                case VolCmdPortamentoDown:
                    //IT compatibility (one of the first testcases - link effect memory)
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        PortamentoDown(pChn, vol << 2, true);
                    else
                        PortamentoDown(pChn, vol << 2, false);
                    break;

                case VolCmdOffset:                                    //rewbs.volOff
                    if (m_nTickCount == nStartTick)
                        SampleOffset(nChn, vol<<3, bPorta);
                    break;
                }
            }
        }

        // Effects
        if (cmd) switch (cmd)
        {
// -> CODE#0010
// -> DESC="add extended parameter mechanism to pattern effects"
        case CmdExtendedParameter:
            break;
// -> NEW_FEATURE#0010
        // Set Volume
        case CmdVol:
            if(m_dwSongFlags & SONG_FIRSTTICK)
            {
                pChn->nVolume = (param < 64) ? param*4 : 256;
                bitset_add(pChn->flags, vflag_ty::FastVolRamp);
            }
            break;

        // Portamento Up
        case CmdPortaUp:
            if ((!param) && (m_nType & MOD_TYPE_MOD)) break;
            PortamentoUp(pChn, param);
            break;

        // Portamento Down
        case CmdPortaDown:
            if ((!param) && (m_nType & MOD_TYPE_MOD)) break;
            PortamentoDown(pChn, param);
            break;

        // Volume Slide
        case CmdVolSlide:
            if ((param) || (m_nType != MOD_TYPE_MOD)) VolumeSlide(pChn, param);
            break;

        // Tone-Portamento
        case CmdPorta:
            TonePortamento(pChn, param);
            break;

        // Tone-Portamento + Volume Slide
        case CmdPortaVolSlide:
            if ((param) || (m_nType != MOD_TYPE_MOD)) VolumeSlide(pChn, param);
            TonePortamento(pChn, 0);
            break;

        // Vibrato
        case CmdVibrato:
            Vibrato(pChn, param);
            break;

        // Vibrato + Volume Slide
        case CmdVibratoVolSlide:
            if ((param) || (m_nType != MOD_TYPE_MOD)) VolumeSlide(pChn, param);
            Vibrato(pChn, 0);
            break;

        // Set Speed
        case CmdSpeed:
            if(m_dwSongFlags & SONG_FIRSTTICK)
                SetSpeed(param);
            break;

        // Set Tempo
        case CmdTempo:
// -> CODE#0010
// -> DESC="add extended parameter mechanism to pattern effects"
                m = NULL;
                if (m_nRow < Patterns[m_nPattern].GetNumRows()-1) {
                    m = Patterns[m_nPattern] + (m_nRow+1) * m_nChannels + nChn;
                }
                if (m && m->command == CmdExtendedParameter) {
                    if (m_nType & MOD_TYPE_XM) {
                        param -= 0x20; //with XM, 0x20 is the lowest tempo. Anything below changes ticks per row.
                    }
                    param = (param<<8) + m->param;
                }
// -! NEW_FEATURE#0010
                if (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT))
                {
                    if (param) pChn->nOldTempo = param; else param = pChn->nOldTempo;
                }
                if (param > GetModSpecifications().tempoMax) param = GetModSpecifications().tempoMax; // rewbs.merge: added check to avoid hyperspaced tempo!
                SetTempo(param);
            break;

        // Set Offset
        case CmdOffset:
            if (m_nTickCount) break;
            //rewbs.volOffset: moved sample offset code to own method
            SampleOffset(nChn, param, bPorta);
            break;

        // Arpeggio
        case CmdArpeggio:
            // IT compatibility 01. Don't ignore Arpeggio if no note is playing (also valid for ST3)
            if ((m_nTickCount) || (((!pChn->nPeriod) || !pChn->nNote) && !IsCompatibleMode(TRK_IMPULSETRACKER | TRK_SCREAMTRACKER))) break;
            if ((!param) && (!(m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT)))) break;
            pChn->nCommand = CmdArpeggio;
            if (param) pChn->nArpeggio = param;
            break;

        // Retrig
        case CmdRetrig:
            if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
            {
                if (!(param & 0xF0)) param |= pChn->nRetrigParam & 0xF0;
                if (!(param & 0x0F)) param |= pChn->nRetrigParam & 0x0F;
                param |= 0x100; // increment retrig count on first row
            }
            // IT compatibility 15. Retrigger
            if(IsCompatibleMode(TRK_IMPULSETRACKER))
            {
                if (param)
                    pChn->nRetrigParam = (uint8_t)(param & 0xFF);

                if (volcmd == VolCmdOffset)
                    RetrigNote(nChn, pChn->nRetrigParam, vol << 3);
                else
                    RetrigNote(nChn, pChn->nRetrigParam);
            }
            else
            {
                // XM Retrig
                if (param) pChn->nRetrigParam = (uint8_t)(param & 0xFF); else param = pChn->nRetrigParam;
                //RetrigNote(nChn, param);
                if (volcmd == VolCmdOffset)
                    RetrigNote(nChn, param, vol << 3);
                else
                    RetrigNote(nChn, param);
            }
            break;

        // Tremor
        case CmdTremor:
            if (!(m_dwSongFlags & SONG_FIRSTTICK)) break;

            // IT compatibility 12. / 13. Tremor (using modified DUMB's Tremor logic here because of old effects - http://dumb.sf.net/)
            if(IsCompatibleMode(TRK_IMPULSETRACKER))
            {
                if (param && !(m_dwSongFlags & SONG_ITOLDEFFECTS))
                {
                    // Old effects have different length interpretation (+1 for both on and off)
                    if (param & 0xf0) param -= 0x10;
                    if (param & 0x0f) param -= 0x01;
                }
                pChn->nTremorCount |= 128; // set on/off flag
            }
            else
            {
                // XM Tremor. Logic is being processed in sndmix.cpp
            }

            pChn->nCommand = CmdTremor;
            if (param) pChn->nTremorParam = param;

            break;

        // Set Global Volume
        case CmdGlobalVol:
            // ST3 applies global volume on tick 1 and does other weird things, but we won't emulate this for now.
//                     if(((GetType() & MOD_TYPE_S3M) && m_nTickCount != 1)
//                             || (!(GetType() & MOD_TYPE_S3M) && !(m_dwSongFlags & SONG_FIRSTTICK)))
//                     {
//                             break;
//                     }

            if (!(GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT))) param <<= 1;
            //IT compatibility 16. FT2, ST3 and IT ignore out-of-range values
            if(IsCompatibleMode(TRK_IMPULSETRACKER | TRK_FASTTRACKER2 | TRK_SCREAMTRACKER))
            {
                if (param <= 128)
                    m_nGlobalVolume = param << 1;
            }
            else
            {
                if (param > 128) param = 128;
                m_nGlobalVolume = param << 1;
            }
            break;

        // Global Volume Slide
        case CmdGlobalVolSlide:
            //IT compatibility 16. Saving last global volume slide param per channel (FT2/IT)
            if(IsCompatibleMode(TRK_IMPULSETRACKER | TRK_FASTTRACKER2))
                GlobalVolSlide(param, &pChn->nOldGlobalVolSlide);
            else
                GlobalVolSlide(param, &m_nOldGlbVolSlide);
            break;

        // Set 8-bit Panning
        case CmdPanning8:
            if (!(m_dwSongFlags & SONG_FIRSTTICK)) break;
            if ((m_dwSongFlags & SONG_PT1XMODE)) break;
            if (!(m_dwSongFlags & SONG_SURROUNDPAN)) bitset_add(pChn->flags, vflag_ty::Surround);
            if (m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT|MOD_TYPE_XM|MOD_TYPE_MOD|MOD_TYPE_MT2))
            {
                pChn->nPan = param;
            } else
            if (param <= 0x80)
            {
                pChn->nPan = param << 1;
            } else
            if (param == 0xA4)
            {
                bitset_add(pChn->flags, vflag_ty::Surround);
                pChn->nPan = 0x80;
            }
            bitset_add(pChn->flags, vflag_ty::FastVolRamp);
            pChn->nRestorePanOnNewNote = 0;
            //IT compatibility 20. Set pan overrides random pan
            if(IsCompatibleMode(TRK_IMPULSETRACKER))
                pChn->nPanSwing = 0;
            break;

        // Panning Slide
        case CmdPanningSlide:
            PanningSlide(pChn, param);
            break;

        // Tremolo
        case CmdTremolo:
            Tremolo(pChn, param);
            break;

        // Fine Vibrato
        case CmdFineVibrato:
            FineVibrato(pChn, param);
            break;

        // MOD/XM Exx Extended Commands
        case CmdModCmdEx:
            ExtendedMODCommands(nChn, param);
            break;

        // S3M/IT Sxx Extended Commands
        case CmdS3mCmdEx:
            ExtendedS3MCommands(nChn, param);
            break;

        // Key Off
        case CmdKeyOff:
            // This is how Key Off is supposed to sound... (in FT2 at least)
            if(IsCompatibleMode(TRK_FASTTRACKER2))
            {
                if (m_nTickCount == param)
                {
                    // XM: Key-Off + Sample == Note Cut
                    if ((!pChn->instrument) || (!(pChn->instrument->volume_envelope.flags & ENV_ENABLED)))
                    {
                        if(param == 0) // FT2 is weird....
                        {
                            bitset_add(pChn->flags, vflag_ty::NoteFade);
                        }
                        else
                        {
                            bitset_add(pChn->flags, vflag_ty::FastVolRamp);
                            pChn->nVolume = 0;
                        }
                    }
                    KeyOff(nChn);
                }
            }
            // This is how it's NOT supposed to sound...
            else
            {
                if(m_dwSongFlags & SONG_FIRSTTICK)
                    KeyOff(nChn);
            }
            break;

        // Extra-fine porta up/down
        case CmdExtraFinePortaUpDown:
            switch(param & 0xF0)
            {
            case 0x10: ExtraFinePortamentoUp(pChn, param & 0x0F); break;
            case 0x20: ExtraFinePortamentoDown(pChn, param & 0x0F); break;
            // ModPlug XM Extensions (ignore in compatible mode)
            case 0x50:
            case 0x60:
            case 0x70:
            case 0x90:
            case 0xA0:
                if(!IsCompatibleMode(TRK_FASTTRACKER2)) ExtendedS3MCommands(nChn, param);
                break;
            }
            break;

        // Set Channel Global Volume
        case CmdChannelVol:
            if ((m_dwSongFlags & SONG_FIRSTTICK) == 0) break;
            if (param <= 64)
            {
                pChn->nGlobalVol = param;
                bitset_add(pChn->flags, vflag_ty::FastVolRamp);
            }
            break;

        // Channel volume slide
        case CmdChannelVolSlide:
            ChannelVolSlide(pChn, param);
            break;

        // Panbrello (IT)
        case CmdPanbrello:
            Panbrello(pChn, param);
            break;

        // Set Envelope Position
        case CmdSetEnvelopePosition:
            if(m_dwSongFlags & SONG_FIRSTTICK)
            {
                pChn->volume_envelope.position = param;

                // XM compatibility: FT2 only sets the position of the Volume envelope
                if(!IsCompatibleMode(TRK_FASTTRACKER2))
                {
                    pChn->panning_envelope.position = param;
                    pChn->pitch_envelope.position = param;
                    if (pChn->instrument)
                    {
                        modplug::tracker::modinstrument_t *pIns = pChn->instrument;
                        if (bitset_is_set(pChn->flags, vflag_ty::PanEnvOn) && (pIns->panning_envelope.num_nodes) && (param > pIns->panning_envelope.Ticks[pIns->panning_envelope.num_nodes-1]))
                        {
                            bitset_remove(pChn->flags, vflag_ty::PanEnvOn);
                        }
                    }
                }

            }
            break;

        // Position Jump
        case CmdPositionJump:
            m_nNextPatStartRow = 0; // FT2 E60 bug
            nPosJump = param;
            if((m_dwSongFlags & SONG_PATTERNLOOP) && m_nSeqOverride == 0)
            {
                 m_nSeqOverride = param + 1;
                 //Releasing pattern loop after position jump could cause
                 //instant jumps - modifying behavior so that now position jumps
                 //occurs also when pattern loop is enabled.
            }
            // see http://forum.openmpt.org/index.php?topic=2769.0 - FastTracker resets Dxx if Bxx is called _after_ Dxx
            if(GetType() == MOD_TYPE_XM)
            {
                nBreakRow = 0;
            }
            break;

        // Pattern Break
        case CmdPatternBreak:
            if(param >= 64 && (GetType() & MOD_TYPE_S3M))
            {
                // ST3 ignores invalid pattern breaks.
                break;
            }
            m_nNextPatStartRow = 0; // FT2 E60 bug
            m = NULL;
            if (m_nRow < Patterns[m_nPattern].GetNumRows()-1)
            {
              m = Patterns[m_nPattern] + (m_nRow+1) * m_nChannels + nChn;
            }
            if (m && m->command == CmdExtendedParameter)
            {
                nBreakRow = (param << 8) + m->param;
            } else
            {
                nBreakRow = param;
            }

            if((m_dwSongFlags & SONG_PATTERNLOOP))
            {
                //If song is set to loop and a pattern break occurs we should stay on the same pattern.
                //Use nPosJump to force playback to "jump to this pattern" rather than move to next, as by default.
                //rewbs.to
                nPosJump = (int)m_nCurrentPattern;
            }
            break;

        // Midi Controller
        case CmdMidi:                    // Midi Controller (on first tick only)
        case CmdSmoothMidi:    // Midi Controller (smooth, i.e. on every tick)

            if((cmd == CmdMidi) && !(m_dwSongFlags & SONG_FIRSTTICK)) break;
            if (param < 0x80)
                ProcessMidiMacro(nChn, (cmd == CmdSmoothMidi), m_MidiCfg.szMidiSFXExt[pChn->nActiveMacro], param);
            else
                ProcessMidiMacro(nChn, (cmd == CmdSmoothMidi), m_MidiCfg.szMidiZXXExt[(param & 0x7F)], 0);
            break;

        // IMF Commands
        case CmdNoteSlideUp:
            NoteSlide(pChn, param, 1);
            break;
        case CmdNoteSlideDown:
            NoteSlide(pChn, param, -1);
            break;
        }

    } // for(...) end

    // Navigation Effects
    if(m_dwSongFlags & SONG_FIRSTTICK)
    {
        // Pattern Loop
        if (nPatLoopRow != RowIndexInvalid)
        {
            m_nNextPattern = m_nCurrentPattern;
            m_nNextRow = nPatLoopRow;
            if (m_nPatternDelay) m_nNextRow++;
            // As long as the pattern loop is running, mark the looped rows as not visited yet
            for(modplug::tracker::rowindex_t nRow = nPatLoopRow; nRow <= m_nRow; nRow++)
            {
                SetRowVisited(m_nCurrentPattern, nRow, false);
            }
        } else
        // Pattern Break / Position Jump only if no loop running
        if ((nBreakRow != RowIndexInvalid) || (nPosJump != modplug::tracker::OrderIndexInvalid))
        {
            if (nPosJump == modplug::tracker::OrderIndexInvalid) nPosJump = m_nCurrentPattern + 1;
            if (nBreakRow == RowIndexInvalid) nBreakRow = 0;
            m_dwSongFlags |= SONG_BREAKTOROW;

            if (nPosJump >= Order.size())
            {
                nPosJump = 0;
            }

            // This checks whether we're jumping to the same row we're already on.
            // Sounds pretty stupid and pointless to me. And noone else does this, either.
            //if((nPosJump != (int)m_nCurrentPattern) || (nBreakRow != (int)m_nRow))
            {
                // IT compatibility: don't reset loop count on pattern break
                if (nPosJump != (int)m_nCurrentPattern && !IsCompatibleMode(TRK_IMPULSETRACKER))
                {
                    for (modplug::tracker::chnindex_t i = 0; i < m_nChannels; i++)
                    {
                        Chn[i].nPatternLoopCount = 0;
                    }
                }
                m_nNextPattern = nPosJump;
                m_nNextRow = nBreakRow;
                m_bPatternTransitionOccurred = true;
            }
        } //Ends condition (nBreakRow >= 0) || (nPosJump >= 0)
        //SetRowVisited(m_nCurrentPattern, m_nRow);
    }
    return TRUE;
}


void module_renderer::ResetChannelEnvelopes(modplug::tracker::voice_ty* pChn)
//------------------------------------------------------
{
    ResetChannelEnvelope(pChn->volume_envelope);
    ResetChannelEnvelope(pChn->panning_envelope);
    ResetChannelEnvelope(pChn->pitch_envelope);
}


void module_renderer::ResetChannelEnvelope(modplug::tracker::modenvstate_t &env)
//------------------------------------------------------------
{
    env.position = 0;
    env.release_value = NOT_YET_RELEASED;
}


////////////////////////////////////////////////////////////
// Channels effects

void module_renderer::PortamentoUp(modplug::tracker::voice_ty *pChn, UINT param, const bool doFinePortamentoAsRegular)
//---------------------------------------------------------
{
    MidiPortamento(pChn, param); //Send midi pitch bend event if there's a plugin

    if(GetType() == MOD_TYPE_MPT && pChn->instrument && pChn->instrument->pTuning)
    {
        if(param)
            pChn->nOldPortaUpDown = param;
        else
            param = pChn->nOldPortaUpDown;

        if(param >= 0xF0 && !doFinePortamentoAsRegular)
            PortamentoFineMPT(pChn, param - 0xF0);
        else
            PortamentoMPT(pChn, param);
        return;
    }

    if (param) pChn->nOldPortaUpDown = param; else param = pChn->nOldPortaUpDown;
    if (!doFinePortamentoAsRegular && (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT|MOD_TYPE_STM)) && ((param & 0xF0) >= 0xE0))
    {
        if (param & 0x0F)
        {
            if ((param & 0xF0) == 0xF0)
            {
                FinePortamentoUp(pChn, param & 0x0F);
            } else
            if ((param & 0xF0) == 0xE0)
            {
                ExtraFinePortamentoUp(pChn, param & 0x0F);
            }
        }
        return;
    }
    // Regular Slide
    if (!(m_dwSongFlags & SONG_FIRSTTICK) || (m_nMusicSpeed == 1))  //rewbs.PortaA01fix
    {
        DoFreqSlide(pChn, -(int)(param * 4));
    }
}


void module_renderer::PortamentoDown(modplug::tracker::voice_ty *pChn, UINT param, const bool doFinePortamentoAsRegular)
//-----------------------------------------------------------
{
    MidiPortamento(pChn, -1*param); //Send midi pitch bend event if there's a plugin

    if(m_nType == MOD_TYPE_MPT && pChn->instrument && pChn->instrument->pTuning)
    {
        if(param)
            pChn->nOldPortaUpDown = param;
        else
            param = pChn->nOldPortaUpDown;

        if(param >= 0xF0 && !doFinePortamentoAsRegular)
            PortamentoFineMPT(pChn, -1*(param - 0xF0));
        else
            PortamentoMPT(pChn, -1*param);
        return;
    }

    if (param) pChn->nOldPortaUpDown = param; else param = pChn->nOldPortaUpDown;
    if (!doFinePortamentoAsRegular && (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT|MOD_TYPE_STM)) && ((param & 0xF0) >= 0xE0))
    {
        if (param & 0x0F)
        {
            if ((param & 0xF0) == 0xF0)
            {
                FinePortamentoDown(pChn, param & 0x0F);
            } else
            if ((param & 0xF0) == 0xE0)
            {
                ExtraFinePortamentoDown(pChn, param & 0x0F);
            }
        }
        return;
    }
    if (!(m_dwSongFlags & SONG_FIRSTTICK)  || (m_nMusicSpeed == 1)) {  //rewbs.PortaA01fix
        DoFreqSlide(pChn, (int)(param << 2));
    }
}

void module_renderer::MidiPortamento(modplug::tracker::voice_ty *pChn, int param)
//----------------------------------------------------------
{
    //Send midi pitch bend event if there's a plugin:
    modplug::tracker::modinstrument_t *pHeader = pChn->instrument;
    if (pHeader && pHeader->midi_channel>0 && pHeader->midi_channel<17) { // instro sends to a midi chan
        UINT nPlug = pHeader->nMixPlug;
        if ((nPlug) && (nPlug <= MAX_MIXPLUGINS)) {
            IMixPlugin *pPlug = (IMixPlugin*)m_MixPlugins[nPlug-1].pMixPlugin;
            if (pPlug) {
                pPlug->MidiPitchBend(pHeader->midi_channel, param, 0);
            }
        }
    }
}

void module_renderer::FinePortamentoUp(modplug::tracker::voice_ty *pChn, UINT param)
//-------------------------------------------------------------
{
    if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
    {
        if (param) pChn->nOldFinePortaUpDown = param; else param = pChn->nOldFinePortaUpDown;
    }
    if (m_dwSongFlags & SONG_FIRSTTICK)
    {
        if ((pChn->nPeriod) && (param))
        {
            if ((m_dwSongFlags & SONG_LINEARSLIDES) && (!(m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))))
            {
                pChn->nPeriod = _muldivr(pChn->nPeriod, LinearSlideDownTable[param & 0x0F], 65536);
            } else
            {
                pChn->nPeriod -= (int)(param * 4);
            }
            if (pChn->nPeriod < 1) pChn->nPeriod = 1;
        }
    }
}


void module_renderer::FinePortamentoDown(modplug::tracker::voice_ty *pChn, UINT param)
//---------------------------------------------------------------
{
    if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
    {
        if (param) pChn->nOldFinePortaUpDown = param; else param = pChn->nOldFinePortaUpDown;
    }
    if (m_dwSongFlags & SONG_FIRSTTICK)
    {
        if ((pChn->nPeriod) && (param))
        {
            if ((m_dwSongFlags & SONG_LINEARSLIDES) && (!(m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))))
            {
                pChn->nPeriod = _muldivr(pChn->nPeriod, LinearSlideUpTable[param & 0x0F], 65536);
            } else
            {
                pChn->nPeriod += (int)(param * 4);
            }
            if (pChn->nPeriod > 0xFFFF) pChn->nPeriod = 0xFFFF;
        }
    }
}


void module_renderer::ExtraFinePortamentoUp(modplug::tracker::voice_ty *pChn, UINT param)
//------------------------------------------------------------------
{
    if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
    {
        if (param) pChn->nOldFinePortaUpDown = param; else param = pChn->nOldFinePortaUpDown;
    }
    if (m_dwSongFlags & SONG_FIRSTTICK)
    {
        if ((pChn->nPeriod) && (param))
        {
            if ((m_dwSongFlags & SONG_LINEARSLIDES) && (!(m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))))
            {
                pChn->nPeriod = _muldivr(pChn->nPeriod, FineLinearSlideDownTable[param & 0x0F], 65536);
            } else
            {
                pChn->nPeriod -= (int)(param);
            }
            if (pChn->nPeriod < 1) pChn->nPeriod = 1;
        }
    }
}


void module_renderer::ExtraFinePortamentoDown(modplug::tracker::voice_ty *pChn, UINT param)
//--------------------------------------------------------------------
{
    if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
    {
        if (param) pChn->nOldFinePortaUpDown = param; else param = pChn->nOldFinePortaUpDown;
    }
    if (m_dwSongFlags & SONG_FIRSTTICK)
    {
        if ((pChn->nPeriod) && (param))
        {
            if ((m_dwSongFlags & SONG_LINEARSLIDES) && (!(m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))))
            {
                pChn->nPeriod = _muldivr(pChn->nPeriod, FineLinearSlideUpTable[param & 0x0F], 65536);
            } else
            {
                pChn->nPeriod += (int)(param);
            }
            if (pChn->nPeriod > 0xFFFF) pChn->nPeriod = 0xFFFF;
        }
    }
}

// Implemented for IMF compatibility, can't actually save this in any formats
// sign should be 1 (up) or -1 (down)
void module_renderer::NoteSlide(modplug::tracker::voice_ty *pChn, UINT param, int sign)
//----------------------------------------------------------------
{
    uint8_t x, y;
    if (m_dwSongFlags & SONG_FIRSTTICK)
    {
        x = param & 0xf0;
        if (x)
            pChn->nNoteSlideSpeed = (x >> 4);
        y = param & 0xf;
        if (y)
            pChn->nNoteSlideStep = y;
        pChn->nNoteSlideCounter = pChn->nNoteSlideSpeed;
    } else
    {
        if (--pChn->nNoteSlideCounter == 0)
        {
            pChn->nNoteSlideCounter = pChn->nNoteSlideSpeed;
            // update it
            pChn->nPeriod = GetPeriodFromNote
                (sign * pChn->nNoteSlideStep + GetNoteFromPeriod(pChn->nPeriod), 8363, 0);
        }
    }
}

// Portamento Slide
void module_renderer::TonePortamento(modplug::tracker::voice_ty *pChn, UINT param)
//-----------------------------------------------------------
{
    bitset_add(pChn->flags, vflag_ty::Portamento);

    //IT compatibility 03
    if(!(m_dwSongFlags & SONG_ITCOMPATGXX) && IsCompatibleMode(TRK_IMPULSETRACKER))
    {
        if(param == 0) param = pChn->nOldPortaUpDown;
        pChn->nOldPortaUpDown = param;
    }

    if(m_nType == MOD_TYPE_MPT && pChn->instrument && pChn->instrument->pTuning)
    {
        //Behavior: Param tells number of finesteps(or 'fullsteps'(notes) with glissando)
        //to slide per row(not per tick).
        const long old_PortamentoTickSlide = (m_nTickCount != 0) ? pChn->m_PortamentoTickSlide : 0;

        if(param)
            pChn->nPortamentoSlide = param;
        else
            if(pChn->nPortamentoSlide == 0)
                return;


        if((pChn->nPortamentoDest > 0 && pChn->nPortamentoSlide < 0) ||
            (pChn->nPortamentoDest < 0 && pChn->nPortamentoSlide > 0))
            pChn->nPortamentoSlide = -pChn->nPortamentoSlide;

        pChn->m_PortamentoTickSlide = (m_nTickCount + 1.0) * pChn->nPortamentoSlide / m_nMusicSpeed;

        if(bitset_is_set(pChn->flags, vflag_ty::Glissando))
        {
            pChn->m_PortamentoTickSlide *= pChn->instrument->pTuning->GetFineStepCount() + 1;
            //With glissando interpreting param as notes instead of finesteps.
        }

        const long slide = pChn->m_PortamentoTickSlide - old_PortamentoTickSlide;

        if(abs(pChn->nPortamentoDest) <= abs(slide))
        {
            if(pChn->nPortamentoDest != 0)
            {
                pChn->m_PortamentoFineSteps += pChn->nPortamentoDest;
                pChn->nPortamentoDest = 0;
                pChn->m_CalculateFreq = true;
            }
        }
        else
        {
            pChn->m_PortamentoFineSteps += slide;
            pChn->nPortamentoDest -= slide;
            pChn->m_CalculateFreq = true;
        }

        return;
    } //End candidate MPT behavior.

    if (param) pChn->nPortamentoSlide = param * 4;
    if ((pChn->nPeriod) && (pChn->nPortamentoDest) && ((m_nMusicSpeed == 1) || !(m_dwSongFlags & SONG_FIRSTTICK)))  //rewbs.PortaA01fix
    {
        if (pChn->nPeriod < pChn->nPortamentoDest)
        {
            LONG delta = (int)pChn->nPortamentoSlide;
            if ((m_dwSongFlags & SONG_LINEARSLIDES) && (!(m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))))
            {
                UINT n = pChn->nPortamentoSlide >> 2;
                if (n > 255) n = 255;
                // Return (a*b+c/2)/c - no divide error
                // Table is 65536*2(n/192)
                delta = _muldivr(pChn->nPeriod, LinearSlideUpTable[n], 65536) - pChn->nPeriod;
                if (delta < 1) delta = 1;
            }
            pChn->nPeriod += delta;
            if (pChn->nPeriod > pChn->nPortamentoDest) pChn->nPeriod = pChn->nPortamentoDest;
        } else
        if (pChn->nPeriod > pChn->nPortamentoDest)
        {
            LONG delta = - (int)pChn->nPortamentoSlide;
            if ((m_dwSongFlags & SONG_LINEARSLIDES) && (!(m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))))
            {
                UINT n = pChn->nPortamentoSlide >> 2;
                if (n > 255) n = 255;
                delta = _muldivr(pChn->nPeriod, LinearSlideDownTable[n], 65536) - pChn->nPeriod;
                if (delta > -1) delta = -1;
            }
            pChn->nPeriod += delta;
            if (pChn->nPeriod < pChn->nPortamentoDest) pChn->nPeriod = pChn->nPortamentoDest;
        }
    }

    //IT compatibility 23. Portamento with no note
    if(pChn->nPeriod == pChn->nPortamentoDest && IsCompatibleMode(TRK_IMPULSETRACKER))
        pChn->nPortamentoDest = 0;

}


void module_renderer::Vibrato(modplug::tracker::voice_ty *p, UINT param)
//-------------------------------------------------
{
    p->m_VibratoDepth = param % 16 / 15.0F;
    //'New tuning'-thing: 0 - 1 <-> No depth - Full depth.


    if (param & 0x0F) p->nVibratoDepth = (param & 0x0F) * 4;
    if (param & 0xF0) p->nVibratoSpeed = (param >> 4) & 0x0F;
    bitset_add(p->flags, vflag_ty::Vibrato);
}


void module_renderer::FineVibrato(modplug::tracker::voice_ty *p, UINT param)
//-----------------------------------------------------
{
    if (param & 0x0F) p->nVibratoDepth = param & 0x0F;
    if (param & 0xF0) p->nVibratoSpeed = (param >> 4) & 0x0F;
    bitset_add(p->flags, vflag_ty::Vibrato);
}


void module_renderer::Panbrello(modplug::tracker::voice_ty *p, UINT param)
//---------------------------------------------------
{
    if (param & 0x0F) p->nPanbrelloDepth = param & 0x0F;
    if (param & 0xF0) p->nPanbrelloSpeed = (param >> 4) & 0x0F;
    bitset_add(p->flags, vflag_ty::Panbrello);
}


void module_renderer::VolumeSlide(modplug::tracker::voice_ty *pChn, UINT param)
//--------------------------------------------------------
{
    if (param)
        pChn->nOldVolumeSlide = param;
    else
        param = pChn->nOldVolumeSlide;
    LONG newvolume = pChn->nVolume;
    if (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT|MOD_TYPE_STM|MOD_TYPE_AMF))
    {
        if ((param & 0x0F) == 0x0F) //Fine upslide or slide -15
        {
            if (param & 0xF0) //Fine upslide
            {
                FineVolumeUp(pChn, (param >> 4));
                return;
            } else //Slide -15
            {
                if ((m_dwSongFlags & SONG_FIRSTTICK) && (!(m_dwSongFlags & SONG_FASTVOLSLIDES)))
                {
                    newvolume -= 0x0F * 4;
                }
            }
        } else
        if ((param & 0xF0) == 0xF0) //Fine downslide or slide +15
        {
            if (param & 0x0F) //Fine downslide
            {
                FineVolumeDown(pChn, (param & 0x0F));
                return;
            } else //Slide +15
            {
                if ((m_dwSongFlags & SONG_FIRSTTICK) && (!(m_dwSongFlags & SONG_FASTVOLSLIDES)))
                {
                    newvolume += 0x0F * 4;
                }
            }
        }
    }
    if ((!(m_dwSongFlags & SONG_FIRSTTICK)) || (m_dwSongFlags & SONG_FASTVOLSLIDES))
    {
        // IT compatibility: Ignore slide commands with both nibbles set.
        if (param & 0x0F)
        {
            if(!IsCompatibleMode(TRK_IMPULSETRACKER) || (param & 0xF0) == 0)
                newvolume -= (int)((param & 0x0F) * 4);
        }
        else
        {
            if(!IsCompatibleMode(TRK_IMPULSETRACKER) || (param & 0x0F) == 0)
                newvolume += (int)((param & 0xF0) >> 2);
        }
        if (m_nType == MOD_TYPE_MOD) bitset_add(pChn->flags, vflag_ty::FastVolRamp);
    }
    newvolume = CLAMP(newvolume, 0, 256);

    pChn->nVolume = newvolume;
}


void module_renderer::PanningSlide(modplug::tracker::voice_ty *pChn, UINT param)
//---------------------------------------------------------
{
    LONG nPanSlide = 0;
    if (param)
        pChn->nOldPanSlide = param;
    else
        param = pChn->nOldPanSlide;
    if (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT|MOD_TYPE_STM))
    {
        if (((param & 0x0F) == 0x0F) && (param & 0xF0))
        {
            if (m_dwSongFlags & SONG_FIRSTTICK)
            {
                param = (param & 0xF0) >> 2;
                nPanSlide = - (int)param;
            }
        } else
        if (((param & 0xF0) == 0xF0) && (param & 0x0F))
        {
            if (m_dwSongFlags & SONG_FIRSTTICK)
            {
                nPanSlide = (param & 0x0F) << 2;
            }
        } else
        {
            if (!(m_dwSongFlags & SONG_FIRSTTICK))
            {
                if (param & 0x0F)
                {
                    if(!IsCompatibleMode(TRK_IMPULSETRACKER) || (param & 0xF0) == 0)
                        nPanSlide = (int)((param & 0x0F) << 2);
                } else
                {
                    if(!IsCompatibleMode(TRK_IMPULSETRACKER) || (param & 0x0F) == 0)
                        nPanSlide = -(int)((param & 0xF0) >> 2);
                }
            }
        }
    } else
    {
        if (!(m_dwSongFlags & SONG_FIRSTTICK))
        {
            // IT compatibility: Ignore slide commands with both nibbles set.
            if (param & 0x0F)
            {
                nPanSlide = -(int)((param & 0x0F) << 2);
            }
            else
            {
                nPanSlide = (int)((param & 0xF0) >> 2);
            }
            // XM compatibility: FT2's panning slide is not as deep
            if(IsCompatibleMode(TRK_FASTTRACKER2))
                nPanSlide >>= 2;
        }
    }
    if (nPanSlide)
    {
        nPanSlide += pChn->nPan;
        nPanSlide = CLAMP(nPanSlide, 0, 256);
        pChn->nPan = nPanSlide;
        pChn->nRestorePanOnNewNote = 0;
    }
}


void module_renderer::FineVolumeUp(modplug::tracker::voice_ty *pChn, UINT param)
//---------------------------------------------------------
{
    if (param) pChn->nOldFineVolUpDown = param; else param = pChn->nOldFineVolUpDown;
    if (m_dwSongFlags & SONG_FIRSTTICK)
    {
        pChn->nVolume += param * 4;
        if (pChn->nVolume > 256) pChn->nVolume = 256;
        if (m_nType & MOD_TYPE_MOD) bitset_add(pChn->flags, vflag_ty::FastVolRamp);
    }
}


void module_renderer::FineVolumeDown(modplug::tracker::voice_ty *pChn, UINT param)
//-----------------------------------------------------------
{
    if (param) pChn->nOldFineVolUpDown = param; else param = pChn->nOldFineVolUpDown;
    if (m_dwSongFlags & SONG_FIRSTTICK)
    {
        pChn->nVolume -= param * 4;
        if (pChn->nVolume < 0) pChn->nVolume = 0;
        if (m_nType & MOD_TYPE_MOD) bitset_add(pChn->flags, vflag_ty::FastVolRamp);
    }
}


void module_renderer::Tremolo(modplug::tracker::voice_ty *p, UINT param)
//-------------------------------------------------
{
    if (param & 0x0F) p->nTremoloDepth = (param & 0x0F) << 2;
    if (param & 0xF0) p->nTremoloSpeed = (param >> 4) & 0x0F;
    bitset_add(p->flags, vflag_ty::Tremolo);
}


void module_renderer::ChannelVolSlide(modplug::tracker::voice_ty *pChn, UINT param)
//------------------------------------------------------------
{
    LONG nChnSlide = 0;
    if (param) pChn->nOldChnVolSlide = param; else param = pChn->nOldChnVolSlide;
    if (((param & 0x0F) == 0x0F) && (param & 0xF0))
    {
        if (m_dwSongFlags & SONG_FIRSTTICK) nChnSlide = param >> 4;
    } else
    if (((param & 0xF0) == 0xF0) && (param & 0x0F))
    {
        if (m_dwSongFlags & SONG_FIRSTTICK) nChnSlide = - (int)(param & 0x0F);
    } else
    {
        if (!(m_dwSongFlags & SONG_FIRSTTICK))
        {
            if (param & 0x0F) nChnSlide = -(int)(param & 0x0F);
            else nChnSlide = (int)((param & 0xF0) >> 4);
        }
    }
    if (nChnSlide)
    {
        nChnSlide += pChn->nGlobalVol;
        nChnSlide = CLAMP(nChnSlide, 0, 64);
        pChn->nGlobalVol = nChnSlide;
    }
}


void module_renderer::ExtendedMODCommands(UINT nChn, UINT param)
//---------------------------------------------------------
{
    modplug::tracker::voice_ty *pChn = &Chn[nChn];
    UINT command = param & 0xF0;
    param &= 0x0F;
    switch(command)
    {
    // E0x: Set Filter
    // E1x: Fine Portamento Up
    case 0x10:    if ((param) || (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))) FinePortamentoUp(pChn, param); break;
    // E2x: Fine Portamento Down
    case 0x20:    if ((param) || (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))) FinePortamentoDown(pChn, param); break;
    // E3x: Set Glissando Control
    case 0x30:    bitset_remove(pChn->flags, vflag_ty::Glissando); if (param) bitset_add(pChn->flags, vflag_ty::Glissando); break;
    // E4x: Set Vibrato WaveForm
    case 0x40:    pChn->nVibratoType = param & 0x07; break;
    // E5x: Set FineTune
    case 0x50:    if(!(m_dwSongFlags & SONG_FIRSTTICK)) break;
                pChn->nC5Speed = S3MFineTuneTable[param];
                if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
                    pChn->nFineTune = param*2;
                else
                    pChn->nFineTune = MOD2XMFineTune(param);
                if (pChn->nPeriod) pChn->nPeriod = GetPeriodFromNote(pChn->nNote, pChn->nFineTune, pChn->nC5Speed);
                break;
    // E6x: Pattern Loop
    // E7x: Set Tremolo WaveForm
    case 0x70:    pChn->nTremoloType = param & 0x07; break;
    // E8x: Set 4-bit Panning
    case 0x80:    if((m_dwSongFlags & SONG_FIRSTTICK) && !(m_dwSongFlags & SONG_PT1XMODE))
                {
                    //IT compatibility 20. (Panning always resets surround state)
                    if(IsCompatibleMode(TRK_ALLTRACKERS))
                    {
                        if (!(m_dwSongFlags & SONG_SURROUNDPAN)) bitset_remove(pChn->flags, vflag_ty::Surround);
                    }
                    pChn->nPan = (param << 4) + 8; bitset_add(pChn->flags, vflag_ty::FastVolRamp);
                }
                break;
    // E9x: Retrig
    case 0x90:    RetrigNote(nChn, param); break;
    // EAx: Fine Volume Up
    case 0xA0:    if ((param) || (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))) FineVolumeUp(pChn, param); break;
    // EBx: Fine Volume Down
    case 0xB0:    if ((param) || (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))) FineVolumeDown(pChn, param); break;
    // ECx: Note Cut
    case 0xC0:    NoteCut(nChn, param); break;
    // EDx: Note Delay
    // EEx: Pattern Delay
    case 0xF0:
        if(GetType() == MOD_TYPE_MOD) // MOD: Invert Loop
        {
            pChn->nEFxSpeed = param;
            if(m_dwSongFlags & SONG_FIRSTTICK) InvertLoop(pChn);
        }
        else // XM: Set Active Midi Macro
        {
            pChn->nActiveMacro = param;
        }
        break;
    }
}


void module_renderer::ExtendedS3MCommands(UINT nChn, UINT param)
//---------------------------------------------------------
{
    modplug::tracker::voice_ty *pChn = &Chn[nChn];
    UINT command = param & 0xF0;
    param &= 0x0F;
    switch(command)
    {
    // S0x: Set Filter
    // S1x: Set Glissando Control
    case 0x10:    bitset_remove(pChn->flags, vflag_ty::Glissando); if (param) bitset_add(pChn->flags, vflag_ty::Glissando); break;
    // S2x: Set FineTune
    case 0x20:    if(!(m_dwSongFlags & SONG_FIRSTTICK)) break;
                pChn->nC5Speed = S3MFineTuneTable[param];
                pChn->nFineTune = MOD2XMFineTune(param);
                if (pChn->nPeriod) pChn->nPeriod = GetPeriodFromNote(pChn->nNote, pChn->nFineTune, pChn->nC5Speed);
                break;
    // S3x: Set Vibrato Waveform
    case 0x30:    if(GetType() == MOD_TYPE_S3M)
                {
                    pChn->nVibratoType = param & 0x03;
                } else
                {
                    // IT compatibility: Ignore waveform types > 3
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        pChn->nVibratoType = (param < 0x04) ? param : 0;
                    else
                        pChn->nVibratoType = param & 0x07;
                }
                break;
    // S4x: Set Tremolo Waveform
    case 0x40:    if(GetType() == MOD_TYPE_S3M)
                {
                    pChn->nTremoloType = param & 0x03;
                } else
                {
                    // IT compatibility: Ignore waveform types > 3
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        pChn->nTremoloType = (param < 0x04) ? param : 0;
                    else
                        pChn->nTremoloType = param & 0x07;
                }
                break;
    // S5x: Set Panbrello Waveform
    case 0x50:
        // IT compatibility: Ignore waveform types > 3
                if(IsCompatibleMode(TRK_IMPULSETRACKER))
                    pChn->nPanbrelloType = (param < 0x04) ? param : 0;
                else
                    pChn->nPanbrelloType = param & 0x07;
                break;
    // S6x: Pattern Delay for x frames
    case 0x60:    m_nFrameDelay = param; break;
    // S7x: Envelope Control / Instrument Control
    case 0x70:    if(!(m_dwSongFlags & SONG_FIRSTTICK)) break;
                switch(param)
                {
                case 0:
                case 1:
                case 2:
                    {
                        modplug::tracker::voice_ty *bkp = &Chn[m_nChannels];
                        for (UINT i=m_nChannels; i<MAX_VIRTUAL_CHANNELS; i++, bkp++)
                        {
                            if (bkp->parent_channel == nChn+1)
                            {
                                if (param == 1)
                                {
                                    KeyOff(i);
                                } else if (param == 2)
                                {
                                    bitset_add(bkp->flags, vflag_ty::NoteFade);
                                } else
                                {
                                    bitset_add(bkp->flags, vflag_ty::NoteFade);
                                    bkp->nFadeOutVol = 0;
                                }
                            }
                        }
                    }
                    break;
                case 3:            pChn->nNNA = NNA_NOTECUT; break;
                case 4:            pChn->nNNA = NNA_CONTINUE; break;
                case 5:            pChn->nNNA = NNA_NOTEOFF; break;
                case 6:            pChn->nNNA = NNA_NOTEFADE; break;
                case 7:            bitset_remove(pChn->flags, vflag_ty::VolEnvOn); break;
                case 8:            bitset_add(pChn->flags, vflag_ty::VolEnvOn); break;
                case 9:            bitset_remove(pChn->flags, vflag_ty::PanEnvOn); break;
                case 10:           bitset_add(pChn->flags, vflag_ty::PanEnvOn); break;
                case 11:           bitset_remove(pChn->flags, vflag_ty::PitchEnvOn); break;
                case 12:           bitset_add(pChn->flags, vflag_ty::PitchEnvOn); break;
                case 13:
                case 14:
                    if(GetType() == MOD_TYPE_MPT)
                    {
                        bitset_add(pChn->flags, vflag_ty::PitchEnvOn);
                        if(param == 13)    // pitch env on, filter env off
                        {
                            bitset_remove(pChn->flags, vflag_ty::PitchEnvAsFilter);
                        } else    // filter env on
                        {
                            bitset_add(pChn->flags, vflag_ty::PitchEnvAsFilter);
                        }
                    }
                    break;
                }
                break;
    // S8x: Set 4-bit Panning
    case 0x80:    if(m_dwSongFlags & SONG_FIRSTTICK)
                {
                    // IT Compatibility (and other trackers as well): panning disables surround (unless panning in rear channels is enabled, which is not supported by the original trackers anyway)
                    if(IsCompatibleMode(TRK_ALLTRACKERS))
                    {
                        if (!(m_dwSongFlags & SONG_SURROUNDPAN)) bitset_remove(pChn->flags, vflag_ty::Surround);
                    }
                    pChn->nPan = (param << 4) + 8; bitset_add(pChn->flags, vflag_ty::FastVolRamp);

                    //IT compatibility 20. Set pan overrides random pan
                    if(IsCompatibleMode(TRK_IMPULSETRACKER))
                        pChn->nPanSwing = 0;
                }
                break;
    // S9x: Sound Control
    case 0x90:    ExtendedChannelEffect(pChn, param); break;
    // SAx: Set 64k Offset
    case 0xA0:    if(m_dwSongFlags & SONG_FIRSTTICK)
                {
                    pChn->nOldHiOffset = param;
                    if ((pChn->nRowNote) && (pChn->nRowNote < 0x80))
                    {
                        uint32_t pos = param << 16;
                        if (pos < pChn->length) pChn->sample_position = pos;
                    }
                }
                break;
    // SBx: Pattern Loop
    // SCx: Note Cut
    case 0xC0:    NoteCut(nChn, param); break;
    // SDx: Note Delay
    // SEx: Pattern Delay for x rows
    // SFx: S3M: Not used, IT: Set Active Midi Macro
    case 0xF0:    pChn->nActiveMacro = param; break;
    }
}


void module_renderer::ExtendedChannelEffect(modplug::tracker::voice_ty *pChn, UINT param)
//------------------------------------------------------------------
{
    // S9x and X9x commands (S3M/XM/IT only)
    if(!(m_dwSongFlags & SONG_FIRSTTICK)) return;
    switch(param & 0x0F)
    {
    // S90: Surround Off
    case 0x00:    bitset_remove(pChn->flags, vflag_ty::Surround); break;
    // S91: Surround On
    case 0x01:    bitset_add(pChn->flags, vflag_ty::Surround); pChn->nPan = 128; break;
    ////////////////////////////////////////////////////////////
    // ModPlug Extensions
    // S98: Reverb Off
    case 0x08:
        break;
    // S99: Reverb On
    case 0x09:
        break;
    // S9A: 2-Channels surround mode
    case 0x0A:
        m_dwSongFlags &= ~SONG_SURROUNDPAN;
        break;
    // S9B: 4-Channels surround mode
    case 0x0B:
        m_dwSongFlags |= SONG_SURROUNDPAN;
        break;
    // S9C: IT Filter Mode
    case 0x0C:
        m_dwSongFlags &= ~SONG_MPTFILTERMODE;
        break;
    // S9D: MPT Filter Mode
    case 0x0D:
        m_dwSongFlags |= SONG_MPTFILTERMODE;
        break;
    // S9E: Go forward
    case 0x0E:
        bitset_remove(pChn->flags, vflag_ty::ScrubBackwards);
        break;
    // S9F: Go backward (set position at the end for non-looping samples)
    case 0x0F:
        if (!bitset_is_set(pChn->flags, vflag_ty::Loop) && (!pChn->sample_position) && (pChn->length))
        {
            pChn->sample_position = pChn->length - 1;
            pChn->fractional_sample_position = 0xFFFF;
        }
        bitset_add(pChn->flags, vflag_ty::ScrubBackwards);
        break;
    }
}


inline void module_renderer::InvertLoop(modplug::tracker::voice_ty *pChn)
//--------------------------------------------------
{
    // EFx implementation for MOD files (PT 1.1A and up: Invert Loop)
    // This effect trashes samples. Thanks to 8bitbubsy for making this work. :)
    if(GetType() != MOD_TYPE_MOD || pChn->nEFxSpeed == 0) return;

    // we obviously also need a sample for this
    modplug::tracker::modsample_t *pModSample = pChn->sample;
    if (pModSample == nullptr ||
        pModSample->sample_data.generic == nullptr ||
        !bitset_is_set(pModSample->flags, sflag_ty::Loop) ||
        (pModSample->sample_tag == stag_ty::Int16) )
    {
        return;
    }

    pChn->nEFxDelay += ModEFxTable[pChn->nEFxSpeed & 0x0F];
    if((pChn->nEFxDelay & 0x80) == 0) return; // only applied if the "delay" reaches 128
    pChn->nEFxDelay = 0;

    if (++pChn->nEFxOffset >= pModSample->loop_end - pModSample->loop_start)
        pChn->nEFxOffset = 0;

    // TRASH IT!!! (Yes, the sample!)
    pModSample->sample_data.generic[pModSample->loop_start + pChn->nEFxOffset] = ~pModSample->sample_data.generic[pModSample->loop_start + pChn->nEFxOffset];
    //AdjustSampleLoop(sample);
}


// Process a Midi Macro.
// Parameters:
// [in] nChn: Mod channel to apply macro on
// [in] isSmooth: If true, internal macros are interpolated between two rows
// [in] pszMidiMacro: Actual Midi Macro
// [in] param: Parameter for parametric macros
void module_renderer::ProcessMidiMacro(UINT nChn, bool isSmooth, LPCSTR pszMidiMacro, UINT param)
//------------------------------------------------------------------------------------------
{
    modplug::tracker::voice_ty *pChn = &Chn[nChn];
    uint32_t dwMacro = LittleEndian(*((uint32_t *)pszMidiMacro)) & MACRO_MASK;
    int nInternalCode;

    // Not Internal Device ?
    if (dwMacro != MACRO_INTERNAL && dwMacro != MACRO_INTERNALEX)
    {
        // we don't cater for external devices at tick resolution.
        if(isSmooth && !(m_dwSongFlags & SONG_FIRSTTICK))
        {
            return;
        }

        UINT pos = 0, nNib = 0, nBytes = 0;
        uint32_t dwMidiCode = 0, dwByteCode = 0;

        while (pos + 6 <= 32)
        {
            const CHAR cData = pszMidiMacro[pos++];
            if (!cData) break;
            if ((cData >= '0') && (cData <= '9')) { dwByteCode = (dwByteCode << 4) | (cData - '0'); nNib++; } else
            if ((cData >= 'A') && (cData <= 'F')) { dwByteCode = (dwByteCode << 4) | (cData - 'A' + 10); nNib++; } else
            if ((cData >= 'a') && (cData <= 'f')) { dwByteCode = (dwByteCode << 4) | (cData - 'a' + 10); nNib++; } else
            if ((cData == 'z') || (cData == 'Z')) { dwByteCode = param & 0x7F; nNib = 2; } else
            if ((cData == 'x') || (cData == 'X')) { dwByteCode = param & 0x70; nNib = 2; } else
            if ((cData == 'y') || (cData == 'Y')) { dwByteCode = (param & 0x0F) << 3; nNib = 2; }
            if ((cData == 'k') || (cData == 'K')) { dwByteCode = (dwByteCode << 4) | GetBestMidiChan(pChn); nNib++; }

            if (nNib >= 2)
            {
                nNib = 0;
                dwMidiCode |= dwByteCode << (nBytes * 8);
                dwByteCode = 0;
                nBytes++;

                if (nBytes >= 3)
                {
                    UINT nMasterCh = (nChn < m_nChannels) ? nChn + 1 : pChn->parent_channel;
                    if ((nMasterCh) && (nMasterCh <= m_nChannels))
                    {
// -> CODE#0015
// -> DESC="channels management dlg"
                        UINT nPlug = GetBestPlugin(nChn, PRIORITISE_CHANNEL, EVEN_IF_MUTED);
                        if (bitset_is_set(pChn->flags, vflag_ty::DisableDsp)) nPlug = 0;
// -! NEW_FEATURE#0015
                        if ((nPlug) && (nPlug <= MAX_MIXPLUGINS))
                        {
                            IMixPlugin *pPlugin = m_MixPlugins[nPlug - 1].pMixPlugin;
                            if ((pPlugin) && (m_MixPlugins[nPlug - 1].pMixState))
                            {
                                pPlugin->MidiSend(dwMidiCode);
                            }
                        }
                    }
                    nBytes = 0;
                    dwMidiCode = 0;
                }
            }

        }

        return;
    }

    // Internal device
    const bool extendedParam = (dwMacro == MACRO_INTERNALEX);

    pszMidiMacro += 4;    // skip the F0.F0 part of the macro
    // Determine which internal device is called; every internal code looks like F0.F0.yy.xx,
    // where yy is the "device" (cutoff, resonance, plugin parameter, etc.) and xx is the value.
    nInternalCode = -256;
    if ((pszMidiMacro[0] >= '0') && (pszMidiMacro[0] <= '9')) nInternalCode = (pszMidiMacro[0] - '0') << 4; else
    if ((pszMidiMacro[0] >= 'A') && (pszMidiMacro[0] <= 'F')) nInternalCode = (pszMidiMacro[0] - 'A' + 0x0A) << 4;
    if ((pszMidiMacro[1] >= '0') && (pszMidiMacro[1] <= '9')) nInternalCode += (pszMidiMacro[1] - '0'); else
    if ((pszMidiMacro[1] >= 'A') && (pszMidiMacro[1] <= 'F')) nInternalCode += (pszMidiMacro[1] - 'A' + 0x0A);
    // Filter ?
    if (nInternalCode >= 0)
    {
        CHAR cData1 = pszMidiMacro[2];
        uint32_t dwParam = 0;

        if ((cData1 == 'z') || (cData1 == 'Z'))
        {
            // parametric macro
            dwParam = param;
        } else
        {
            // fixed macro
            CHAR cData2 = pszMidiMacro[3];
            if ((cData1 >= '0') && (cData1 <= '9')) dwParam += (cData1 - '0') << 4; else
            if ((cData1 >= 'A') && (cData1 <= 'F')) dwParam += (cData1 - 'A' + 0x0A) << 4;
            if ((cData2 >= '0') && (cData2 <= '9')) dwParam += (cData2 - '0'); else
            if ((cData2 >= 'A') && (cData2 <= 'F')) dwParam += (cData2 - 'A' + 0x0A);
        }

        switch(nInternalCode)
        {
        // F0.F0.00.xx: Set CutOff
        case 0x00:
            {
                int oldcutoff = pChn->nCutOff;
                if (dwParam < 0x80)
                {
                    if(!isSmooth)
                    {
                        pChn->nCutOff = dwParam;
                    } else
                    {
                        // on the first tick only, calculate step
                        if(m_dwSongFlags & SONG_FIRSTTICK)
                        {
                            pChn->m_nPlugInitialParamValue = pChn->nCutOff;
                            // (dwParam & 0x7F) extracts the actual value that we're going to pass
                            pChn->m_nPlugParamValueStep = (float)((int)dwParam - pChn->m_nPlugInitialParamValue) / (float)m_nMusicSpeed;
                        }
                        //update param on all ticks
                        pChn->nCutOff = (uint8_t) (pChn->m_nPlugInitialParamValue + (m_nTickCount + 1) * pChn->m_nPlugParamValueStep + 0.5);
                    }
                    pChn->nRestoreCutoffOnNewNote = 0;
                }
                oldcutoff -= pChn->nCutOff;
                if (oldcutoff < 0) oldcutoff = -oldcutoff;
                if ( (pChn->nVolume > 0) ||
                     (oldcutoff < 0x10) ||
                     (!bitset_is_set(pChn->flags, vflag_ty::Filter)) ||
                     (!(pChn->left_volume|pChn->right_volume)) )
                {
                    SetupChannelFilter(pChn, !bitset_is_set(pChn->flags, vflag_ty::Filter));
                }
            }
            break;

        // F0.F0.01.xx: Set Resonance
        case 0x01:
            if (dwParam < 0x80)
            {
                pChn->nRestoreResonanceOnNewNote = 0;
                if(!isSmooth)
                {
                    pChn->nResonance = dwParam;
                } else
                {
                    // on the first tick only, calculate step
                    if(m_dwSongFlags & SONG_FIRSTTICK)
                    {
                        pChn->m_nPlugInitialParamValue = pChn->nResonance;
                        // (dwParam & 0x7F) extracts the actual value that we're going to pass
                        pChn->m_nPlugParamValueStep = (float)((int)dwParam - pChn->m_nPlugInitialParamValue) / (float)m_nMusicSpeed;
                    }
                    //update param on all ticks
                    pChn->nResonance = (uint8_t) (pChn->m_nPlugInitialParamValue + (m_nTickCount + 1) * pChn->m_nPlugParamValueStep + 0.5);
                }
            }

            SetupChannelFilter(pChn, !bitset_is_set(pChn->flags, vflag_ty::Filter));
            break;

        // F0.F0.02.xx: Set filter mode (high nibble determines filter mode)
        case 0x02:
            if (dwParam < 0x20)
            {
                pChn->nFilterMode = (dwParam >> 4);
                SetupChannelFilter(pChn, !bitset_is_set(pChn->flags, vflag_ty::Filter));
            }
            break;

    // F0.F0.03.xx: Set plug dry/wet
        case 0x03:
            {
                const UINT nPlug = GetBestPlugin(nChn, PRIORITISE_CHANNEL, EVEN_IF_MUTED);
                if ((nPlug) && (nPlug <= MAX_MIXPLUGINS) && dwParam < 0x80)
                {
                    if(!isSmooth)
                    {
                        m_MixPlugins[nPlug - 1].fDryRatio = 1.0 - (static_cast<float>(dwParam) / 127.0f);
                    } else
                    {
                        // on the first tick only, calculate step
                        if(m_dwSongFlags & SONG_FIRSTTICK)
                        {
                            pChn->m_nPlugInitialParamValue = m_MixPlugins[nPlug - 1].fDryRatio;
                            // (dwParam & 0x7F) extracts the actual value that we're going to pass
                            pChn->m_nPlugParamValueStep = ((1 - ((float)(dwParam) / 127.0f)) - pChn->m_nPlugInitialParamValue) / (float)m_nMusicSpeed;
                        }
                        //update param on all ticks
                        m_MixPlugins[nPlug - 1].fDryRatio = pChn->m_nPlugInitialParamValue + (float)(m_nTickCount + 1) * pChn->m_nPlugParamValueStep;
                    }
                }
            }
            break;


        // F0.F0.{80|n}.xx: Set VST effect parameter n to xx
        default:
            if (nInternalCode & 0x80  || extendedParam)
            {
                UINT nPlug = GetBestPlugin(nChn, PRIORITISE_CHANNEL, EVEN_IF_MUTED);
                if ((nPlug) && (nPlug <= MAX_MIXPLUGINS))
                {
                    IMixPlugin *pPlugin = m_MixPlugins[nPlug-1].pMixPlugin;
                    if ((pPlugin) && (m_MixPlugins[nPlug-1].pMixState))
                    {
                        if(!isSmooth)
                        {
                            pPlugin->SetZxxParameter(extendedParam ? (0x80 + nInternalCode) : (nInternalCode & 0x7F), dwParam & 0x7F);
                        } else
                        {
                            // on the first tick only, calculate step
                            if(m_dwSongFlags & SONG_FIRSTTICK)
                            {
                                pChn->m_nPlugInitialParamValue = pPlugin->GetZxxParameter(extendedParam ? (0x80 + nInternalCode) : (nInternalCode & 0x7F));
                                // (dwParam & 0x7F) extracts the actual value that we're going to pass
                                pChn->m_nPlugParamValueStep = ((int)(dwParam & 0x7F) - pChn->m_nPlugInitialParamValue) / (float)m_nMusicSpeed;
                            }
                            //update param on all ticks
                            pPlugin->SetZxxParameter(extendedParam ? (0x80 + nInternalCode) : (nInternalCode & 0x7F), (UINT) (pChn->m_nPlugInitialParamValue + (m_nTickCount + 1) * pChn->m_nPlugParamValueStep + 0.5));
                        }
                    }
                }
            }

        } // end switch
    } // end internal device

}


//rewbs.volOffset: moved offset code to own method as it will be used in several places now
void module_renderer::SampleOffset(UINT nChn, UINT param, bool bPorta)
//---------------------------------------------------------------
{

    modplug::tracker::voice_ty *pChn = &Chn[nChn];
// -! NEW_FEATURE#0010
// -> CODE#0010
// -> DESC="add extended parameter mechanism to pattern effects"
// rewbs.NOTE: maybe move param calculation outside of method to cope with [ effect.
            //if (param) pChn->nOldOffset = param; else param = pChn->nOldOffset;
            //param <<= 8;
            //param |= (UINT)(pChn->nOldHiOffset) << 16;
            modplug::tracker::modevent_t *m;
            m = NULL;

            if(m_nRow < Patterns[m_nPattern].GetNumRows()-1) m = Patterns[m_nPattern] + (m_nRow+1) * m_nChannels + nChn;

            if(m && m->command == CmdExtendedParameter){
                UINT tmp = m->param;
                m = NULL;
                if(m_nRow < Patterns[m_nPattern].GetNumRows()-2) m = Patterns[m_nPattern] + (m_nRow+2) * m_nChannels  + nChn;

                if(m && m->command == CmdExtendedParameter) param = (param<<16) + (tmp<<8) + m->param;
                else param = (param<<8) + tmp;
            }
            else{
                if (param) pChn->nOldOffset = param; else param = pChn->nOldOffset;
                param <<= 8;
                param |= (UINT)(pChn->nOldHiOffset) << 16;
            }
// -! NEW_FEATURE#0010

    if ((pChn->nRowNote >= NoteMin) && (pChn->nRowNote <= NoteMax))
    {
        /* if (bPorta)
            pChn->sample_position = param;
        else
            pChn->sample_position += param; */
        // The code above doesn't make sense at all. If there's a note and no porta, how on earth could sample_position be something else than 0?
        // Anyway, keeping a debug assert here, just in case...
        ASSERT(bPorta || pChn->sample_position == 0);

        // XM compatibility: Portamento + Offset = Ignore offset
        if(bPorta && IsCompatibleMode(TRK_FASTTRACKER2))
        {
            return;
        }
        pChn->sample_position = param;

        if (pChn->sample_position >= pChn->length)
        {
            // Offset beyond sample size
            if (!(m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2)))
            {
                // IT Compatibility: Offset
                if(IsCompatibleMode(TRK_IMPULSETRACKER))
                {
                    if(m_dwSongFlags & SONG_ITOLDEFFECTS)
                        pChn->sample_position = pChn->length; // Old FX: Clip to end of sample
                    else
                        pChn->sample_position = 0; // Reset to beginning of sample
                }
                else
                {
                    pChn->sample_position = pChn->loop_start;
                    if ((m_dwSongFlags & SONG_ITOLDEFFECTS) && (pChn->length > 4))
                    {
                        pChn->sample_position = pChn->length - 2;
                    }
                }
            } else if(IsCompatibleMode(TRK_FASTTRACKER2))
            {
                // XM Compatibility: Don't play note
                bitset_add(pChn->flags, vflag_ty::FastVolRamp);
                pChn->nVolume = pChn->nPeriod = 0;
            }
        }
    } else
    if ((param < pChn->length) && (m_nType & (MOD_TYPE_MTM|MOD_TYPE_DMF)))
    {
        pChn->sample_position = param;
    }

    return;
}
//end rewbs.volOffset:

void module_renderer::RetrigNote(UINT nChn, int param, UINT offset)    //rewbs.VolOffset: added offset param.
//------------------------------------------------------------
{
    // Retrig: bit 8 is set if it's the new XM retrig
    modplug::tracker::voice_ty *pChn = &Chn[nChn];
    int nRetrigSpeed = param & 0x0F;
    int nRetrigCount = pChn->nRetrigCount;
    bool bDoRetrig = false;

    //IT compatibility 15. Retrigger
    if(IsCompatibleMode(TRK_IMPULSETRACKER))
    {
        if ((m_dwSongFlags & SONG_FIRSTTICK) && pChn->nRowNote)
        {
            pChn->nRetrigCount = param & 0xf;
        }
        else if (!pChn->nRetrigCount || !--pChn->nRetrigCount)
        {
            pChn->nRetrigCount = param & 0xf;
            bDoRetrig = true;
        }
    }
    // buggy-like-hell FT2 Rxy retrig!
    else if(IsCompatibleMode(TRK_FASTTRACKER2) && (param & 0x100))
    {
        if(m_dwSongFlags & SONG_FIRSTTICK)
        {
            // here are some really stupid things FT2 does
            if(pChn->nRowVolCmd == VolCmdVol) return;
            if(pChn->nRowInstr > 0 && pChn->nRowNote == NoteNone) nRetrigCount = 1;
            if(pChn->nRowNote != NoteNone && pChn->nRowNote <= GetModSpecifications().noteMax) nRetrigCount++;
        }
        if (nRetrigCount >= nRetrigSpeed)
        {
            if (!(m_dwSongFlags & SONG_FIRSTTICK) || (pChn->nRowNote == NoteNone))
            {
                bDoRetrig = true;
                nRetrigCount = 0;
            }
        }
    } else
    {
        // old routines

        if (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT))
        {
            if (!nRetrigSpeed) nRetrigSpeed = 1;
            if ((nRetrigCount) && (!(nRetrigCount % nRetrigSpeed))) bDoRetrig = true;
            nRetrigCount++;
        } else
        {
            int realspeed = nRetrigSpeed;
            // FT2 bug: if a retrig (Rxy) occours together with a volume command, the first retrig interval is increased by one tick
            if ((param & 0x100) && (pChn->nRowVolCmd == VolCmdVol) && (pChn->nRowParam & 0xF0)) realspeed++;
            if (!(m_dwSongFlags & SONG_FIRSTTICK) || (param & 0x100))
            {
                if (!realspeed) realspeed = 1;
                if ((!(param & 0x100)) && (m_nMusicSpeed) && (!(m_nTickCount % realspeed))) bDoRetrig = true;
                nRetrigCount++;
            } else if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2)) nRetrigCount = 0;
            if (nRetrigCount >= realspeed)
            {
                if ((m_nTickCount) || ((param & 0x100) && (!pChn->nRowNote))) bDoRetrig = true;
            }
        }
    }

    if (bDoRetrig)
    {
        UINT dv = (param >> 4) & 0x0F;
        if (dv)
        {
            int vol = pChn->nVolume;

            // XM compatibility: Retrig + volume will not change volume of retrigged notes
            if(!IsCompatibleMode(TRK_FASTTRACKER2) || !(pChn->nRowVolCmd == VolCmdVol))
            {
                if (retrigTable1[dv])
                    vol = (vol * retrigTable1[dv]) >> 4;
                else
                    vol += ((int)retrigTable2[dv]) << 2;
            }

            vol = CLAMP(vol, 0, 256);

            pChn->nVolume = vol;
            bitset_add(pChn->flags, vflag_ty::FastVolRamp);
        }
        UINT nNote = pChn->nNewNote;
        LONG nOldPeriod = pChn->nPeriod;
        if ((nNote) && (nNote <= NoteMax) && (pChn->length)) CheckNNA(nChn, 0, nNote, TRUE);
        bool bResetEnv = false;
        if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
        {
            if ((pChn->nRowInstr) && (param < 0x100))
            {
                InstrumentChange(pChn, pChn->nRowInstr, false, false);
                bResetEnv = true;
            }
            if (param < 0x100) bResetEnv = true;
        }
        // IT compatibility: Really weird combination of envelopes and retrigger (see Storlek's q.it testcase)
        NoteChange(nChn, nNote, IsCompatibleMode(TRK_IMPULSETRACKER) ? true : false, bResetEnv);
        if (m_nInstruments) {
            ProcessMidiOut(nChn, pChn);    //Send retrig to Midi
        }
        if ((m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT)) && (!pChn->nRowNote) && (nOldPeriod)) pChn->nPeriod = nOldPeriod;
        if (!(m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT))) nRetrigCount = 0;
        // IT compatibility: see previous IT compatibility comment =)
        if(IsCompatibleMode(TRK_IMPULSETRACKER)) pChn->sample_position = pChn->fractional_sample_position = 0;

        if (offset)                                                                    //rewbs.volOffset: apply offset on retrig
        {
            if (pChn->sample)
                pChn->length = pChn->sample->length;
            SampleOffset(nChn, offset, false);
        }
    }

    // buggy-like-hell FT2 Rxy retrig!
    if(IsCompatibleMode(TRK_FASTTRACKER2) && (param & 0x100)) nRetrigCount++;

    // Now we can also store the retrig value for IT...
    if(!IsCompatibleMode(TRK_IMPULSETRACKER))
        pChn->nRetrigCount = (uint8_t)nRetrigCount;
}


void module_renderer::DoFreqSlide(modplug::tracker::voice_ty *pChn, LONG nFreqSlide)
//-------------------------------------------------------------
{
    // IT Linear slides
    if (!pChn->nPeriod) return;
    if ((m_dwSongFlags & SONG_LINEARSLIDES) && (!(m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))))
    {
        LONG nOldPeriod = pChn->nPeriod;
        if (nFreqSlide < 0)
        {
            UINT n = (- nFreqSlide) >> 2;
            if (n)
            {
                if (n > 255) n = 255;
                pChn->nPeriod = _muldivr(pChn->nPeriod, LinearSlideDownTable[n], 65536);
                if (pChn->nPeriod == nOldPeriod) pChn->nPeriod = nOldPeriod-1;
            }
        } else
        {
            UINT n = (nFreqSlide) >> 2;
            if (n)
            {
                if (n > 255) n = 255;
                pChn->nPeriod = _muldivr(pChn->nPeriod, LinearSlideUpTable[n], 65536);
                if (pChn->nPeriod == nOldPeriod) pChn->nPeriod = nOldPeriod+1;
            }
        }
    } else
    {
        pChn->nPeriod += nFreqSlide;
    }
    if (pChn->nPeriod < 1)
    {
        pChn->nPeriod = 1;
        if (m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))
        {
            bitset_add(pChn->flags, vflag_ty::NoteFade);
            pChn->nFadeOutVol = 0;
        }
    }
}


void module_renderer::NoteCut(UINT nChn, UINT nTick)
//---------------------------------------------
{
    if(nTick == 0)
    {
        //IT compatibility 22. SC0 == SC1
        if(IsCompatibleMode(TRK_IMPULSETRACKER))
            nTick = 1;
        // ST3 doesn't cut notes with SC0
        else if(GetType() == MOD_TYPE_S3M)
            return;
    }

    if (m_nTickCount == nTick)
    {
        modplug::tracker::voice_ty *pChn = &Chn[nChn];
        // if (m_nInstruments) KeyOff(pChn); ?
        pChn->nVolume = 0;
        // S3M/IT compatibility: Note Cut really cuts notes and does not just mute them (so that following volume commands could restore the sample)
        if(IsCompatibleMode(TRK_IMPULSETRACKER|TRK_SCREAMTRACKER))
        {
            pChn->nPeriod = 0;
        }
        bitset_add(pChn->flags, vflag_ty::FastVolRamp);

        modplug::tracker::modinstrument_t *pHeader = pChn->instrument;
        // instro sends to a midi chan
        if (pHeader && pHeader->midi_channel>0 && pHeader->midi_channel<17)
        {
            UINT nPlug = pHeader->nMixPlug;
            if ((nPlug) && (nPlug <= MAX_MIXPLUGINS))
            {
                IMixPlugin *pPlug = (IMixPlugin*)m_MixPlugins[nPlug-1].pMixPlugin;
                if (pPlug)
                {
                    pPlug->MidiCommand(pHeader->midi_channel, pHeader->midi_program, pHeader->midi_bank, /*pChn->nNote+*/NoteKeyOff, 0, nChn);
                }
            }
        }

    }
}


void module_renderer::KeyOff(UINT nChn)
//--------------------------------
{
    modplug::tracker::voice_ty *pChn = &Chn[nChn];
    const bool bKeyOn = !bitset_is_set(pChn->flags, vflag_ty::KeyOff);
    bitset_add(pChn->flags, vflag_ty::KeyOff);
    //if ((!pChn->instrument) || (!(pChn->flags & CHN_VOLENV)))
    if ((pChn->instrument) && (!bitset_is_set(pChn->flags, vflag_ty::VolEnvOn)))
    {
        bitset_add(pChn->flags, vflag_ty::NoteFade);
    }
    if (!pChn->length) return;
    if (bitset_is_set(pChn->flags, vflag_ty::SustainLoop) && (pChn->sample) && (bKeyOn))
    {
        const modplug::tracker::modsample_t *pSmp = pChn->sample;
        if (bitset_is_set(pSmp->flags, sflag_ty::Loop))
        {
            if (bitset_is_set(pSmp->flags, sflag_ty::BidiLoop)) {
                bitset_add(pChn->flags, vflag_ty::BidiLoop);
            } else {
                bitset_remove(pChn->flags, vflag_ty::BidiLoop);
                bitset_remove(pChn->flags, vflag_ty::ScrubBackwards);
            }
            bitset_add(pChn->flags, vflag_ty::Loop);
            pChn->length = pSmp->length;
            pChn->loop_start = pSmp->loop_start;
            pChn->loop_end = pSmp->loop_end;
            if (pChn->length > pChn->loop_end) pChn->length = pChn->loop_end;
        } else
        {
            bitset_remove(pChn->flags, vflag_ty::Loop);
            bitset_remove(pChn->flags, vflag_ty::BidiLoop);
            bitset_remove(pChn->flags, vflag_ty::ScrubBackwards);
            pChn->length = pSmp->length;
        }
    }
    if (pChn->instrument)
    {
        modplug::tracker::modinstrument_t *pIns = pChn->instrument;
        if (((pIns->volume_envelope.flags & ENV_LOOP) || (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))) && (pIns->fadeout))
        {
            bitset_add(pChn->flags, vflag_ty::NoteFade);
        }

        if (pIns->volume_envelope.release_node != ENV_RELEASE_NODE_UNSET)
        {
            pChn->volume_envelope.release_value = GetVolEnvValueFromPosition(pChn->volume_envelope.position, pIns);
            pChn->volume_envelope.position= pIns->volume_envelope.Ticks[pIns->volume_envelope.release_node];
        }

    }
}


//////////////////////////////////////////////////////////
// CSoundFile: Global Effects


void module_renderer::SetSpeed(UINT param)
//-----------------------------------
{
    //if ((m_nType & MOD_TYPE_S3M) && (param > 0x80)) param -= 0x80;
    // Allow high speed values here for VBlank MODs. (Maybe it would be better to have a "VBlank MOD" flag somewhere? Is it worth the effort?)
    if ((param) && (param <= GetModSpecifications().speedMax || (m_nType & MOD_TYPE_MOD))) m_nMusicSpeed = param;
}


void module_renderer::SetTempo(UINT param, bool setAsNonModcommand)
//------------------------------------------------------------
{
    const CModSpecifications& specs = GetModSpecifications();
    if(setAsNonModcommand)
    {
        if(param < specs.tempoMin) m_nMusicTempo = specs.tempoMin;
        else
        {
            if(param > specs.tempoMax) m_nMusicTempo = specs.tempoMax;
            else m_nMusicTempo = param;
        }
    }
    else
    {
        if (param >= 0x20 && (m_dwSongFlags & SONG_FIRSTTICK)) //rewbs.tempoSlideFix: only set if not (T0x or T1x) and tick is 0
        {
            m_nMusicTempo = param;
        }
        // Tempo Slide
        else if (param < 0x20 && !(m_dwSongFlags & SONG_FIRSTTICK)) //rewbs.tempoSlideFix: only slide if (T0x or T1x) and tick is not 0
        {
            if ((param & 0xF0) == 0x10)
                m_nMusicTempo += (param & 0x0F); //rewbs.tempoSlideFix: no *2
            else
                m_nMusicTempo -= (param & 0x0F); //rewbs.tempoSlideFix: no *2

        // -> CODE#0016
        // -> DESC="default tempo update"
            if(IsCompatibleMode(TRK_ALLTRACKERS))    // clamp tempo correctly in compatible mode
                m_nMusicTempo = CLAMP(m_nMusicTempo, 32, 255);
            else
                m_nMusicTempo = CLAMP(m_nMusicTempo, specs.tempoMin, specs.tempoMax);
        // -! BEHAVIOUR_CHANGE#0016
        }
    }
}


int module_renderer::PatternLoop(modplug::tracker::voice_ty *pChn, UINT param)
//-------------------------------------------------------
{
    if (param)
    {
        if (pChn->nPatternLoopCount)
        {
            pChn->nPatternLoopCount--;
            if(!pChn->nPatternLoopCount)
            {
                //IT compatibility 10. Pattern loops (+ same fix for MOD files)
                if(IsCompatibleMode(TRK_IMPULSETRACKER | TRK_PROTRACKER))
                    pChn->nPatternLoop = m_nRow + 1;

                return -1;
            }
        } else
        {
            modplug::tracker::voice_ty *p = Chn.data();

            //IT compatibility 10. Pattern loops (+ same fix for XM and MOD files)
            if(!IsCompatibleMode(TRK_IMPULSETRACKER | TRK_FASTTRACKER2 | TRK_PROTRACKER))
            {
                for (UINT i=0; i<m_nChannels; i++, p++) if (p != pChn)
                {
                    // Loop already done
                    if (p->nPatternLoopCount) return -1;
                }
            }
            pChn->nPatternLoopCount = param;
        }
        m_nNextPatStartRow = pChn->nPatternLoop; // Nasty FT2 E60 bug emulation!
        return pChn->nPatternLoop;
    } else
    {
        pChn->nPatternLoop = m_nRow;
    }
    return -1;
}


void module_renderer::GlobalVolSlide(UINT param, UINT * nOldGlobalVolSlide)
//-----------------------------------------
{
    LONG nGlbSlide = 0;
    if (param) *nOldGlobalVolSlide = param; else param = *nOldGlobalVolSlide;
    if (((param & 0x0F) == 0x0F) && (param & 0xF0))
    {
        if (m_dwSongFlags & SONG_FIRSTTICK) nGlbSlide = (param >> 4) * 2;
    } else
    if (((param & 0xF0) == 0xF0) && (param & 0x0F))
    {
        if (m_dwSongFlags & SONG_FIRSTTICK) nGlbSlide = - (int)((param & 0x0F) * 2);
    } else
    {
        if (!(m_dwSongFlags & SONG_FIRSTTICK))
        {
            if (param & 0xF0) nGlbSlide = (int)((param & 0xF0) >> 4) * 2;
            else nGlbSlide = -(int)((param & 0x0F) * 2);
        }
    }
    if (nGlbSlide)
    {
        if (!(m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) nGlbSlide *= 2;
        nGlbSlide += m_nGlobalVolume;
        nGlbSlide = CLAMP(nGlbSlide, 0, 256);
        m_nGlobalVolume = nGlbSlide;
    }
}


uint32_t module_renderer::IsSongFinished(UINT nStartOrder, UINT nStartRow) const
//----------------------------------------------------------------------
{
    UINT nOrd;

    for (nOrd=nStartOrder; nOrd<Order.size(); nOrd++)
    {
        UINT nPat = Order[nOrd];
        if (nPat != Order.GetIgnoreIndex())
        {
            if (nPat >= Patterns.Size()) break;
            const CPattern& p = Patterns[nPat];
            if (p)
            {
                UINT len = Patterns[nPat].GetNumRows() * m_nChannels;
                UINT pos = (nOrd == nStartOrder) ? nStartRow : 0;
                pos *= m_nChannels;
                while (pos < len)
                {
                    UINT cmd;
                    if ((p[pos].note) || (p[pos].volcmd)) return 0;
                    cmd = p[pos].command;
                    if (cmd == CmdModCmdEx)
                    {
                        UINT cmdex = p[pos].param & 0xF0;
                        if ((!cmdex) || (cmdex == 0x60) || (cmdex == 0xE0) || (cmdex == 0xF0)) cmd = 0;
                    }
                    if ((cmd) && (cmd != CmdSpeed) && (cmd != CmdTempo)) return 0;
                    pos++;
                }
            }
        }
    }
    return (nOrd < Order.size()) ? nOrd : Order.size()-1;
}


//////////////////////////////////////////////////////
// Note/Period/Frequency functions

UINT module_renderer::GetNoteFromPeriod(UINT period) const
//---------------------------------------------------
{
    if (!period) return 0;
    if (m_nType & (MOD_TYPE_MED|MOD_TYPE_MOD|MOD_TYPE_MTM|MOD_TYPE_669|MOD_TYPE_OKT|MOD_TYPE_AMF0))
    {
        period >>= 2;
        for (UINT i=0; i<6*12; i++)
        {
            if (period >= ProTrackerPeriodTable[i])
            {
                if ((period != ProTrackerPeriodTable[i]) && (i))
                {
                    UINT p1 = ProTrackerPeriodTable[i-1];
                    UINT p2 = ProTrackerPeriodTable[i];
                    if (p1 - period < (period - p2)) return i+36;
                }
                return i+1+36;
            }
        }
        return 6*12+36;
    } else
    {
        for (UINT i=1; i<NoteMax; i++)
        {
            LONG n = GetPeriodFromNote(i, 0, 0);
            if ((n > 0) && (n <= (LONG)period)) return i;
        }
        return NoteMax;
    }
}



UINT module_renderer::GetPeriodFromNote(UINT note, int nFineTune, UINT nC5Speed) const
//-------------------------------------------------------------------------------
{
    if ((!note) || (note >= NoteMinSpecial)) return 0;
    if (m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT|MOD_TYPE_S3M|MOD_TYPE_STM|MOD_TYPE_MDL|MOD_TYPE_ULT|MOD_TYPE_WAV
                |MOD_TYPE_FAR|MOD_TYPE_DMF|MOD_TYPE_PTM|MOD_TYPE_AMS|MOD_TYPE_DBM|MOD_TYPE_AMF|MOD_TYPE_PSM|MOD_TYPE_J2B|MOD_TYPE_IMF))
    {
        note--;
        if (m_dwSongFlags & SONG_LINEARSLIDES)
        {
            return (FreqS3MTable[note % 12] << 5) >> (note / 12);
        } else
        {
            if (!nC5Speed) nC5Speed = 8363;
            //(a*b)/c
            return _muldiv(8363, (FreqS3MTable[note % 12] << 5), nC5Speed << (note / 12));
            //8363 * freq[note%12] / nC5Speed * 2^(5-note/12)
        }
    } else if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
    {
        if (note < 13) note = 13;
        note -= 13;
        if (m_dwSongFlags & SONG_LINEARSLIDES)
        {
            LONG l = ((NoteMax - note) << 6) - (nFineTune / 2);
            if (l < 1) l = 1;
            return (UINT)l;
        } else
        {
            int finetune = nFineTune;
            UINT rnote = (note % 12) << 3;
            UINT roct = note / 12;
            int rfine = finetune / 16;
            int i = rnote + rfine + 8;
            if (i < 0) i = 0;
            if (i >= 104) i = 103;
            UINT per1 = XMPeriodTable[i];
            if ( finetune < 0 )
            {
                rfine--;
                finetune = -finetune;
            } else rfine++;
            i = rnote+rfine+8;
            if (i < 0) i = 0;
            if (i >= 104) i = 103;
            UINT per2 = XMPeriodTable[i];
            rfine = finetune & 0x0F;
            per1 *= 16-rfine;
            per2 *= rfine;
            return ((per1 + per2) << 1) >> roct;
        }
    } else {
        note--;
        nFineTune = XM2MODFineTune(nFineTune);
        if ((nFineTune) || (note < 36) || (note >= 36+6*12))
            return (ProTrackerTunedPeriods[nFineTune*12 + note % 12] << 5) >> (note / 12);
        else
            return (ProTrackerPeriodTable[note-36] << 2);
    }
}


UINT module_renderer::GetFreqFromPeriod(UINT period, UINT nC5Speed, int nPeriodFrac) const
//-----------------------------------------------------------------------------------
{
    if (!period) return 0;
    if (m_nType & (MOD_TYPE_MED|MOD_TYPE_MOD|MOD_TYPE_MTM|MOD_TYPE_669|MOD_TYPE_OKT|MOD_TYPE_AMF0))
    {
        return (3546895L*4) / period;
    } else
    if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MT2))
    {
        if (m_dwSongFlags & SONG_LINEARSLIDES)
            return XMLinearTable[period % 768] >> (period / 768);
        else
            return 8363 * 1712L / period;
    } else
    {
        if (m_dwSongFlags & SONG_LINEARSLIDES)
        {
            if (!nC5Speed) nC5Speed = 8363;
            return _muldiv(nC5Speed, 1712L << 8, (period << 8)+nPeriodFrac);
        } else
        {
            return _muldiv(8363, 1712L << 8, (period << 8)+nPeriodFrac);
        }
    }
}


UINT  module_renderer::GetBestPlugin(UINT nChn, UINT priority, bool respectMutes)
//-------------------------------------------------------------------------
{
    if (nChn > MAX_VIRTUAL_CHANNELS)            //Check valid channel number
    {
        return 0;
    }

    //Define search source order
    UINT nPlugin=0;
    switch (priority) {
        case CHANNEL_ONLY:
            nPlugin = GetChannelPlugin(nChn, respectMutes);
            break;
        case INSTRUMENT_ONLY:
            nPlugin  = GetActiveInstrumentPlugin(nChn, respectMutes);
            break;
        case PRIORITISE_INSTRUMENT:
            nPlugin  = GetActiveInstrumentPlugin(nChn, respectMutes);
            if ((!nPlugin) || (nPlugin>MAX_MIXPLUGINS)) {
                nPlugin = GetChannelPlugin(nChn, respectMutes);
            }
            break;
        case PRIORITISE_CHANNEL:
            nPlugin  = GetChannelPlugin(nChn, respectMutes);
            if ((!nPlugin) || (nPlugin>MAX_MIXPLUGINS)) {
                nPlugin = GetActiveInstrumentPlugin(nChn, respectMutes);
            }
            break;
    }

    return nPlugin; // 0 Means no plugin found.
}


UINT __cdecl module_renderer::GetChannelPlugin(UINT nChn, bool respectMutes) const
//---------------------------------------------------------------------------
{
    const modplug::tracker::voice_ty *pChn = &Chn[nChn];

    // If it looks like this is an NNA channel, we need to find the master channel.
    // This ensures we pick up the right ChnSettings.
    // NB: parent_channel==0 means no master channel, so we need to -1 to get correct index.
    if (nChn>=m_nChannels && pChn && pChn->parent_channel>0) {
        nChn = pChn->parent_channel-1;
    }

    UINT nPlugin;
    if ( (respectMutes && bitset_is_set(pChn->flags, vflag_ty::Mute)) ||
         bitset_is_set(pChn->flags, vflag_ty::DisableDsp) )
     {
        nPlugin = 0;
    } else {
        nPlugin = ChnSettings[nChn].nMixPlugin;
    }
    return nPlugin;
}


UINT module_renderer::GetActiveInstrumentPlugin(UINT nChn, bool respectMutes) const
//----------------------------------------------------------------------------
{
    const modplug::tracker::voice_ty *pChn = &Chn[nChn];
    // Unlike channel settings, instrument is copied from the original chan to the NNA chan,
    // so we don't need to worry about finding the master chan.

    UINT nPlugin=0;
    if (pChn && pChn->instrument) {
    /*
        if (respectMutes && pChn->sample && (pChn->sample->flags & CHN_MUTE)) {
            nPlugin = 0;
        } else {
        */
            nPlugin = pChn->instrument->nMixPlug;
        //}
    }
    return nPlugin;
}


UINT module_renderer::GetBestMidiChan(const modplug::tracker::voice_ty *pChn) const
//------------------------------------------------------------
{
    if (pChn && pChn->instrument && pChn->instrument->midi_channel)
    {
        return (pChn->instrument->midi_channel - 1) & 0x0F;
    }
    return 0;
}


void module_renderer::HandlePatternTransitionEvents()
//----------------------------------------------
{
    if (!m_bPatternTransitionOccurred)
        return;

    // MPT sequence override
    if ((m_nSeqOverride > 0) && (m_nSeqOverride <= Order.size()))
    {
        if (m_dwSongFlags & SONG_PATTERNLOOP)
        {
            m_nPattern = Order[m_nSeqOverride - 1];
        }
        m_nNextPattern = m_nSeqOverride - 1;
        m_nSeqOverride = 0;
    }

    // Channel mutes
    for (modplug::tracker::chnindex_t chan=0; chan<m_nChannels; chan++)
    {
        if (m_bChannelMuteTogglePending[chan])
        {
            if(m_pModDoc)
                m_pModDoc->MuteChannel(chan, !m_pModDoc->IsChannelMuted(chan));
            m_bChannelMuteTogglePending[chan] = false;
        }
    }

    m_bPatternTransitionOccurred = false;
}


// Update time signatures (global or pattern-specific). Don't forget to call this when changing the RPB/RPM settings anywhere!
void module_renderer::UpdateTimeSignature()
//------------------------------------
{
    if(!Patterns.IsValidIndex(m_nPattern) || !Patterns[m_nPattern].GetOverrideSignature())
    {
        m_nCurrentRowsPerBeat = m_nDefaultRowsPerBeat;
        m_nCurrentRowsPerMeasure = m_nDefaultRowsPerMeasure;
    } else
    {
        m_nCurrentRowsPerBeat = Patterns[m_nPattern].GetRowsPerBeat();
        m_nCurrentRowsPerMeasure = Patterns[m_nPattern].GetRowsPerMeasure();
    }
}


void module_renderer::PortamentoMPT(modplug::tracker::voice_ty* pChn, int param)
//---------------------------------------------------------
{
    //Behavior: Modifies portamento by param-steps on every tick.
    //Note that step meaning depends on tuning.

    pChn->m_PortamentoFineSteps += param;
    pChn->m_CalculateFreq = true;
}


void module_renderer::PortamentoFineMPT(modplug::tracker::voice_ty* pChn, int param)
//-------------------------------------------------------------
{
    //Behavior: Divides portamento change between ticks/row. For example
    //if Ticks/row == 6, and param == +-6, portamento goes up/down by one tuning-dependent
    //fine step every tick.

    if(m_nTickCount == 0)
        pChn->nOldFinePortaUpDown = 0;

    const int tickParam = (m_nTickCount + 1.0) * param / m_nMusicSpeed;
    pChn->m_PortamentoFineSteps += (param >= 0) ? tickParam - pChn->nOldFinePortaUpDown : tickParam + pChn->nOldFinePortaUpDown;
    if(m_nTickCount + 1 == m_nMusicSpeed)
        pChn->nOldFinePortaUpDown = abs(param);
    else
        pChn->nOldFinePortaUpDown = abs(tickParam);

    pChn->m_CalculateFreq = true;
}


/* Now, some fun code begins: This will determine if a specific row in a pattern (orderlist item)
   has been visited before. This way, we can tell when the module starts to loop, i.e. when we have determined
   the song length (or found out that a given point of the module cannot be reached).
   The concept is actually very simple: Store a boolean value for every row for every possible orderlist item.

   Specific implementations:

   Length detection code:
   As the ModPlug engine already deals with pattern loops sufficiently (though not always correctly),
   there's no problem with (infinite) pattern loops in this code.

   Normal player code:
   Bare in mind that rows inside pattern loops should only be evaluated once, or else the algorithm will cancel too early!
   So in that case, the pattern loop rows have to be reset when looping back.
*/


// Resize / Clear the row vector.
// If bReset is true, the vector is not only resized to the required dimensions, but also completely cleared (i.e. all visited rows are unset).
// If pRowVector is specified, an alternative row vector instead of the module's global one will be used (f.e. when using GetLength()).
void module_renderer::InitializeVisitedRows(const bool bReset, VisitedRowsType *pRowVector)
//------------------------------------------------------------------------------------
{
    if(pRowVector == nullptr)
    {
        pRowVector = &m_VisitedRows;
    }
    pRowVector->resize(Order.GetLengthTailTrimmed());

    for(modplug::tracker::orderindex_t nOrd = 0; nOrd < Order.GetLengthTailTrimmed(); nOrd++)
    {
        VisitedRowsBaseType *pRow = &(pRowVector->at(nOrd));
        // If we want to reset the vectors completely, we overwrite existing items with false.
        if(bReset)
        {
            pRow->assign(pRow->size(), false);
        }
        pRow->resize(GetVisitedRowsVectorSize(Order[nOrd]), false);
    }
}


// (Un)sets a given row as visited.
// nOrd, nRow - which row should be (un)set
// If bVisited is true, the row will be set as visited.
// If pRowVector is specified, an alternative row vector instead of the module's global one will be used (f.e. when using GetLength()).
void module_renderer::SetRowVisited(const modplug::tracker::orderindex_t nOrd, const modplug::tracker::rowindex_t nRow, const bool bVisited, VisitedRowsType *pRowVector)
//--------------------------------------------------------------------------------------------------------------------------
{
    const modplug::tracker::orderindex_t nMaxOrd = Order.GetLengthTailTrimmed();
    if(nOrd >= nMaxOrd || nRow >= GetVisitedRowsVectorSize(Order[nOrd]))
    {
        return;
    }

    if(pRowVector == nullptr)
    {
        pRowVector = &m_VisitedRows;
    }

    // The module might have been edited in the meantime - so we have to extend this a bit.
    if(nOrd >= pRowVector->size() || nRow >= pRowVector->at(nOrd).size())
    {
        InitializeVisitedRows(false, pRowVector);
    }

    pRowVector->at(nOrd)[nRow] = bVisited;
}


// Returns if a given row has been visited yet.
// If bAutoSet is true, the queried row will automatically be marked as visited.
// Use this parameter instead of consecutive IsRowVisited/SetRowVisited calls.
// If pRowVector is specified, an alternative row vector instead of the module's global one will be used (f.e. when using GetLength()).
bool module_renderer::IsRowVisited(const modplug::tracker::orderindex_t nOrd, const modplug::tracker::rowindex_t nRow, const bool bAutoSet, VisitedRowsType *pRowVector)
//-------------------------------------------------------------------------------------------------------------------------
{
    const modplug::tracker::orderindex_t nMaxOrd = Order.GetLengthTailTrimmed();
    if(nOrd >= nMaxOrd)
    {
        return false;
    }

    if(pRowVector == nullptr)
    {
        pRowVector = &m_VisitedRows;
    }

    // The row slot for this row has not been assigned yet - Just return false, as this means that the program has not played the row yet.
    if(nOrd >= pRowVector->size() || nRow >= pRowVector->at(nOrd).size())
    {
        if(bAutoSet)
        {
            SetRowVisited(nOrd, nRow, true, pRowVector);
        }
        return false;
    }

    if(pRowVector->at(nOrd)[nRow])
    {
        // we visited this row already - this module must be looping.
        return true;
    }

    if(bAutoSet)
    {
        pRowVector->at(nOrd)[nRow] = true;
    }

    return false;
}


// Get the needed vector size for pattern nPat.
size_t module_renderer::GetVisitedRowsVectorSize(const modplug::tracker::patternindex_t nPat)
//------------------------------------------------------------------
{
    if(Patterns.IsValidPat(nPat))
    {
        return (size_t)(Patterns[nPat].GetNumRows());
    }
    else
    {
        // invalid patterns consist of a "fake" row.
        return 1;
    }
}
