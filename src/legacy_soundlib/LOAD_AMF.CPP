/*
 * This source code is public domain.
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *			OpenMPT dev(s)	(miscellaneous modifications)
*/

///////////////////////////////////////////////////
//
// AMF module loader
//
// There is 2 types of AMF files:
// - ASYLUM Music Format
// - Advanced Music Format(DSM)
//
///////////////////////////////////////////////////
#include "stdafx.h"
#include "Loaders.h"

//#define AMFLOG

#pragma warning(disable:4244) //"conversion from 'type1' to 'type2', possible loss of data"

#pragma pack(1)

typedef struct _AMFFILEHEADER
{
    UCHAR szAMF[3];
    UCHAR version;
    CHAR title[32];
    UCHAR numsamples;
    UCHAR numorders;
    USHORT numtracks;
    UCHAR numchannels;
} AMFFILEHEADER;

typedef struct _AMFSAMPLE
{
    UCHAR type;
    CHAR  samplename[32];
    CHAR  filename[13];
    ULONG offset;
    ULONG length;
    USHORT c2spd;
    UCHAR volume;
} AMFSAMPLE;

#pragma pack()


#ifdef AMFLOG
extern void Log(LPCSTR, ...);
#endif

void AMF_Unpack(modplug::tracker::modevent_t *pPat, const uint8_t *pTrack, UINT nRows, UINT nChannels)
//-------------------------------------------------------------------------------
{
    UINT lastinstr = 0;
    UINT nTrkSize = LittleEndianW(*(USHORT *)pTrack);
    nTrkSize += (UINT)pTrack[2] << 16;
    pTrack += 3;
    while (nTrkSize--)
    {
        UINT row = pTrack[0];
        UINT cmd = pTrack[1];
        UINT arg = pTrack[2];
        if (row >= nRows) break;
        modplug::tracker::modevent_t *m = pPat + row * nChannels;
        if (cmd < 0x7F) // note+vol
        {
            m->note = cmd+1;
            // Does the next line make sense? I can't find any documents on this format, nor do any other players seem to handle this problem (or I'm overlooking something). However, the tunes in Pinball World seem to play a lot better with this, and it kind of looks right.
            if(arg != 0xFF)
            {
                if (!m->instr) m->instr = lastinstr;
                m->volcmd = VolCmdVol;
                m->vol = arg;
            }
        } else
        if (cmd == 0x7F) // duplicate row
        {
            signed char rdelta = (signed char)arg;
            int rowsrc = (int)row + (int)rdelta;
            if ((rowsrc >= 0) && (rowsrc < (int)nRows)) *m = pPat[rowsrc*nChannels];
        } else
        if (cmd == 0x80) // instrument
        {
            m->instr = arg+1;
            lastinstr = m->instr;
        } else
        if (cmd == 0x83) // volume
        {
            m->volcmd = VolCmdVol;
            m->vol = arg;
        } else
        // effect
        {
            UINT command = cmd & 0x7F;
            UINT param = arg;
            switch(command)
            {
            // 0x01: Set Speed
            case 0x01:	command = CMD_SPEED; break;
            // 0x02: Volume Slide
            // 0x0A: Tone Porta + Vol Slide
            // 0x0B: Vibrato + Vol Slide
            case 0x02:	command = CMD_VOLUMESLIDE;
            case 0x0A:	if (command == 0x0A) command = CMD_TONEPORTAVOL;
            case 0x0B:	if (command == 0x0B) command = CMD_VIBRATOVOL;
                        if (param & 0x80) param = (-(signed char)param)&0x0F;
                        else param = (param&0x0F)<<4;
                        break;
            // 0x04: Porta Up/Down
            case 0x04:      if (param & 0x80) { command = CMD_PORTAMENTOUP; param = (-(signed char)param)&0x7F; }
                            else { command = CMD_PORTAMENTODOWN; }
                            break;
            // 0x06: Tone Portamento
            case 0x06:	command = CMD_TONEPORTAMENTO; break;
            // 0x07: Tremor
            case 0x07:	command = CMD_TREMOR; break;
            // 0x08: Arpeggio
            case 0x08:	command = CMD_ARPEGGIO; break;
            // 0x09: Vibrato
            case 0x09:	command = CMD_VIBRATO; break;
            // 0x0C: Pattern Break
            case 0x0C:	command = CMD_PATTERNBREAK; break;
            // 0x0D: Position Jump
            case 0x0D:	command = CMD_POSITIONJUMP; break;
            // 0x0F: Retrig
            case 0x0F:	command = CMD_RETRIG; break;
            // 0x10: Offset
            case 0x10:	command = CMD_OFFSET; break;
            // 0x11: Fine Volume Slide
            case 0x11:	if (param) { command = CMD_VOLUMESLIDE;
                            if (param & 0x80) param = 0xF0|((-(signed char)param)&0x0F);
                            else param = 0x0F|((param&0x0F)<<4);
                        } else command = 0; break;
            // 0x12: Fine Portamento
            // 0x16: Extra Fine Portamento
            case 0x12:
            case 0x16:	if (param) { int mask = (command == 0x16) ? 0xE0 : 0xF0;
                            command = (param & 0x80) ? CMD_PORTAMENTOUP : CMD_PORTAMENTODOWN;
                            if (param & 0x80) param = mask|((-(signed char)param)&0x0F);
                            else param |= mask;
                        } else command = 0; break;
            // 0x13: Note Delay
            case 0x13:	command = CMD_S3MCMDEX; param = 0xD0|(param & 0x0F); break;
            // 0x14: Note Cut
            case 0x14:	command = CMD_S3MCMDEX; param = 0xC0|(param & 0x0F); break;
            // 0x15: Set Tempo
            case 0x15:	command = CMD_TEMPO; break;
            // 0x17: Panning
            case 0x17:	param = (param + 64) & 0x7F;
                        if (m->command) { if (!m->volcmd) { m->volcmd = VolCmdPan;  m->vol = param >> 1; } command = 0; }
                        else { command = CMD_PANNING8; }
                        break;
            // Unknown effects
            default:	command = param = 0;
            }
            if (command)
            {
                m->command = command;
                m->param = param;
            }
        }
        pTrack += 3;
    }
}



bool module_renderer::ReadAMF(const uint8_t * const lpStream, const uint32_t dwMemLength)
//-----------------------------------------------------------------------
{
    const AMFFILEHEADER *pfh = (AMFFILEHEADER *)lpStream;
    uint32_t dwMemPos;

    if ((!lpStream) || (dwMemLength < 2048)) return false;
    if ((!strncmp((LPCSTR)lpStream, "ASYLUM Music Format V1.0", 25)) && (dwMemLength > 4096))
    {
        UINT numorders, numpats, numsamples;

        dwMemPos = 32;
        numpats = lpStream[dwMemPos+3];
        numorders = lpStream[dwMemPos+4];
        numsamples = 64;
        dwMemPos += 6;
        if ((!numpats) || (numpats > MAX_PATTERNS) || (!numorders)
         || (numpats*64*32 + 294 + 37*64 >= dwMemLength)) return false;
        m_nType = MOD_TYPE_AMF0;
        m_nChannels = 8;
        m_nInstruments = 0;
        m_nSamples = 31;
        m_nDefaultTempo = 125;
        m_nDefaultSpeed = 6;
        Order.ReadAsByte(lpStream + dwMemPos, numorders, dwMemLength - dwMemPos);
        dwMemPos = 294; // ???
        for (UINT iSmp=0; iSmp<numsamples; iSmp++)
        {
            modsample_t *psmp = &Samples[iSmp+1];
            memcpy(m_szNames[iSmp + 1], lpStream+dwMemPos, 22);
            SpaceToNullStringFixed<22>(m_szNames[iSmp + 1]);
            psmp->nFineTune = MOD2XMFineTune(lpStream[dwMemPos+22]);
            psmp->default_volume = lpStream[dwMemPos+23];
            psmp->global_volume = 64;
            if (psmp->default_volume > 0x40) psmp->default_volume = 0x40;
            psmp->default_volume <<= 2;
            psmp->length = LittleEndian(*((LPDWORD)(lpStream+dwMemPos+25)));
            psmp->loop_start = LittleEndian(*((LPDWORD)(lpStream+dwMemPos+29)));
            psmp->loop_end = psmp->loop_start + LittleEndian(*((LPDWORD)(lpStream+dwMemPos+33)));
            if (psmp->loop_end <= 2) psmp->loop_end = 0;
            if ((psmp->loop_end > psmp->loop_start) && (psmp->loop_end <= psmp->length))
            {
                psmp->flags = CHN_LOOP;
            } else
            {
                psmp->loop_start = psmp->loop_end = 0;
            }
            if ((psmp->length) && (iSmp>31)) m_nSamples = iSmp+1;
            dwMemPos += 37;
        }
        for (UINT iPat=0; iPat<numpats; iPat++)
        {
            Patterns.Insert(iPat, 64);
            modplug::tracker::modevent_t* p = Patterns[iPat];
            if (!p) break;
            const UCHAR *pin = lpStream + dwMemPos;
            for (UINT i=0; i<8*64; i++)
            {
                p->note = NoteNone;
                if (pin[0])
                {
                    p->note = pin[0] + 13;
                }
                p->instr = pin[1];
                p->command = pin[2];
                p->param = pin[3];
                if (p->command > 0x0F)
                {
                #ifdef AMFLOG
                    Log("0x%02X.0x%02X ?", p->command, p->param);
                #endif
                    p->command = 0;
                }
                ConvertModCommand(p);
                pin += 4;
                p++;
            }
            dwMemPos += 64*32;
        }
        // Read samples
        for (UINT iData=0; iData<m_nSamples; iData++)
        {
            modsample_t *psmp = &Samples[iData+1];
            if (psmp->length)
            {
                if(dwMemPos > dwMemLength) return false;
                dwMemPos += ReadSample(psmp, RS_PCM8S, (LPCSTR)(lpStream+dwMemPos), dwMemLength - dwMemPos);
            }
        }
        return true;
    }
    ////////////////////////////
    // DSM/AMF
    USHORT *ptracks[MAX_PATTERNS];
    uint32_t sampleseekpos[MAX_SAMPLES];

    if ((pfh->szAMF[0] != 'A') || (pfh->szAMF[1] != 'M') || (pfh->szAMF[2] != 'F')
     || (pfh->version < 10) || (pfh->version > 14) || (!LittleEndianW(pfh->numtracks))
     || (!pfh->numorders) || (pfh->numorders > MAX_PATTERNS)
     || (!pfh->numsamples) || (pfh->numsamples > MAX_SAMPLES)
     || (pfh->numchannels < 1) || (pfh->numchannels > 32))
        return false;

    assign_without_padding(this->song_name, pfh->title, 31);

    dwMemPos = sizeof(AMFFILEHEADER);
    m_nType = MOD_TYPE_AMF;
    m_nChannels = pfh->numchannels;
    m_nSamples = pfh->numsamples;
    m_nInstruments = 0;
    // Setup Channel Pan Positions
    if (pfh->version >= 11)
    {
        signed char *panpos = (signed char *)(lpStream + dwMemPos);
        UINT nchannels = (pfh->version >= 13) ? 32 : 16;
        for (UINT i=0; i<nchannels; i++)
        {
            int pan = (panpos[i] + 64) * 2;
            if (pan < 0) pan = 0;
            if (pan > 256) { pan = 128; ChnSettings[i].dwFlags |= CHN_SURROUND; }
            ChnSettings[i].nPan = pan;
        }
        dwMemPos += nchannels;
    } else
    {
        for (UINT i=0; i<16; i++)
        {
            ChnSettings[i].nPan = (lpStream[dwMemPos + i] & 1) ? 0x40 : 0xC0;
        }
        dwMemPos += 16;
    }
    // Get Tempo/Speed
    m_nDefaultTempo = 125;
    m_nDefaultSpeed = 6;
    if (pfh->version >= 13)
    {
        if (lpStream[dwMemPos] >= 32) m_nDefaultTempo = lpStream[dwMemPos];
        if (lpStream[dwMemPos+1] <= 32) m_nDefaultSpeed = lpStream[dwMemPos+1];
        dwMemPos += 2;
    }
    // Setup sequence list
    Order.resize(pfh->numorders, Order.GetInvalidPatIndex());
    vector<modplug::tracker::rowindex_t> patternLength(pfh->numorders, 64);
    for (UINT iOrd=0; iOrd < pfh->numorders; iOrd++)
    {
        Order[iOrd] = iOrd;
        if (pfh->version >= 14)
        {
            patternLength[iOrd] = LittleEndianW(*(uint16_t *)(lpStream+dwMemPos));
            dwMemPos += 2;
        }
        ptracks[iOrd] = (USHORT *)(lpStream+dwMemPos);
        dwMemPos += m_nChannels * sizeof(USHORT);
    }
    if (dwMemPos + m_nSamples * (sizeof(AMFSAMPLE)+8) > dwMemLength) return true;
    // Read Samples
    UINT maxsampleseekpos = 0;
    for (UINT iIns=0; iIns<m_nSamples; iIns++)
    {
        modsample_t *pSmp = &Samples[iIns+1];
        AMFSAMPLE *psh = (AMFSAMPLE *)(lpStream + dwMemPos);

        dwMemPos += sizeof(AMFSAMPLE);
        memcpy(m_szNames[iIns+1], psh->samplename, 31);
        memcpy(pSmp->legacy_filename, psh->filename, 13);
        SpaceToNullStringFixed<31>(m_szNames[iIns + 1]);
        SpaceToNullStringFixed<13>(pSmp->legacy_filename);
        pSmp->length = LittleEndian(psh->length);
        pSmp->c5_samplerate = LittleEndianW(psh->c2spd);
        pSmp->global_volume = 64;
        pSmp->default_volume = psh->volume * 4;
        if (pfh->version >= 11)
        {
            pSmp->loop_start = LittleEndian(*(uint32_t *)(lpStream+dwMemPos));
            pSmp->loop_end = LittleEndian(*(uint32_t *)(lpStream+dwMemPos+4));
            dwMemPos += 8;
        } else
        {
            pSmp->loop_start = LittleEndianW(*(uint16_t *)(lpStream+dwMemPos));
            pSmp->loop_end = pSmp->length;
            dwMemPos += 2;
        }
        sampleseekpos[iIns] = 0;
        if ((psh->type) && (LittleEndian(psh->offset) < dwMemLength-1))
        {
            sampleseekpos[iIns] = LittleEndian(psh->offset);
            if (LittleEndian(psh->offset) > maxsampleseekpos)
                maxsampleseekpos = LittleEndian(psh->offset);
            if ((pSmp->loop_end > pSmp->loop_start + 2)
             && (pSmp->loop_end <= pSmp->length)) pSmp->flags |= CHN_LOOP;
        }
    }
    // Read Track Mapping Table
    USHORT *pTrackMap = (USHORT *)(lpStream+dwMemPos);
    UINT realtrackcnt = 0;
    dwMemPos += pfh->numtracks * sizeof(USHORT);
    for (UINT iTrkMap=0; iTrkMap<pfh->numtracks; iTrkMap++)
    {
        if (realtrackcnt < pTrackMap[iTrkMap]) realtrackcnt = pTrackMap[iTrkMap];
    }
    // Store tracks positions
    uint8_t **pTrackData = new uint8_t *[realtrackcnt];
    memset(pTrackData, 0, sizeof(pTrackData));
    for (UINT iTrack=0; iTrack<realtrackcnt; iTrack++) if (dwMemPos <= dwMemLength-3)
    {
        UINT nTrkSize = LittleEndianW(*(USHORT *)(lpStream+dwMemPos));
        nTrkSize += (UINT)lpStream[dwMemPos+2] << 16;
        if (dwMemPos + nTrkSize * 3 + 3 <= dwMemLength)
        {
            pTrackData[iTrack] = (uint8_t *)(lpStream + dwMemPos);
        }
        dwMemPos += nTrkSize * 3 + 3;
    }
    // Create the patterns from the list of tracks
    for (UINT iPat=0; iPat<pfh->numorders; iPat++)
    {
        if(Patterns.Insert(iPat, patternLength[iPat]))
        {
            break;
        }
        for (UINT iChn=0; iChn<m_nChannels; iChn++)
        {
            UINT nTrack = LittleEndianW(ptracks[iPat][iChn]);
            if ((nTrack) && (nTrack <= pfh->numtracks))
            {
                UINT realtrk = LittleEndianW(pTrackMap[nTrack-1]);
                if (realtrk)
                {
                    realtrk--;
                    if ((realtrk < realtrackcnt) && (pTrackData[realtrk]))
                    {
                        AMF_Unpack(Patterns[iPat].GetpModCommand(0, iChn), pTrackData[realtrk], Patterns[iPat].GetNumRows(), m_nChannels);
                    }
                }
            }
        }
    }
    delete[] pTrackData;
    // Read Sample Data
    for (UINT iSeek=1; iSeek<=maxsampleseekpos; iSeek++)
    {
        if (dwMemPos >= dwMemLength) break;
        for (UINT iSmp=0; iSmp<m_nSamples; iSmp++) if (iSeek == sampleseekpos[iSmp])
        {
            modsample_t *pSmp = &Samples[iSmp+1];
            dwMemPos += ReadSample(pSmp, RS_PCM8U, (LPCSTR)(lpStream+dwMemPos), dwMemLength-dwMemPos);
            break;
        }
    }
    return true;
}