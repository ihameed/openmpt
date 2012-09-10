/*
 * modcommand.cpp
 * --------------
 * Purpose: Various functions for writing effects to patterns, converting MODCOMMANDs, etc.
 * Authors: OpenMPT Devs
 *
 */

#include "stdafx.h"

#include "Sndfile.h"

#include "../tracker/tracker.h"

using namespace modplug::tracker;

extern uint8_t ImpulseTrackerPortaVolCmd[16];


// Convert an Exx command (MOD) to Sxx command (S3M)
void module_renderer::MODExx2S3MSxx(modplug::tracker::modevent_t *m)
//-------------------------------------------
{
    if(m->command != CmdModCmdEx) return;
    m->command = CmdS3mCmdEx;
    switch(m->param & 0xF0)
    {
    case 0x10: m->command = CmdPortaUp; m->param |= 0xF0; break;
    case 0x20: m->command = CmdPortaDown; m->param |= 0xF0; break;
    case 0x30: m->param = (m->param & 0x0F) | 0x10; break;
    case 0x40: m->param = (m->param & 0x03) | 0x30; break;
    case 0x50: m->param = (m->param & 0x0F) | 0x20; break;
    case 0x60: m->param = (m->param & 0x0F) | 0xB0; break;
    case 0x70: m->param = (m->param & 0x03) | 0x40; break;
    case 0x90: m->command = CmdRetrig; m->param = (m->param & 0x0F); break;
    case 0xA0: if (m->param & 0x0F) { m->command = CmdVolumeSlide; m->param = (m->param << 4) | 0x0F; } else m->command = CmdNone; break;
    case 0xB0: if (m->param & 0x0F) { m->command = CmdVolumeSlide; m->param |= 0xF0; } else m->command = CmdNone; break;
    case 0xC0: if (m->param == 0xC0) { m->command = CmdNone; m->note = NoteNoteCut; }        // this does different things in IT and ST3
    case 0xD0: if (m->param == 0xD0) { m->command = CmdNone; }        // dito
                            // rest are the same
    }
}


// Convert an Sxx command (S3M) to Exx command (MOD)
void module_renderer::S3MSxx2MODExx(modplug::tracker::modevent_t *m)
//-------------------------------------------
{
    if(m->command != CmdS3mCmdEx) return;
    m->command = CmdModCmdEx;
    switch(m->param & 0xF0)
    {
    case 0x10: m->param = (m->param & 0x0F) | 0x30; break;
    case 0x20: m->param = (m->param & 0x0F) | 0x50; break;
    case 0x30: m->param = (m->param & 0x0F) | 0x40; break;
    case 0x40: m->param = (m->param & 0x0F) | 0x70; break;
    case 0x50:
    case 0x60:
    case 0x90:
    case 0xA0: m->command = CmdExtraFinePortaUpDown; break;
    case 0xB0: m->param = (m->param & 0x0F) | 0x60; break;
    case 0x70: m->command = CmdNone;        // No NNA / envelope control in MOD/XM format
            // rest are the same
    }
}


// Convert a mod command from one format to another.
void module_renderer::ConvertCommand(modplug::tracker::modevent_t *m, MODTYPE nOldType, MODTYPE nNewType)
//--------------------------------------------------------------------------------
{
    // helper variables
    const bool oldTypeIsMOD        = (nOldType == MOD_TYPE_MOD);
    const bool oldTypeIsXM         = (nOldType == MOD_TYPE_XM);
    const bool oldTypeIsS3M        = (nOldType == MOD_TYPE_S3M);
    const bool oldTypeIsIT         = (nOldType == MOD_TYPE_IT);
    const bool oldTypeIsMPT        = (nOldType == MOD_TYPE_MPT);
    const bool oldTypeIsMOD_XM     = (oldTypeIsMOD || oldTypeIsXM);
    const bool oldTypeIsS3M_IT_MPT = (oldTypeIsS3M || oldTypeIsIT || oldTypeIsMPT);
    const bool oldTypeIsIT_MPT     = (oldTypeIsIT || oldTypeIsMPT);

    const bool newTypeIsMOD        = (nNewType == MOD_TYPE_MOD);
    const bool newTypeIsXM         = (nNewType == MOD_TYPE_XM);
    const bool newTypeIsS3M        = (nNewType == MOD_TYPE_S3M);
    const bool newTypeIsIT         = (nNewType == MOD_TYPE_IT);
    const bool newTypeIsMPT        = (nNewType == MOD_TYPE_MPT);
    const bool newTypeIsMOD_XM     = (newTypeIsMOD || newTypeIsXM);
    const bool newTypeIsS3M_IT_MPT = (newTypeIsS3M || newTypeIsIT || newTypeIsMPT);
    const bool newTypeIsIT_MPT     = (newTypeIsIT || newTypeIsMPT);

    //////////////////////////
    // Convert 8-bit Panning
    if(m->command == CmdPanning8)
    {
            if(newTypeIsS3M)
            {
                    m->param = (m->param + 1) >> 1;
            }
            else if(oldTypeIsS3M)
            {
                    if(m->param == 0xA4)
                    {
                            // surround remap
                            m->command = (nNewType & (MOD_TYPE_IT|MOD_TYPE_MPT)) ? CmdS3mCmdEx : CmdExtraFinePortaUpDown;
                            m->param = 0x91;
                    }
                    else
                    {
                            m->param = min(m->param << 1, 0xFF);
                    }
            }
    } // End if(m->command == CMD_PANNING8)

    ///////////////////////////////////////////////////////////////////////////////////////
    // MPTM to anything: Convert param control, extended envelope control, note delay+cut
    if(oldTypeIsMPT)
    {
            if(m->IsPcNote())
            {
                    modplug::tracker::cmd_t newcommand = (m->note == NotePc) ? CmdMidi : CmdSmoothMidi;
                    if(!GetModSpecifications(nNewType).HasCommand(newcommand))
                    {
                            newcommand = CmdMidi;        // assuming that this was CMD_SMOOTHMIDI
                    }
                    if(!GetModSpecifications(nNewType).HasCommand(newcommand))
                    {
                            newcommand = CmdNone;
                    }

                    m->param = (uint8_t)(min(modplug::tracker::modevent_t::MaxColumnValue, m->GetValueEffectCol()) * 0x7F / modplug::tracker::modevent_t::MaxColumnValue);
                    m->command = newcommand; // might be removed later
                    m->volcmd = VolCmdNone;
                    m->note = NoteNone;
                    m->instr = 0;
            }

            // adjust extended envelope control commands
            if((m->command == CmdS3mCmdEx) && ((m->param & 0xF0) == 0x70) && ((m->param & 0x0F) > 0x0C))
            {
                    m->param = 0x7C;
            }

            if(m->command == CmdDelayCut)
            {
                    m->command = CmdS3mCmdEx; // when converting to MOD/XM, this will be converted to CMD_MODCMDEX later
                    m->param = 0xD0 | (m->param >> 4); // preserve delay nibble.
            }
    } // End if(oldTypeIsMPT)

    /////////////////////////////////////////
    // Convert MOD / XM to S3M / IT / MPTM
    if(oldTypeIsMOD_XM && newTypeIsS3M_IT_MPT)
    {
            switch(m->command)
            {
            case CmdModCmdEx:
                    MODExx2S3MSxx(m);
                    break;
            case CmdVol:
                    // Effect column volume command overrides the volume column in XM.
                    if (m->volcmd == VolCmdNone || m->volcmd == VolCmdVol)
                    {
                            m->volcmd = VolCmdVol;
                            m->vol = m->param;
                            if (m->vol > 0x40) m->vol = 0x40;
                            m->command = CmdNone;
                            m->param = 0;
                    }
                    break;
            case CmdPortaUp:
                    if (m->param > 0xDF) m->param = 0xDF;
                    break;
            case CmdPortaDown:
                    if (m->param > 0xDF) m->param = 0xDF;
                    break;
            case CmdExtraFinePortaUpDown:
                    switch(m->param & 0xF0)
                    {
                    case 0x10:        m->command = CmdPortaUp; m->param = (m->param & 0x0F) | 0xE0; break;
                    case 0x20:        m->command = CmdPortaDown; m->param = (m->param & 0x0F) | 0xE0; break;
                    case 0x50:
                    case 0x60:
                    case 0x70:
                    case 0x90:
                    case 0xA0:
                            m->command = CmdS3mCmdEx;
                            // surround remap (this is the "official" command)
                            if(nNewType & MOD_TYPE_S3M && m->param == 0x91)
                            {
                                    m->command = CmdPanning8;
                                    m->param = 0xA4;
                            }
                            break;
                    }
                    break;
            case CmdKeyOff:
                    if(m->note == 0)
                    {
                            m->note = (newTypeIsS3M) ? NoteNoteCut : NoteKeyOff;
                            m->command = CmdS3mCmdEx;
                            if(m->param == 0)
                                    m->instr = 0;
                            m->param = 0xD0 | (m->param & 0x0F);
                    }
                    break;
            case CmdPanningSlide:
                    // swap L/R
                    m->param = ((m->param & 0x0F) << 4) | (m->param >> 4);
            default:
                    break;
            }
    } // End if(oldTypeIsMOD_XM && newTypeIsS3M_IT_MPT)


    /////////////////////////////////////////
    // Convert S3M / IT / MPTM to MOD / XM
    else if(oldTypeIsS3M_IT_MPT && newTypeIsMOD_XM)
    {
            if(m->note == NoteNoteCut)
            {
                    // convert note cut to EC0
                    m->note = NoteNone;
                    m->command = CmdModCmdEx;
                    m->param = 0xC0;
            } else if(m->note == NoteFade)
            {
                    // convert note fade to note off
                    m->note = NoteKeyOff;
            }

            switch(m->command)
            {
            case CmdS3mCmdEx:
                    S3MSxx2MODExx(m);
                    break;
            case CmdVolumeSlide:
                    if ((m->param & 0xF0) && ((m->param & 0x0F) == 0x0F))
                    {
                            m->command = CmdModCmdEx;
                            m->param = (m->param >> 4) | 0xA0;
                    } else
                            if ((m->param & 0x0F) && ((m->param & 0xF0) == 0xF0))
                            {
                                    m->command = CmdModCmdEx;
                                    m->param = (m->param & 0x0F) | 0xB0;
                            }
                            break;
            case CmdPortaUp:
                    if (m->param >= 0xF0)
                    {
                            m->command = CmdModCmdEx;
                            m->param = (m->param & 0x0F) | 0x10;
                    } else
                            if (m->param >= 0xE0)
                            {
                                    if(newTypeIsXM)
                                    {
                                            m->command = CmdExtraFinePortaUpDown;
                                            m->param = 0x10 | (m->param & 0x0F);
                                    } else
                                    {
                                            m->command = CmdModCmdEx;
                                            m->param = (((m->param & 0x0F) + 3) >> 2) | 0x10;
                                    }
                            } else m->command = CmdPortaUp;
                    break;
            case CmdPortaDown:
                    if (m->param >= 0xF0)
                    {
                            m->command = CmdModCmdEx;
                            m->param = (m->param & 0x0F) | 0x20;
                    } else
                            if (m->param >= 0xE0)
                            {
                                    if(newTypeIsXM)
                                    {
                                            m->command = CmdExtraFinePortaUpDown;
                                            m->param = 0x20 | (m->param & 0x0F);
                                    } else
                                    {
                                            m->command = CmdModCmdEx;
                                            m->param = (((m->param & 0x0F) + 3) >> 2) | 0x20;
                                    }
                            } else m->command = CmdPortaDown;
                    break;
            case CmdSpeed:
                    {
                            m->param = min(m->param, (nNewType == MOD_TYPE_XM) ? 0x1F : 0x20);
                    }
                    break;
            case CmdTempo:
                    if(m->param < 0x20) m->command = CmdNone; // no tempo slides
                    break;
            case CmdPanningSlide:
                    // swap L/R
                    m->param = ((m->param & 0x0F) << 4) | (m->param >> 4);
                    // remove fine slides
                    if((m->param > 0xF0) || ((m->param & 0x0F) == 0x0F && m->param != 0x0F))
                            m->command = CmdNone;
            case CmdRetrig:
                    // Retrig: Q0y doesn't change volume in IT/S3M, but R0y in XM takes the last x parameter
                    if(m->param != 0 && (m->param & 0xF0) == 0)
                    {
                            m->param |= 0x80;
                    }
                    break;
            default:
                    break;
            }
    } // End if(oldTypeIsS3M_IT_MPT && newTypeIsMOD_XM)


    ///////////////////////
    // Convert IT to S3M
    else if(oldTypeIsIT_MPT && newTypeIsS3M)
    {
            if(m->note == NoteKeyOff || m->note == NoteFade)
                    m->note = NoteNoteCut;

            switch(m->command)
            {
            case CmdS3mCmdEx:
                    switch(m->param & 0xF0)
                    {
                    case 0x70: m->command = CmdNone; break;        // No NNA / envelope control in S3M format
                    case 0x90:
                            if(m->param == 0x91)
                            {
                                    // surround remap (this is the "official" command)
                                    m->command = CmdPanning8;
                                    m->param = 0xA4;
                            } else if(m->param == 0x90)
                            {
                                    m->command = CmdPanning8;
                                    m->param = 0x40;
                            }
                            break;
                    }
                    break;
            case CmdSmoothMidi:
                    m->command = CmdMidi;
                    break;
            default:
                    break;
            }
    } // End if (oldTypeIsIT_MPT && newTypeIsS3M)

    ///////////////////////////////////
    // MOD <-> XM: Speed/Tempo update
    if(oldTypeIsMOD && newTypeIsXM)
    {
            switch(m->command)
            {
            case CmdSpeed:
                    m->param = min(m->param, 0x1F);
                    break;
            }
    } else if(oldTypeIsXM && newTypeIsMOD)
    {
            switch(m->command)
            {
            case CmdTempo:
                    m->param = max(m->param, 0x21);
                    break;
            }
    }


    ///////////////////////////////////////////////////////////////////////
    // Convert MOD to anything - adjust effect memory, remove Invert Loop
    if (oldTypeIsMOD)
    {
            switch(m->command)
            {
            case CmdPortamentoVol: // lacks memory -> 500 is the same as 300
                    if(m->param == 0x00) m->command = CmdPorta;
                    break;
            case CmdVibratoVol: // lacks memory -> 600 is the same as 400
                    if(m->param == 0x00) m->command = CmdVibrato;
                    break;

            case CmdModCmdEx: // This would turn into "Set Active Macro", so let's better remove it
            case CmdS3mCmdEx:
                    if((m->param & 0xF0) == 0xF0) m->command = CmdNone;
                    break;
            }
    } // End if (oldTypeIsMOD && newTypeIsXM)

    /////////////////////////////////////////////////////////////////////
    // Convert anything to MOD - remove volume column, remove Set Macro
    if (newTypeIsMOD)
    {
            // convert note off events
            if(m->note >= NoteMinSpecial)
            {
                    m->note = NoteNone;
                    // no effect present, so just convert note off to volume 0
                    if(m->command == CmdNone)
                    {
                            m->command = CmdVol;
                            m->param = 0;
                            // EDx effect present, so convert it to ECx
                    } else if((m->command == CmdModCmdEx) && ((m->param & 0xF0) == 0xD0))
                    {
                            m->param = 0xC0 | (m->param & 0x0F);
                    }
            }

            if(m->command) switch(m->command)
            {
                    case CmdRetrig: // MOD only has E9x
                            m->command = CmdModCmdEx;
                            m->param = 0x90 | (m->param & 0x0F);
                            break;
                    case CmdModCmdEx: // This would turn into "Invert Loop", so let's better remove it
                            if((m->param & 0xF0) == 0xF0) m->command = CmdNone;
                            break;
            }

            else switch(m->volcmd)
            {
                    case VolCmdVol:
                            m->command = CmdVol;
                            m->param = m->vol;
                            break;
                    case VolCmdPan:
                            m->command = CmdPanning8;
                            m->param = CLAMP(m->vol << 2, 0, 0xFF);
                            break;
                    case VolCmdSlideDown:
                            m->command = CmdVolumeSlide;
                            m->param = m->vol;
                            break;
                    case VolCmdSlideUp:
                            m->command = CmdVolumeSlide;
                            m->param = m->vol << 4;
                            break;
                    case VolCmdFineDown:
                            m->command = CmdModCmdEx;
                            m->param = 0xB0 | m->vol;
                            break;
                    case VolCmdFineUp:
                            m->command = CmdModCmdEx;
                            m->param = 0xA0 | m->vol;
                            break;
                    case VolCmdPortamentoDown:
                            m->command = CmdPortaDown;
                            m->param = m->vol << 2;
                            break;
                    case VolCmdPortamentoUp:
                            m->command = CmdPortaUp;
                            m->param = m->vol << 2;
                            break;
                    case VolCmdPortamento:
                            m->command = CmdPorta;
                            m->param = m->vol << 2;
                            break;
                    case VolCmdVibratoDepth:
                            m->command = CmdVibrato;
                            m->param = m->vol;
                            break;
                    case VolCmdVibratoSpeed:
                            m->command = CmdVibrato;
                            m->param = m->vol << 4;
                            break;
                            // OpenMPT-specific commands
                    case VolCmdOffset:
                            m->command = CmdOffset;
                            m->param = m->vol << 3;
                            break;
                    default:
                            break;
            }
            m->volcmd = VolCmdNone;
    } // End if (newTypeIsMOD)

    ///////////////////////////////////////////////////
    // Convert anything to S3M - adjust volume column
    if (newTypeIsS3M)
    {
            if(!m->command) switch(m->volcmd)
            {
                    case VolCmdSlideDown:
                            m->command = CmdVolumeSlide;
                            m->param = m->vol;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdSlideUp:
                            m->command = CmdVolumeSlide;
                            m->param = m->vol << 4;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdFineDown:
                            m->command = CmdVolumeSlide;
                            m->param = 0xF0 | m->vol;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdFineUp:
                            m->command = CmdVolumeSlide;
                            m->param = (m->vol << 4) | 0x0F;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdPortamentoDown:
                            m->command = CmdPortaDown;
                            m->param = m->vol << 2;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdPortamentoUp:
                            m->command = CmdPortaUp;
                            m->param = m->vol << 2;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdPortamento:
                            m->command = CmdPorta;
                            m->param = m->vol << 2;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdVibratoDepth:
                            m->command = CmdVibrato;
                            m->param = m->vol;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdVibratoSpeed:
                            m->command = CmdVibrato;
                            m->param = m->vol << 4;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdPanSlideLeft:
                            m->command = CmdPanningSlide;
                            m->param = m->vol << 4;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdPanSlideRight:
                            m->command = CmdPanningSlide;
                            m->param = m->vol;
                            m->volcmd = VolCmdNone;
                            break;
                            // OpenMPT-specific commands
                    case VolCmdOffset:
                            m->command = CmdOffset;
                            m->param = m->vol << 3;
                            m->volcmd = VolCmdNone;
                            break;
                    default:
                            break;
            }
    } // End if (newTypeIsS3M)

    ////////////////////////////////////////////////////////////////////////
    // Convert anything to XM - adjust volume column, breaking EDx command
    if (newTypeIsXM)
    {
            // remove EDx if no note is next to it, or it will retrigger the note in FT2 mode
            if(m->command == CmdModCmdEx && (m->param & 0xF0) == 0xD0 && m->note == NoteNone)
            {
                    m->command = CmdNone;
                    m->param = 0;
            }

            if(!m->command) switch(m->volcmd)
            {
                    case VolCmdPortamentoDown:
                            m->command = CmdPortaDown;
                            m->param = m->vol << 2;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdPortamentoUp:
                            m->command = CmdPortaUp;
                            m->param = m->vol << 2;
                            m->volcmd = VolCmdNone;
                            break;
                            // OpenMPT-specific commands
                    case VolCmdOffset:
                            m->command = CmdOffset;
                            m->param = m->vol << 3;
                            m->volcmd = VolCmdNone;
                            break;
                    default:
                            break;
            }
    } // End if (newTypeIsXM)

    ///////////////////////////////////////////////////
    // Convert anything to IT - adjust volume column
    if (newTypeIsIT_MPT)
    {
            if(!m->command) switch(m->volcmd)
            {
                    case VolCmdSlideDown:
                    case VolCmdSlideUp:
                    case VolCmdFineDown:
                    case VolCmdFineUp:
                    case VolCmdPortamentoDown:
                    case VolCmdPortamentoUp:
                    case VolCmdPortamento:
                    case VolCmdVibratoDepth:
                            // OpenMPT-specific commands
                    case VolCmdOffset:
                            m->vol = min(m->vol, 9);
                            break;
                    case VolCmdPanSlideLeft:
                            m->command = CmdPanningSlide;
                            m->param = m->vol << 4;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdPanSlideRight:
                            m->command = CmdPanningSlide;
                            m->param = m->vol;
                            m->volcmd = VolCmdNone;
                            break;
                    case VolCmdVibratoSpeed:
                            m->command = CmdVibrato;
                            m->param = m->vol << 4;
                            m->volcmd = VolCmdNone;
                            break;
                    default:
                            break;
            }
    } // End if (newTypeIsIT)

    if(!module_renderer::GetModSpecifications(nNewType).HasNote(m->note))
            m->note = NoteNone;

    // ensure the commands really exist in this format
    if(module_renderer::GetModSpecifications(nNewType).HasCommand(m->command) == false)
            m->command = CmdNone;
    if(module_renderer::GetModSpecifications(nNewType).HasVolCommand(m->volcmd) == false)
            m->volcmd = VolCmdNone;

}


// "importance" of every FX command. Table is used for importing from formats with multiple effect colums
// and is approximately the same as in SchismTracker.
uint16_t module_renderer::GetEffectWeight(modplug::tracker::cmd_t cmd)
//---------------------------------------------------------
{
    switch(cmd)
    {
    case CmdPatternBreak    : return 288;
    case CmdPositionJump    : return 280;
    case CmdSpeed           : return 272;
    case CmdTempo           : return 264;
    case CmdGlobalVol    : return 256;
    case CmdGlobalVolSlide  : return 248;
    case CmdChannelVol   : return 240;
    case CmdChannelVolSlide : return 232;
    case CmdPortamentoVol    : return 224;
    case CmdPorta  : return 216;
    case CmdArpeggio        : return 208;
    case CmdRetrig          : return 200;
    case CmdTremor          : return 192;
    case CmdOffset          : return 184;
    case CmdVol          : return 176;
    case CmdVibratoVol      : return 168;
    case CmdVolumeSlide     : return 160;
    case CmdPortaDown  : return 152;
    case CmdPortaUp    : return 133;
    case CmdNoteSlideDown   : return 136;
    case CmdNoteSlideUp     : return 128;
    case CmdPanning8        : return 120;
    case CmdPanningSlide    : return 112;
    case CmdSmoothMidi      : return 104;
    case CmdMidi            : return  96;
    case CmdDelayCut        : return  88;
    case CmdModCmdEx        : return  80;
    case CmdS3mCmdEx        : return  72;
    case CmdPanbrello       : return  64;
    case CmdExtraFinePortaUpDown: return  56;
    case CmdVibrato         : return  48;
    case CmdFineVibrato     : return  40;
    case CmdTremolo         : return  32;
    case CmdKeyOff          : return  24;
    case CmdSetEnvelopePosition  : return  16;
    case CmdExtendedParameter          : return   8;
    case CmdNone            :
    default                  : return   0;
    }
}


/* Try to write an (volume column) effect in a given channel or any channel of a pattern in a specific row.
   Usage: nPat - Pattern that should be modified
          nRow - Row that should be modified
              nEffect - (Volume) Effect that should be written
              nParam - Effect that should be written
          bIsVolumeEffect  - Indicates whether the given effect is a volume column effect or not
              nChn - Channel that should be modified - use CHANNELINDEX_INVALID to allow all channels of the given row
              bAllowMultipleEffects - If false, No effect will be written if an effect of the same type is already present in the channel(s). Useful for f.e. tempo effects.
              allowRowChange - Indicates whether it is allowed to use the next or previous row if there's no space for the effect
              bRetry - For internal use only. Indicates whether an effect "rewrite" has already taken place (for recursive calls)
   NOTE: Effect remapping is only implemented for a few basic effects.
*/
bool module_renderer::TryWriteEffect(modplug::tracker::patternindex_t nPat, modplug::tracker::rowindex_t nRow, uint8_t nEffect, uint8_t nParam, bool bIsVolumeEffect, modplug::tracker::chnindex_t nChn, bool bAllowMultipleEffects, writeEffectAllowRowChange allowRowChange, bool bRetry)
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
{
    // First, reject invalid parameters.
    if(!Patterns.IsValidIndex(nPat) || nRow >= Patterns[nPat].GetNumRows() || (nChn >= GetNumChannels() && nChn != ChannelIndexInvalid))
    {
            return false;
    }

    modplug::tracker::chnindex_t nScanChnMin = nChn, nScanChnMax = nChn;
    modplug::tracker::modevent_t *p = Patterns[nPat], *m;

    // Scan all channels
    if(nChn == ChannelIndexInvalid)
    {
            nScanChnMin = 0;
            nScanChnMax = m_nChannels - 1;
    }

    // Scan channel(s) for same effect type - if an effect of the same type is already present, exit.
    if(!bAllowMultipleEffects)
    {
            for(modplug::tracker::chnindex_t i = nScanChnMin; i <= nScanChnMax; i++)
            {
                    m = p + nRow * m_nChannels + i;
                    if(!bIsVolumeEffect && m->command == nEffect)
                            return true;
                    if(bIsVolumeEffect && m->volcmd == nEffect)
                            return true;
            }
    }

    // Easy case: check if there's some space left to put the effect somewhere
    for(modplug::tracker::chnindex_t i = nScanChnMin; i <= nScanChnMax; i++)
    {
        m = p + nRow * m_nChannels + i;
        if(!bIsVolumeEffect && m->command == CmdNone) {
            //XXXih: gross
            m->command = (modplug::tracker::cmd_t) nEffect;
            m->param = nParam;
            return true;
        }
        if(bIsVolumeEffect && m->volcmd == VolCmdNone) {
            //XXXih: gross
            m->volcmd = (modplug::tracker::volcmd_t) nEffect;
            m->vol = nParam;
            return true;
        }
    }

    // Ok, apparently there's no space. If we haven't tried already, try to map it to the volume column or effect column instead.
    if(bRetry) {
            // Move some effects that also work in the volume column, so there's place for our new effect.
            if(!bIsVolumeEffect) {
                for(modplug::tracker::chnindex_t i = nScanChnMin; i <= nScanChnMax; i++) {
                    m = p + nRow * m_nChannels + i;
                    switch(m->command) {
                    case CmdVol:
                        m->volcmd = VolCmdVol;
                        m->vol = m->param;
                        //XXXih: gross
                        m->command = (modplug::tracker::cmd_t) nEffect;
                        m->param = nParam;
                        return true;

                    case CmdPanning8:
                        if(m_nType & MOD_TYPE_S3M && nParam > 0x80)
                                break;

                        m->volcmd = VolCmdPan;
                        //XXXih: gross
                        m->command = (modplug::tracker::cmd_t) nEffect;

                        if(m_nType & MOD_TYPE_S3M)
                        {
                                m->vol = m->param >> 1;
                        }
                        else
                        {
                                m->vol = (m->param >> 2) + 1;
                        }

                        m->param = nParam;
                        return true;
                    }
                }
            }

            // Let's try it again by writing into the "other" effect column.
            uint8_t nNewEffect = CmdNone;
            if(bIsVolumeEffect)
            {
                    switch(nEffect)
                    {
                    case VolCmdPan:
                            nNewEffect = CmdPanning8;
                            if(m_nType & MOD_TYPE_S3M)
                                    nParam <<= 1;
                            else
                                    nParam = min(nParam << 2, 0xFF);
                            break;
                    case VolCmdVol:
                            nNewEffect = CmdVol;
                            break;
                    }
            } else
            {
                    switch(nEffect)
                    {
                    case CmdPanning8:
                            nNewEffect = VolCmdPan;
                            if(m_nType & MOD_TYPE_S3M)
                            {
                                    if(nParam <= 0x80)
                                            nParam >>= 1;
                                    else
                                            nNewEffect = CmdNone;
                            }
                            else
                            {
                                    nParam = (nParam >> 2) + 1;
                            }
                            break;
                    case CmdVol:
                            nNewEffect = CmdVol;
                            break;
                    }
            }
            if(nNewEffect != CmdNone)
            {
                    if(TryWriteEffect(nPat, nRow, nNewEffect, nParam, !bIsVolumeEffect, nChn, bAllowMultipleEffects, allowRowChange, false) == true) return true;
            }
    }

    // Try in the next row if possible (this may also happen if we already retried)
    if(allowRowChange == weTryNextRow && (nRow + 1 < Patterns[nPat].GetNumRows()))
    {
            return TryWriteEffect(nPat, nRow + 1, nEffect, nParam, bIsVolumeEffect, nChn, bAllowMultipleEffects, allowRowChange, bRetry);
    } else if(allowRowChange == weTryPreviousRow && (nRow > 0))
    {
            return TryWriteEffect(nPat, nRow - 1, nEffect, nParam, bIsVolumeEffect, nChn, bAllowMultipleEffects, allowRowChange, bRetry);
    }

    return false;
}


// Try to convert a fx column command (*e) into a volume column command.
// Returns true if successful.
// Some commands can only be converted by losing some precision.
// If moving the command into the volume column is more important than accuracy, use bForce = true.
// (Code translated from SchismTracker and mainly supposed to be used with loaders ported from this tracker)
bool module_renderer::ConvertVolEffect(uint8_t *e, uint8_t *p, bool bForce)
//----------------------------------------------------------------
{
    switch (*e)
    {
    case CmdNone:
            return true;
    case CmdVol:
            *e = VolCmdVol;
            *p = min(*p, 64);
            break;
    case CmdPortaUp:
            // if not force, reject when dividing causes loss of data in LSB, or if the final value is too
            // large to fit. (volume column Ex/Fx are four times stronger than effect column)
            if (!bForce && ((*p & 3) || *p > 9 * 4 + 3))
                    return false;
            *p = min(*p / 4, 9);
            *e = VolCmdPortamentoUp;
            break;
    case CmdPortaDown:
            if (!bForce && ((*p & 3) || *p > 9 * 4 + 3))
                    return false;
            *p = min(*p / 4, 9);
            *e = VolCmdPortamentoDown;
            break;
    case CmdPorta:
            if (*p >= 0xF0)
            {
                    // hack for people who can't type F twice :)
                    *e = VolCmdPortamento;
                    *p = 9;
                    return true;
            }
            for (uint8_t n = 0; n < 10; n++)
            {
                    if (bForce
                            ? (*p <= ImpulseTrackerPortaVolCmd[n])
                            : (*p == ImpulseTrackerPortaVolCmd[n]))
                    {
                            *e = VolCmdPortamento;
                            *p = n;
                            return true;
                    }
            }
            return false;
    case CmdVibrato:
            if (bForce)
                    *p = min(*p, 9);
            else if (*p > 9)
                    return false;
            *e = VolCmdVibratoDepth;
            break;
    case CmdFineVibrato:
            if (bForce)
                    *p = 0;
            else if (*p)
                    return false;
            *e = VolCmdVibratoDepth;
            break;
    case CmdPanning8:
            *p = min(64, *p * 64 / 255);
            *e = VolCmdPan;
            break;
    case CmdVolumeSlide:
            if (*p == 0)
                    return false;
            if ((*p & 0xF) == 0)        // Dx0 / Cx
            {
                    if (bForce)
                            *p = min(*p >> 4, 9);
                    else if ((*p >> 4) > 9)
                            return false;
                    else
                            *p >>= 4;
                    *e = VolCmdSlideUp;
            } else if ((*p & 0xF0) == 0)        // D0x / Dx
            {
                    if (bForce)
                            *p = min(*p, 9);
                    else if (*p > 9)
                            return false;
                    *e = VolCmdSlideDown;
            } else if ((*p & 0xF) == 0xF)        // DxF / Ax
            {
                    if (bForce)
                            *p = min(*p >> 4, 9);
                    else if ((*p >> 4) > 9)
                            return false;
                    else
                            *p >>= 4;
                    *e = VolCmdFineUp;
            } else if ((*p & 0xf0) == 0xf0)        // DFx / Bx
            {
                    if (bForce)
                            *p = min(*p, 9);
                    else if ((*p & 0xF) > 9)
                            return false;
                    else
                            *p &= 0xF;
                    *e = VolCmdFineDown;
            } else // ???
            {
                    return false;
            }
            break;
    case CmdS3mCmdEx:
            switch (*p >> 4)
            {
            case 8:
                    *e = VolCmdPan;
                    *p = ((*p & 0xf) << 2) + 2;
                    return true;
            case 0: case 1: case 2: case 0xF:
                    if (bForce)
                    {
                            *e = *p = 0;
                            return true;
                    }
                    break;
            default:
                    break;
            }
            return false;
    default:
            return false;
    }
    return true;
}