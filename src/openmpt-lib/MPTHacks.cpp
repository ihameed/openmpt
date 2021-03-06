/*
 * MPTHacks.cpp
 * ------------
 * Purpose: Find out if MOD/XM/S3M/IT modules have MPT-specific hacks and fix them.
 * Notes  : This is not finished yet.
 * Authors: OpenMPT Devs
 */

#include "stdafx.h"
#include "Moddoc.h"
#include "tracker/tracker.hpp"

using namespace modplug::tracker;

/* TODO:
stereo/16bit samples (only XM? stereo/16bit flags are defined in the S3M and IT specs...)
out-of range sample pre-amp
song flags and properties (just look at the song properties window)
+++/--- orders in XM/MOD sequence
comments in XM files
AUTOFIX actions!

maybe two options in program: convert to mptm / remove hacks from file!
*/

// Functor for fixing hacked patterns
struct FixHackedPatterns
//======================
{
    FixHackedPatterns(const CModSpecifications *originalSpecs, MODTYPE type, bool autofix, bool *foundHacks)
    {
            this->originalSpecs = originalSpecs;
            this->type = type;
            this->autofix = autofix;
            this->foundHacks = foundHacks;
            *foundHacks = false;
    }

    void operator()(modplug::tracker::modevent_t& m)
    {
            // definitely not perfect yet. :)
            // Probably missing: Some extended effect parameters
            if(!originalSpecs->HasNote(m.note))
            {
                    *foundHacks = true;
                    if(autofix)
                            m.note = NoteNone;
            }

            if(!originalSpecs->HasCommand(m.command))
            {
                    *foundHacks = true;
                    if(autofix)
                            m.command = CmdNone;
            }

            if(!originalSpecs->HasVolCommand(m.volcmd))
            {
                    *foundHacks = true;
                    if(autofix)
                            m.volcmd = VolCmdNone;
            }

            if(type == MOD_TYPE_XM)                // ModPlug XM extensions
            {
                    if(m.command == CmdExtraFinePortaUpDown && m.param >= 0x30)
                    {
                            *foundHacks = true;
                            if(autofix)
                                    m.command = CmdNone;
                    }
            } else if(type == MOD_TYPE_IT)                // ModPlug IT extensions
            {
                    if((m.command == CmdS3mCmdEx) && ((m.param >> 4) == 0x09) && (m.param != 0x91))
                    {
                            *foundHacks = true;
                            if(autofix)
                                    m.command = CmdNone;
                    }

            }
    }

    const CModSpecifications *originalSpecs;
    MODTYPE type;
    bool autofix;
    bool *foundHacks;
};


// Find and fix envelopes where two nodes are on the same tick.
bool FindIncompatibleEnvelopes(modplug::tracker::modenvelope_t &env, bool autofix)
//-------------------------------------------------------------------
{
    bool found = false;
    for(UINT i = 1; i < env.num_nodes; i++)
    {
            if(env.Ticks[i] <= env.Ticks[i - 1])        // "<=" so we can fix envelopes "on the fly"
            {
                    found = true;
                    if(autofix)
                    {
                            env.Ticks[i] = env.Ticks[i - 1] + 1;
                    }
            }
    }
    return found;
}


// Go through the module to find out if it contains any hacks introduced by (Open)MPT
bool CModDoc::HasMPTHacks(const bool autofix)
//-------------------------------------------
{
    const CModSpecifications *originalSpecs = &m_SndFile.GetModSpecifications();
    // retrieve original (not hacked) specs.
    switch(m_SndFile.GetBestSaveFormat())
    {
    case MOD_TYPE_MOD:
            originalSpecs = &ModSpecs::mod;
            break;
    case MOD_TYPE_XM:
            originalSpecs = &ModSpecs::xm;
            break;
    case MOD_TYPE_S3M:
            originalSpecs = &ModSpecs::s3m;
            break;
    case MOD_TYPE_IT:
            originalSpecs = &ModSpecs::it;
            break;
    }

    bool foundHacks = false, foundHere = false;
    CString message;
    ClearLog();

    // Check for plugins
    foundHere = false;
    for(PLUGINDEX i = 0; i < MAX_MIXPLUGINS; i++)
    {
            if(m_SndFile.m_MixPlugins[i].pMixPlugin != nullptr || m_SndFile.m_MixPlugins[i].Info.dwPluginId1 || m_SndFile.m_MixPlugins[i].Info.dwPluginId2)
            {
                    foundHere = foundHacks = true;
                    break;
            }
            // REQUIRES AUTOFIX
    }
    if(foundHere)
            AddToLog("Found VST plugins\n");

    // Pattern count
    if(m_SndFile.GetNumPatterns() > originalSpecs->patternsMax)
    {
            message.Format("Found too many patterns (%d allowed)\n", originalSpecs->patternsMax);
            AddToLog(message);
            foundHacks = true;
            // REQUIRES (INTELLIGENT) AUTOFIX
    }

    // Check for too big/small patterns
    foundHere = false;
    for(modplug::tracker::patternindex_t i = 0; i < m_SndFile.Patterns.Size(); i++)
    {
            if(m_SndFile.Patterns.IsValidPat(i))
            {
                    const modplug::tracker::rowindex_t patSize = m_SndFile.Patterns[i].GetNumRows();
                    if(patSize > originalSpecs->patternRowsMax)
                    {
                            foundHacks = foundHere = true;
                            if(autofix)
                            {
                                    // REQUIRES (INTELLIGENT) AUTOFIX
                            } else
                            {
                                    break;
                            }
                    } else if(patSize < originalSpecs->patternRowsMin)
                    {
                            foundHacks = foundHere = true;
                            if(autofix)
                            {
                                    m_SndFile.Patterns[i].Resize(originalSpecs->patternRowsMin);
                                    m_SndFile.TryWriteEffect(i, patSize - 1, CmdPatternBreak, 0, false, ChannelIndexInvalid, false, weTryNextRow);
                            } else
                            {
                                    break;
                            }
                    }
            }
    }
    if(foundHere)
    {
            message.Format("Found incompatible pattern lengths (must be between %d and %d rows)\n", originalSpecs->patternRowsMin, originalSpecs->patternRowsMax);
            AddToLog(message);
    }

    // Check for invalid pattern commands
    foundHere = false;
    m_SndFile.Patterns.ForEachModCommand(FixHackedPatterns(originalSpecs, m_SndFile.GetType(), autofix, &foundHere));
    if(foundHere)
    {
            AddToLog("Found invalid pattern commands\n");
            foundHacks = true;
    }

    // Check for pattern names
    if(m_SndFile.Patterns.GetNumNamedPatterns() > 0)
    {
            AddToLog("Found pattern names\n");
            foundHacks = true;
            if(autofix)
            {
                    for(modplug::tracker::patternindex_t i = 0; i < m_SndFile.GetNumPatterns(); i++)
                    {
                            m_SndFile.Patterns[i].SetName("");
                    }
            }
    }

    // Check for too many channels
    if(m_SndFile.GetNumChannels() > originalSpecs->channelsMax || m_SndFile.GetNumChannels() < originalSpecs->channelsMin)
    {
            message.Format("Found incompatible channel count (must be between %d and %d channels)\n", originalSpecs->channelsMin, originalSpecs->channelsMax);
            AddToLog(message);
            foundHacks = true;
            if(autofix)
            {
                    vector<bool> usedChannels;
                    CheckUsedChannels(usedChannels);
                    RemoveChannels(usedChannels);
                    // REQUIRES (INTELLIGENT) AUTOFIX
            }
    }

    // Check for channel names
    foundHere = false;
    for(modplug::tracker::chnindex_t i = 0; i < m_SndFile.GetNumChannels(); i++)
    {
            if(strcmp(m_SndFile.ChnSettings[i].szName, "") != 0)
            {
                    foundHere = foundHacks = true;
                    if(autofix)
                            MemsetZero(m_SndFile.ChnSettings[i].szName);
                    else
                            break;
            }
    }
    if(foundHere)
            AddToLog("Found channel names\n");

    // Check for too many samples
    if(m_SndFile.GetNumSamples() > originalSpecs->samplesMax)
    {
            message.Format("Found too many samples (%d allowed)\n", originalSpecs->samplesMax);
            AddToLog(message);
            foundHacks = true;
            // REQUIRES (INTELLIGENT) AUTOFIX
    }

    // Check for sample extensions
    foundHere = false;
    for(modplug::tracker::sampleindex_t i = 1; i <= m_SndFile.GetNumSamples(); i++)
    {
            modplug::tracker::modsample_t &smp = m_SndFile.Samples[i];
            if(m_SndFile.GetType() == MOD_TYPE_XM && sample_channels(smp) > 1)
            {
                    foundHere = foundHacks = true;
                    if(autofix)
                    {
                            //ctrlSmp::ConvertToMono(&smp, &m_SndFile); //XXXih: ctrlSmp
                    } else
                    {
                            break;
                    }
            }
    }
    if(foundHere)
            AddToLog("Stereo samples are not supported in the original XM format\n");

    // Check for too many instruments
    if(m_SndFile.GetNumInstruments() > originalSpecs->instrumentsMax)
    {
            message.Format("Found too many instruments (%d allowed)\n", originalSpecs->instrumentsMax);
            AddToLog(message);
            foundHacks = true;
            // REQUIRES (INTELLIGENT) AUTOFIX
    }

    // Check for instrument extensions
    foundHere = false;
    bool foundEnvelopes = false;
    for(modplug::tracker::instrumentindex_t i = 1; i <= m_SndFile.GetNumInstruments(); i++)
    {
            modplug::tracker::modinstrument_t *instr = m_SndFile.Instruments[i];
            if(instr == nullptr) continue;

            // Extended instrument attributes
            if(instr->default_filter_mode != FLTMODE_UNCHANGED || instr->volume_ramp_up != 0 || instr->resampling_mode != SRCMODE_DEFAULT ||
                    instr->random_cutoff_weight != 0 || instr->random_resonance_weight != 0 || instr->nMixPlug != 0 || instr->pitch_to_tempo_lock != 0 ||
                    instr->duplicate_check_type == DCT_PLUGIN ||
                    instr->volume_envelope.release_node != ENV_RELEASE_NODE_UNSET ||
                    instr->panning_envelope.release_node != ENV_RELEASE_NODE_UNSET ||
                    instr->pitch_envelope.release_node != ENV_RELEASE_NODE_UNSET
            )
            {
                    foundHere = foundHacks = true;
                    if(autofix)
                    {
                            instr->default_filter_mode = FLTMODE_UNCHANGED;
                            instr->volume_ramp_up = 0;
                            instr->resampling_mode = SRCMODE_DEFAULT;
                            instr->random_cutoff_weight = 0;
                            instr->random_resonance_weight = 0;
                            instr->nMixPlug = 0;
                            instr->pitch_to_tempo_lock = 0;
                            if(instr->duplicate_check_type == DCT_PLUGIN) instr->duplicate_check_type = DCT_NONE;
                            instr->volume_envelope.release_node = instr->panning_envelope.release_node = instr->pitch_envelope.release_node = ENV_RELEASE_NODE_UNSET;
                    }
            }
            // Incompatible envelope shape
            foundEnvelopes |= FindIncompatibleEnvelopes(instr->volume_envelope, autofix);
            foundEnvelopes |= FindIncompatibleEnvelopes(instr->panning_envelope, autofix);
            foundEnvelopes |= FindIncompatibleEnvelopes(instr->pitch_envelope, autofix);
            foundHacks |= foundEnvelopes;
    }
    if(foundHere)
            AddToLog("Found MPT instrument extensions\n");
    if(foundEnvelopes)
            AddToLog("Two envelope points may not share the same tick.\n");

    // Check for too many orders
    if(m_SndFile.Order.GetLengthTailTrimmed() > originalSpecs->ordersMax)
    {
            message.Format("Found too many orders (%d allowed)\n", originalSpecs->ordersMax);
            AddToLog(message);
            foundHacks = true;
            // REQUIRES (INTELLIGENT) AUTOFIX
    }

    // Check for invalid default tempo
    if(m_SndFile.m_nDefaultTempo > originalSpecs->tempoMax || m_SndFile.m_nDefaultTempo < originalSpecs->tempoMin)
    {
            message.Format("Found incompatible default tempo (must be between %d and %d)\n", originalSpecs->tempoMin, originalSpecs->tempoMax);
            AddToLog(message);
            foundHacks = true;
            if(autofix)
                    m_SndFile.m_nDefaultTempo = CLAMP(m_SndFile.m_nDefaultTempo, originalSpecs->tempoMin, originalSpecs->tempoMax);
    }

    // Check for invalid default speed
    if(m_SndFile.m_nDefaultSpeed > originalSpecs->speedMax || m_SndFile.m_nDefaultSpeed < originalSpecs->speedMin)
    {
            message.Format("Found incompatible default speed (must be between %d and %d)\n", originalSpecs->speedMin, originalSpecs->speedMax);
            AddToLog(message);
            foundHacks = true;
            if(autofix)
                    m_SndFile.m_nDefaultSpeed = CLAMP(m_SndFile.m_nDefaultSpeed, originalSpecs->speedMin, originalSpecs->speedMax);
    }

    // Check for invalid rows per beat / measure values
    if(m_SndFile.m_nDefaultRowsPerBeat >= originalSpecs->patternRowsMax || m_SndFile.m_nDefaultRowsPerMeasure >= originalSpecs->patternRowsMax)
    {
            AddToLog("Found incompatible rows per beat / measure\n");
            foundHacks = true;
            if(autofix)
            {
                    m_SndFile.m_nDefaultRowsPerBeat = CLAMP(m_SndFile.m_nDefaultRowsPerBeat, 1, (originalSpecs->patternRowsMax - 1));
                    m_SndFile.m_nDefaultRowsPerMeasure = CLAMP(m_SndFile.m_nDefaultRowsPerMeasure, m_SndFile.m_nDefaultRowsPerBeat, (originalSpecs->patternRowsMax - 1));
            }
    }

    // Find pattern-specific time signatures
    if(!originalSpecs->hasPatternSignatures)
    {
            foundHere = false;
            for(modplug::tracker::patternindex_t i = 0; i < m_SndFile.GetNumPatterns(); i++)
            {
                    if(m_SndFile.Patterns[i].GetOverrideSignature())
                    {
                            if(!foundHere)
                                    AddToLog("Found pattern-specific time signatures\n");

                            if(autofix)
                                    m_SndFile.Patterns[i].RemoveSignature();

                            foundHacks = foundHere = true;
                            if(!autofix)
                                    break;
                    }
            }
    }

    // Check for new tempo modes
    if(m_SndFile.m_nTempoMode != tempo_mode_classic)
    {
            AddToLog("Found incompatible tempo mode (only classic tempo mode allowed)\n");
            foundHacks = true;
            if(autofix)
                    m_SndFile.m_nTempoMode = tempo_mode_classic;
    }

    // Check for extended filter range flag
    if(m_SndFile.m_dwSongFlags & SONG_EXFILTERRANGE)
    {
            AddToLog("Found extended filter range\n");
            foundHacks = true;
            if(autofix)
                    m_SndFile.m_dwSongFlags &= ~SONG_EXFILTERRANGE;
    }

    // Embedded MIDI configuration in XM files
    if((m_SndFile.m_dwSongFlags & SONG_EMBEDMIDICFG) != 0 && m_SndFile.GetType() == MOD_TYPE_XM)
    {
            AddToLog("Found embedded MIDI macros\n");
            foundHacks = true;
            if(autofix)
                    m_SndFile.m_dwSongFlags &= ~SONG_EMBEDMIDICFG;
    }

    // Player flags
    if((m_SndFile.GetType() & (MOD_TYPE_XM|MOD_TYPE_IT)) && !m_SndFile.GetModFlag(MSF_COMPATIBLE_PLAY))
    {
            AddToLog("Compatible play is deactivated\n");
            foundHacks = true;
            if(autofix)
                    m_SndFile.SetModFlag(MSF_COMPATIBLE_PLAY, true);
    }

    // Check for restart position where it should not be
    if(m_SndFile.m_nRestartPos > 0 && !originalSpecs->hasRestartPos)
    {
            AddToLog("Found restart position\n");
            foundHacks = true;
            if(autofix)
            {
                    RestartPosToPattern();
            }
    }

    if(m_SndFile.m_nMixLevels != mixLevels_compatible)
    {
            AddToLog("Found incorrect mix levels (only compatible mix levels allowed)\n");
            foundHacks = true;
            if(autofix)
                    m_SndFile.m_nMixLevels = mixLevels_compatible;
    }

    if(autofix && foundHacks)
            SetModified();

    return foundHacks;
}
