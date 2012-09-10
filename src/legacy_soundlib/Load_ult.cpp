/*
 * Purpose: Load ULT (UltraTracker) modules
 * Authors: Storlek (Original author - http://schismtracker.org/)
 *                    Johannes Schultz (OpenMPT Port, tweaks)
 *
 * Thanks to Storlek for allowing me to use this code!
 */

#include "stdafx.h"
#include "Loaders.h"

enum
{
    ULT_16BIT = 4,
    ULT_LOOP  = 8,
    ULT_PINGPONGLOOP = 16,
};

#pragma pack(1)
struct ULT_SAMPLE
{
    char   name[32];
    char   filename[12];
    uint32_t loop_start;
    uint32_t loop_end;
    uint32_t size_start;
    uint32_t size_end;
    uint8_t  volume;                // 0-255, apparently prior to 1.4 this was logarithmic?
    uint8_t  flags;                // above
    uint16_t speed;                // only exists for 1.4+
    int16_t  finetune;
};
static_assert(sizeof(ULT_SAMPLE) >= 64, "sizeof(ULT_SAMPLE) >= 64 failed");
#pragma pack()

/* Unhandled effects:
5x1 - do not loop sample (x is unused)
5xC - end loop and finish sample
9xx - set sample offset to xx * 1024
    with 9yy: set sample offset to xxyy * 4
E0x - set vibrato strength (2 is normal)

The logarithmic volume scale used in older format versions here, or pretty
much anywhere for that matter. I don't even think Ultra Tracker tries to
convert them. */

static const uint8_t ult_efftrans[] =
{
    CmdArpeggio,
    CmdPortaUp,
    CmdPortaDown,
    CmdPorta,
    CmdVibrato,
    CmdNone,
    CmdNone,
    CmdTremolo,
    CmdNone,
    CmdOffset,
    CmdVolSlide,
    CmdPanning8,
    CmdVol,
    CmdPatternBreak,
    CmdNone, // extended effects, processed separately
    CmdSpeed,
};

static void TranslateULTCommands(uint8_t *pe, uint8_t *pp)
//----------------------------------------------------
{
    uint8_t e = *pe & 0x0F;
    uint8_t p = *pp;

    *pe = ult_efftrans[e];

    switch (e)
    {
    case 0x00:
            if (!p)
                    *pe = CmdNone;
            break;
    case 0x05:
            // play backwards
            if((p & 0x0F) == 0x02)
            {
                    *pe = CmdS3mCmdEx;
                    p = 0x9F;
            }
            break;
    case 0x0A:
            // blah, this sucks
            if (p & 0xF0)
                    p &= 0xF0;
            break;
    case 0x0B:
            // mikmod does this wrong, resulting in values 0-225 instead of 0-255
            p = (p & 0x0F) * 0x11;
            break;
    case 0x0C: // volume
            p >>= 2;
            break;
    case 0x0D: // pattern break
            p = 10 * (p >> 4) + (p & 0x0F);
    case 0x0E: // special
            switch (p >> 4)
            {
            case 0x01:
                    *pe = CmdPortaUp;
                    p = 0xF0 | (p & 0x0F);
                    break;
            case 0x02:
                    *pe = CmdPortaDown;
                    p = 0xF0 | (p & 0x0F);
                    break;
            case 0x08:
                    *pe = CmdS3mCmdEx;
                    p = 0x60 | (p & 0x0F);
                    break;
            case 0x09:
                    *pe = CmdRetrig;
                    p &= 0x0F;
                    break;
            case 0x0A:
                    *pe = CmdVolSlide;
                    p = ((p & 0x0F) << 4) | 0x0F;
                    break;
            case 0x0B:
                    *pe = CmdVolSlide;
                    p = 0xF0 | (p & 0x0F);
                    break;
            case 0x0C: case 0x0D:
                    *pe = CmdS3mCmdEx;
                    break;
            }
            break;
    case 0x0F:
            if (p > 0x2F)
                    *pe = CmdTempo;
            break;
    }

    *pp = p;
}

static int ReadULTEvent(modplug::tracker::modevent_t *note, const uint8_t *lpStream, uint32_t *dwMP, const uint32_t dwMemLength)
//---------------------------------------------------------------------------------------------------
{
    #define ASSERT_CAN_READ_ULTENV(x) ASSERT_CAN_READ_PROTOTYPE(dwMemPos, dwMemLength, x, return 0);

    uint32_t dwMemPos = *dwMP;
    uint8_t b, repeat = 1;
    uint8_t cmd1, cmd2;        // 1 = vol col, 2 = fx col in the original schismtracker code
    uint8_t param1, param2;

    ASSERT_CAN_READ_ULTENV(1)
    b = lpStream[dwMemPos++];
    if (b == 0xFC)        // repeat event
    {
            ASSERT_CAN_READ_ULTENV(2);
            repeat = lpStream[dwMemPos++];
            b = lpStream[dwMemPos++];
    }
    ASSERT_CAN_READ_ULTENV(4)
    note->note = (b > 0 && b < 61) ? b + 36 : NoteNone;
    note->instr = lpStream[dwMemPos++];
    b = lpStream[dwMemPos++];
    cmd1 = b & 0x0F;
    cmd2 = b >> 4;
    param1 = lpStream[dwMemPos++];
    param2 = lpStream[dwMemPos++];
    TranslateULTCommands(&cmd1, &param1);
    TranslateULTCommands(&cmd2, &param2);

    // sample offset -- this is even more special than digitrakker's
    if(cmd1 == CmdOffset && cmd2 == CmdOffset)
    {
            uint32_t off = ((param1 << 8) | param2) >> 6;
            cmd1 = CmdNone;
            param1 = (uint8_t)min(off, 0xFF);
    } else if(cmd1 == CmdOffset)
    {
            uint32_t off = param1 * 4;
            param1 = (uint8_t)min(off, 0xFF);
    } else if(cmd2 == CmdOffset)
    {
            uint32_t off = param2 * 4;
            param2 = (uint8_t)min(off, 0xFF);
    } else if(cmd1 == cmd2)
    {
            // don't try to figure out how ultratracker does this, it's quite random
            cmd2 = CmdNone;
    }
    if (cmd2 == CmdVol || (cmd2 == CmdNone && cmd1 != CmdVol))
    {
            // swap commands
            std::swap(cmd1, cmd2);
            std::swap(param1, param2);
    }

    // Do that dance.
    // Maybe I should quit rewriting this everywhere and make a generic version :P
    int n;
    for (n = 0; n < 4; n++)
    {
            if(module_renderer::ConvertVolEffect(&cmd1, &param1, (n >> 1) ? true : false))
            {
                    n = 5;
                    break;
            }
            std::swap(cmd1, cmd2);
            std::swap(param1, param2);
    }
    if (n < 5)
    {
            if (module_renderer::GetEffectWeight((modplug::tracker::cmd_t)cmd1) > module_renderer::GetEffectWeight((modplug::tracker::cmd_t)cmd2))
            {
                    std::swap(cmd1, cmd2);
                    std::swap(param1, param2);
            }
            cmd1 = CmdNone;
    }
    if (!cmd1)
            param1 = 0;
    if (!cmd2)
            param2 = 0;

    //XXXih: gross
    note->volcmd = (modplug::tracker::volcmd_t) cmd1;
    note->vol = param1;
    note->command = (modplug::tracker::cmd_t) cmd2;
    note->param = param2;

    *dwMP = dwMemPos;
    return repeat;

    #undef ASSERT_CAN_READ_ULTENV
}

// Functor for postfixing ULT patterns (this is easier than just remembering everything WHILE we're reading the pattern events)
struct PostFixUltCommands
//=======================
{
    PostFixUltCommands(modplug::tracker::chnindex_t numChannels)
    {
            this->numChannels = numChannels;
            curChannel = 0;
            writeT125 = false;
            isPortaActive.resize(numChannels, false);
    }

    void operator()(modplug::tracker::modevent_t& m)
    {
            // Attempt to fix portamentos.
            // UltraTracker will slide until the destination note is reached or 300 is encountered.

            // Stop porta?
            if(m.command == CmdPorta && m.param == 0)
            {
                    isPortaActive[curChannel] = false;
                    m.command = CmdNone;
            }
            if(m.volcmd == VolCmdPortamento && m.vol == 0)
            {
                    isPortaActive[curChannel] = false;
                    m.volcmd = VolCmdNone;
            }

            // Apply porta?
            if(m.note == NoteNone && isPortaActive[curChannel])
            {
                    if(m.command == CmdNone && m.vol != VolCmdPortamento)
                    {
                            m.command = CmdPorta;
                            m.param = 0;
                    } else if(m.volcmd == VolCmdNone && m.command != CmdPorta)
                    {
                            m.volcmd = VolCmdPortamento;
                            m.vol = 0;
                    }
            } else        // new note -> stop porta (or initialize again)
            {
                    isPortaActive[curChannel] = (m.command == CmdPorta || m.volcmd == VolCmdPortamento);
            }

            // attempt to fix F00 (reset to tempo 125, speed 6)
            if(writeT125 && m.command == CmdNone)
            {
                    m.command = CmdTempo;
                    m.param = 125;
            }
            if(m.command == CmdSpeed && m.param == 0)
            {
                    m.param = 6;
                    writeT125 = true;
            }
            if(m.command == CmdTempo)        // don't try to fix this anymore if the tempo has already changed.
            {
                    writeT125 = false;
            }
            curChannel = (curChannel + 1) % numChannels;
    }

    vector<bool> isPortaActive;
    bool writeT125;
    modplug::tracker::chnindex_t numChannels, curChannel;
};


bool module_renderer::ReadUlt(const uint8_t *lpStream, const uint32_t dwMemLength)
//---------------------------------------------------------------------
{
    uint32_t dwMemPos = 0;
    uint8_t ult_version;

    // Tracker ID
    ASSERT_CAN_READ(15);
    if (memcmp(lpStream, "MAS_UTrack_V00", 14) != 0)
            return false;
    dwMemPos += 14;
    ult_version = lpStream[dwMemPos++];
    if (ult_version < '1' || ult_version > '4')
            return false;
    ult_version -= '0';

    ASSERT_CAN_READ(32);
    assign_without_padding(this->song_name, reinterpret_cast<const char *>(lpStream + dwMemPos), 32);
    dwMemPos += 32;

    m_nType = MOD_TYPE_ULT;
    m_dwSongFlags = SONG_ITCOMPATGXX | SONG_ITOLDEFFECTS;        // this will be converted to IT format by MPT.
    SetModFlag(MSF_COMPATIBLE_PLAY, true);

    ASSERT_CAN_READ(1);
    uint8_t nNumLines = (uint8_t)lpStream[dwMemPos++];
    ASSERT_CAN_READ((uint32_t)(nNumLines * 32));
    // read "nNumLines" lines, each containing 32 characters.
    ReadFixedLineLengthMessage(lpStream + dwMemPos, nNumLines * 32, 32, 0);
    dwMemPos += nNumLines * 32;

    ASSERT_CAN_READ(1);
    m_nSamples = (modplug::tracker::sampleindex_t)lpStream[dwMemPos++];
    if(m_nSamples >= MAX_SAMPLES)
            return false;

    for(modplug::tracker::sampleindex_t nSmp = 0; nSmp < m_nSamples; nSmp++)
    {
            ULT_SAMPLE ultSmp;
            modsample_t *pSmp = &(Samples[nSmp + 1]);
            // annoying: v4 added a field before the end of the struct
            if(ult_version >= 4)
            {
                    ASSERT_CAN_READ(sizeof(ULT_SAMPLE));
                    memcpy(&ultSmp, lpStream + dwMemPos, sizeof(ULT_SAMPLE));
                    dwMemPos += sizeof(ULT_SAMPLE);

                    ultSmp.speed = LittleEndianW(ultSmp.speed);
            } else
            {
                    ASSERT_CAN_READ(sizeof(64));
                    memcpy(&ultSmp, lpStream + dwMemPos, 64);
                    dwMemPos += 64;

                    ultSmp.finetune = ultSmp.speed;
                    ultSmp.speed = 8363;
            }
            ultSmp.finetune = LittleEndianW(ultSmp.finetune);
            ultSmp.loop_start = LittleEndian(ultSmp.loop_start);
            ultSmp.loop_end = LittleEndian(ultSmp.loop_end);
            ultSmp.size_start = LittleEndian(ultSmp.size_start);
            ultSmp.size_end = LittleEndian(ultSmp.size_end);

            memcpy(m_szNames[nSmp + 1], ultSmp.name, 32);
            SetNullTerminator(m_szNames[nSmp + 1]);
            memcpy(pSmp->legacy_filename, ultSmp.filename, 12);
            SpaceToNullStringFixed<12>(pSmp->legacy_filename);

            if(ultSmp.size_end <= ultSmp.size_start)
                    continue;
            pSmp->length = ultSmp.size_end - ultSmp.size_start;
            pSmp->loop_start = ultSmp.loop_start;
            pSmp->loop_end = min(ultSmp.loop_end, pSmp->length);
            pSmp->default_volume = ultSmp.volume;
            pSmp->global_volume = 64;

            /* mikmod does some weird integer math here, but it didn't really work for me */
            pSmp->c5_samplerate = ultSmp.speed;
            if(ultSmp.finetune)
            {
                    pSmp->c5_samplerate = (UINT)(((double)pSmp->c5_samplerate) * pow(2.0, (((double)ultSmp.finetune) / (12.0 * 32768))));
            }

            if(ultSmp.flags & ULT_LOOP)
                    pSmp->flags |= CHN_LOOP;
            if(ultSmp.flags & ULT_PINGPONGLOOP)
                    pSmp->flags |= CHN_PINGPONGLOOP;
            if(ultSmp.flags & ULT_16BIT)
            {
                    pSmp->flags |= CHN_16BIT;
                    pSmp->loop_start >>= 1;
                    pSmp->loop_end >>= 1;
            }
    }

    // ult just so happens to use 255 for its end mark, so there's no need to fiddle with this
    Order.ReadAsByte(lpStream + dwMemPos, 256, dwMemLength - dwMemPos);
    dwMemPos += 256;

    ASSERT_CAN_READ(2);
    m_nChannels = lpStream[dwMemPos++] + 1;
    modplug::tracker::patternindex_t nNumPats = lpStream[dwMemPos++] + 1;

    if(m_nChannels > MAX_BASECHANNELS || nNumPats > MAX_PATTERNS)
            return false;

    if(ult_version >= 3)
    {
            ASSERT_CAN_READ(m_nChannels);
            for(modplug::tracker::chnindex_t nChn = 0; nChn < m_nChannels; nChn++)
            {
                    ChnSettings[nChn].nPan = ((lpStream[dwMemPos + nChn] & 0x0F) << 4) + 8;
            }
            dwMemPos += m_nChannels;
    } else
    {
            for(modplug::tracker::chnindex_t nChn = 0; nChn < m_nChannels; nChn++)
            {
                    ChnSettings[nChn].nPan = (nChn & 1) ? 192 : 64;
            }
    }

    for(modplug::tracker::patternindex_t nPat = 0; nPat < nNumPats; nPat++)
    {
            if(Patterns.Insert(nPat, 64))
                    return false;
    }

    for(modplug::tracker::chnindex_t nChn = 0; nChn < m_nChannels; nChn++)
    {
            modplug::tracker::modevent_t evnote;
            modplug::tracker::modevent_t *note;
            int repeat;
            evnote.Clear();

            for(modplug::tracker::patternindex_t nPat = 0; nPat < nNumPats; nPat++)
            {
                    note = Patterns[nPat] + nChn;
                    modplug::tracker::rowindex_t nRow = 0;
                    while(nRow < 64)
                    {
                            repeat = ReadULTEvent(&evnote, lpStream, &dwMemPos, dwMemLength);
                            if(repeat + nRow > 64)
                                    repeat = 64 - nRow;
                            if(repeat == 0) break;
                            while (repeat--)
                            {
                                    *note = evnote;
                                    note += m_nChannels;
                                    nRow++;
                            }
                    }
            }
    }

    // Post-fix some effects.
    Patterns.ForEachModCommand(PostFixUltCommands(m_nChannels));

    for(modplug::tracker::sampleindex_t nSmp = 0; nSmp < m_nSamples; nSmp++)
    {
            dwMemPos += ReadSample(&Samples[nSmp + 1], (Samples[nSmp + 1].flags & CHN_16BIT) ? RS_PCM16S : RS_PCM8S, (LPCSTR)(lpStream + dwMemPos), dwMemLength - dwMemPos);
    }
    return true;
}