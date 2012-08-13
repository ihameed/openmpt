/*
 * This source code is public domain.
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
 *			OpenMPT dev(s)	(miscellaneous modifications)
*/

///////////////////////////////////////////////////////////////
//
// DigiBooster Pro Module Loader (*.dbm)
//
// Note: this loader doesn't handle multiple songs
//
///////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Loaders.h"

#define DBM_FILE_MAGIC	0x304d4244
#define DBM_ID_NAME		0x454d414e
#define DBM_NAMELEN		0x2c000000
#define DBM_ID_INFO		0x4f464e49
#define DBM_INFOLEN		0x0a000000
#define DBM_ID_SONG		0x474e4f53
#define DBM_ID_INST		0x54534e49
#define DBM_ID_VENV		0x564e4556
#define DBM_ID_PATT		0x54544150
#define DBM_ID_SMPL		0x4c504d53

#pragma pack(1)

typedef struct DBMFILEHEADER
{
    uint32_t dbm_id;		// "DBM0" = 0x304d4244
    uint16_t trkver;		// Tracker version: 02.15
    uint16_t reserved;
    uint32_t name_id;		// "NAME" = 0x454d414e
    uint32_t name_len;		// name length: always 44
    CHAR songname[44];
    uint32_t info_id;		// "INFO" = 0x4f464e49
    uint32_t info_len;		// 0x0a000000
    uint16_t instruments;
    uint16_t samples;
    uint16_t songs;
    uint16_t patterns;
    uint16_t channels;
    uint32_t song_id;		// "SONG" = 0x474e4f53
    uint32_t song_len;
    CHAR songname2[44];
    uint16_t orders;
//	uint16_t orderlist[0];	// orderlist[orders] in words
} DBMFILEHEADER;

typedef struct DBMINSTRUMENT
{
    CHAR name[30];
    uint16_t sampleno;
    uint16_t volume;
    uint32_t finetune;
    uint32_t loopstart;
    uint32_t looplen;
    uint16_t panning;
    uint16_t flags;
} DBMINSTRUMENT;

typedef struct DBMENVELOPE
{
    uint16_t instrument;
    uint8_t flags;
    uint8_t numpoints;
    uint8_t sustain1;
    uint8_t loopbegin;
    uint8_t loopend;
    uint8_t sustain2;
    uint16_t volenv[2*32];
} DBMENVELOPE;

typedef struct DBMPATTERN
{
    uint16_t rows;
    uint32_t packedsize;
    uint8_t patterndata[2];	// [packedsize]
} DBMPATTERN;

typedef struct DBMSAMPLE
{
    uint32_t flags;
    uint32_t samplesize;
    uint8_t sampledata[2];		// [samplesize]
} DBMSAMPLE;

#pragma pack()


static modplug::tracker::modevent_t::COMMAND dbm_efftrans[23] =
{
    CMD_ARPEGGIO, CMD_PORTAMENTOUP, CMD_PORTAMENTODOWN, CMD_TONEPORTAMENTO,
    CMD_VIBRATO, CMD_TONEPORTAVOL, CMD_VIBRATOVOL, CMD_TREMOLO,
    CMD_PANNING8, CMD_OFFSET, CMD_VOLUMESLIDE, CMD_POSITIONJUMP,
    CMD_VOLUME, CMD_PATTERNBREAK, CMD_MODCMDEX, CMD_TEMPO,
    CMD_GLOBALVOLUME, CMD_GLOBALVOLSLIDE, CMD_KEYOFF, CMD_SETENVPOSITION,
    CMD_CHANNELVOLUME, CMD_CHANNELVOLSLIDE, CMD_PANNINGSLIDE,
};


void ConvertDBMEffect(uint8_t &command, uint8_t &param)
//-------------------------------------------------
{
    if(command < CountOf(dbm_efftrans))
        command = dbm_efftrans[command];
    else
        command = CMD_NONE;

    switch (command)
    {
    case CMD_ARPEGGIO:
        if(param == 0)
            command = CMD_NONE;
        break;
    case CMD_VOLUMESLIDE:
        if(param & 0xF0)
            param &= 0xF0;
        break;
    case CMD_GLOBALVOLUME:
        if(param <= 64)
            param *= 2;
        else
            param = 128;
    case CMD_MODCMDEX:
        switch(param & 0xF0)
        {
        case 0x00:	// set filter
            command = CMD_NONE;
            break;
        case 0x30:	// play backwards
            command = CMD_S3MCMDEX;
            param = 0x9F;
            break;
        case 0x40:	// turn off sound in channel
            // TODO
            break;
        case 0x50:	// turn on/off channel
            // TODO is this correct?
            if((param & 0x0F) <= 0x01)
            {
                command = CMD_CHANNELVOLUME;
                param = (param == 0x50) ? 0x00 : 0x40;
            }
            break;
        case 0x60:	// set loop begin / loop
            // TODO
            break;
        case 0x70:	// set offset
            // TODO
            break;
        case 0xF0:	// turn on/off channel
            // TODO
            break;
        default:
            // Rest will be converted later from CMD_MODCMDEX to CMD_S3MCMDEX.
            break;
        }
        break;
    case CMD_TEMPO:
        if(param <= 0x1F) command = CMD_SPEED;
        break;
    case CMD_KEYOFF:
        if (param == 0)
        {
            // TODO key of at tick 0
        }
        break;
    case CMD_OFFSET:
        // TODO Sample offset slide
        command = CMD_NONE;
        break;
    }
}


bool module_renderer::ReadDBM(const uint8_t *lpStream, const uint32_t dwMemLength)
//---------------------------------------------------------------------
{
    const DBMFILEHEADER *pfh = (DBMFILEHEADER *)lpStream;
    uint32_t dwMemPos;
    uint16_t nOrders, nSamples, nInstruments, nPatterns;

    if ((!lpStream) || (dwMemLength <= sizeof(DBMFILEHEADER)) || (!pfh->channels)
     || (pfh->dbm_id != DBM_FILE_MAGIC) || (!pfh->songs) || (pfh->song_id != DBM_ID_SONG)
     || (pfh->name_id != DBM_ID_NAME) || (pfh->name_len != DBM_NAMELEN)
     || (pfh->info_id != DBM_ID_INFO) || (pfh->info_len != DBM_INFOLEN)) return false;
    dwMemPos = sizeof(DBMFILEHEADER);
    nOrders = BigEndianW(pfh->orders);
    if (dwMemPos + 2 * nOrders + 8*3 >= dwMemLength) return false;
    nInstruments = BigEndianW(pfh->instruments);
    nSamples = BigEndianW(pfh->samples);
    nPatterns = BigEndianW(pfh->patterns);
    m_nType = MOD_TYPE_DBM;
    m_nChannels = CLAMP(BigEndianW(pfh->channels), 1, MAX_BASECHANNELS);	// note: MAX_BASECHANNELS is currently 127, but DBM supports up to 128 channels.
    assign_without_padding(this->song_name, (pfh->songname[0]) ? pfh->songname : pfh->songname2, 32);
    Order.resize(nOrders, Order.GetInvalidPatIndex());
    for (UINT iOrd=0; iOrd < nOrders; iOrd++)
    {
        if (iOrd >= MAX_ORDERS) break;
        Order[iOrd] = (PATTERNINDEX)BigEndianW(*((uint16_t *)(lpStream + dwMemPos + iOrd * 2)));
    }
    dwMemPos += 2*nOrders;
    while (dwMemPos + 10 < dwMemLength)
    {
        uint32_t chunk_id = LittleEndian(((LPDWORD)(lpStream+dwMemPos))[0]);
        uint32_t chunk_size = BigEndian(((LPDWORD)(lpStream+dwMemPos))[1]);
        uint32_t chunk_pos;

        dwMemPos += 8;
        chunk_pos = dwMemPos;
        if ((dwMemPos + chunk_size > dwMemLength) || (chunk_size > dwMemLength)) break;
        dwMemPos += chunk_size;
        // Instruments
        if (chunk_id == DBM_ID_INST)
        {
            if (nInstruments >= MAX_INSTRUMENTS) nInstruments = MAX_INSTRUMENTS-1;
            for(INSTRUMENTINDEX iIns = 0; iIns < nInstruments; iIns++)
            {
                modsample_t *psmp;
                modinstrument_t *pIns;
                DBMINSTRUMENT *pih;
                uint16_t nsmp;

                if (chunk_pos + sizeof(DBMINSTRUMENT) > dwMemPos) break;
                if ((pIns = new modinstrument_t) == nullptr) break;
                memset(pIns, 0, sizeof(modinstrument_t));
                SetDefaultInstrumentValues(pIns);
                Instruments[iIns + 1] = pIns;

                pih = (DBMINSTRUMENT *)(lpStream + chunk_pos);
                nsmp = BigEndianW(pih->sampleno);
                psmp = ((nsmp) && (nsmp < MAX_SAMPLES)) ? &Samples[nsmp] : nullptr;
                memcpy(pIns->name, pih->name, 30);
                SpaceToNullStringFixed<30>(pIns->name);
                if (psmp)
                {
                    memcpy(m_szNames[nsmp], pih->name, 30);
                    SpaceToNullStringFixed<30>(m_szNames[nsmp]);
                }

                pIns->fadeout = 1024;	// ???
                pIns->global_volume = 64;
                pIns->default_pan = BigEndianW(pih->panning);
                if ((pIns->default_pan) && (pIns->default_pan < 256))
                    pIns->flags = INS_SETPANNING;
                else
                    pIns->default_pan = 128;
                pIns->pitch_pan_center = 5 * 12;

                for (uint8_t i = 0; i < NOTE_MAX; i++)
                {
                    pIns->Keyboard[i] = nsmp;
                    pIns->NoteMap[i] = i + 1;
                }
                // Sample Info
                if (psmp)
                {
                    uint32_t sflags = BigEndianW(pih->flags);
                    psmp->default_volume = BigEndianW(pih->volume) * 4;
                    if ((!psmp->default_volume) || (psmp->default_volume > 256)) psmp->default_volume = 256;
                    psmp->global_volume = 64;
                    psmp->c5_samplerate = BigEndian(pih->finetune);
                    int f2t = FrequencyToTranspose(psmp->c5_samplerate);
                    psmp->RelativeTone = (signed char)(f2t >> 7);
                    psmp->nFineTune = f2t & 0x7F;
                    if ((pih->looplen) && (sflags & 3))
                    {
                        psmp->loop_start = BigEndian(pih->loopstart);
                        psmp->loop_end = psmp->loop_start + BigEndian(pih->looplen);
                        psmp->flags |= CHN_LOOP;
                        psmp->flags &= ~CHN_PINGPONGLOOP;
                        if (sflags & 2) psmp->flags |= CHN_PINGPONGLOOP;
                    }
                }
                chunk_pos += sizeof(DBMINSTRUMENT);
                m_nInstruments = iIns + 1;
            }
        } else
        // Volume Envelopes
        if (chunk_id == DBM_ID_VENV)
        {
            UINT nEnvelopes = lpStream[chunk_pos+1];

            chunk_pos += 2;
            for (UINT iEnv=0; iEnv<nEnvelopes; iEnv++)
            {
                DBMENVELOPE *peh;
                UINT nins;

                if (chunk_pos + sizeof(DBMENVELOPE) > dwMemPos) break;
                peh = (DBMENVELOPE *)(lpStream+chunk_pos);
                nins = BigEndianW(peh->instrument);
                if ((nins) && (nins < MAX_INSTRUMENTS) && (Instruments[nins]) && (peh->numpoints))
                {
                    modinstrument_t *pIns = Instruments[nins];

                    if (peh->flags & 1) pIns->volume_envelope.flags |= ENV_ENABLED;
                    if (peh->flags & 2) pIns->volume_envelope.flags |= ENV_SUSTAIN;
                    if (peh->flags & 4) pIns->volume_envelope.flags |= ENV_LOOP;
                    pIns->volume_envelope.num_nodes = peh->numpoints + 1;
                    if (pIns->volume_envelope.num_nodes > MAX_ENVPOINTS) pIns->volume_envelope.num_nodes = MAX_ENVPOINTS;
                    pIns->volume_envelope.loop_start = peh->loopbegin;
                    pIns->volume_envelope.loop_end = peh->loopend;
                    pIns->volume_envelope.sustain_start = pIns->volume_envelope.sustain_end = peh->sustain1;
                    for (UINT i=0; i<pIns->volume_envelope.num_nodes; i++)
                    {
                        pIns->volume_envelope.Ticks[i] = BigEndianW(peh->volenv[i*2]);
                        pIns->volume_envelope.Values[i] = (uint8_t)BigEndianW(peh->volenv[i*2+1]);
                    }
                }
                chunk_pos += sizeof(DBMENVELOPE);
            }
        } else
        // Packed Pattern Data
        if (chunk_id == DBM_ID_PATT)
        {
            if (nPatterns > MAX_PATTERNS) nPatterns = MAX_PATTERNS;
            for(PATTERNINDEX iPat = 0; iPat < nPatterns; iPat++)
            {
                DBMPATTERN *pph;
                uint32_t pksize;
                UINT nRows;

                if (chunk_pos + sizeof(DBMPATTERN) > dwMemPos) break;
                pph = (DBMPATTERN *)(lpStream+chunk_pos);
                pksize = BigEndian(pph->packedsize);
                if ((chunk_pos + pksize + 6 > dwMemPos) || (pksize > dwMemPos)) break;
                nRows = BigEndianW(pph->rows);
                if ((nRows >= 4) && (nRows <= 256))
                {
                    Patterns.Insert(iPat, nRows);
                    modplug::tracker::modevent_t *m = Patterns[iPat];
                    if (m)
                    {
                        LPBYTE pkdata = (LPBYTE)&pph->patterndata;
                        UINT row = 0;
                        UINT i = 0;

                        while ((i+3<pksize) && (row < nRows))
                        {
                            UINT ch = pkdata[i++];

                            if (ch)
                            {
                                uint8_t b = pkdata[i++];
                                ch--;
                                if (ch < m_nChannels)
                                {
                                    if (b & 0x01)
                                    {
                                        uint8_t note = pkdata[i++];

                                        if (note == 0x1F) note = NOTE_KEYOFF; else
                                        if ((note) && (note < 0xFE))
                                        {
                                            note = ((note >> 4) * 12) + (note & 0x0F) + 13;
                                        }
                                        m[ch].note = note;
                                    }
                                    if (b & 0x02) m[ch].instr = pkdata[i++];
                                    if (b & 0x3C)
                                    {
                                        uint8_t cmd1 = CMD_NONE, cmd2 = CMD_NONE;
                                        uint8_t param1 = 0, param2 = 0;
                                        if (b & 0x04) cmd2 = pkdata[i++];
                                        if (b & 0x08) param2 = pkdata[i++];
                                        if (b & 0x10) cmd1 = pkdata[i++];
                                        if (b & 0x20) param1 = pkdata[i++];
                                        ConvertDBMEffect(cmd1, param1);
                                        ConvertDBMEffect(cmd2, param2);

                                        // this is the same conversion algorithm as in the ULT loader. maybe this should be merged at some point...
                                        if (cmd2 == CMD_VOLUME || (cmd2 == CMD_NONE && cmd1 != CMD_VOLUME))
                                        {
                                            std::swap(cmd1, cmd2);
                                            std::swap(param1, param2);
                                        }

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
                                            if (module_renderer::GetEffectWeight((modplug::tracker::modevent_t::COMMAND)cmd1) > module_renderer::GetEffectWeight((modplug::tracker::modevent_t::COMMAND)cmd2))
                                            {
                                                std::swap(cmd1, cmd2);
                                                std::swap(param1, param2);
                                            }
                                            cmd1 = CMD_NONE;
                                        }
                                        if (!cmd1)
                                            param1 = 0;
                                        if (!cmd2)
                                            param2 = 0;

                                        m[ch].volcmd = cmd1;
                                        m[ch].vol = param1;
                                        m[ch].command = cmd2;
                                        m[ch].param = param2;
                                        MODExx2S3MSxx(&m[ch]);
                                    }
                                } else
                                {
                                    if (b & 0x01) i++;
                                    if (b & 0x02) i++;
                                    if (b & 0x04) i++;
                                    if (b & 0x08) i++;
                                    if (b & 0x10) i++;
                                    if (b & 0x20) i++;
                                }
                            } else
                            {
                                row++;
                                m += m_nChannels;
                            }
                        }
                    }
                }
                chunk_pos += 6 + pksize;
            }
        } else
        // Reading Sample Data
        if (chunk_id == DBM_ID_SMPL)
        {
            if (nSamples >= MAX_SAMPLES) nSamples = MAX_SAMPLES-1;
            m_nSamples = nSamples;
            for (UINT iSmp=1; iSmp<=nSamples; iSmp++)
            {
                modsample_t *pSmp;
                DBMSAMPLE *psh;
                uint32_t samplesize;
                uint32_t sampleflags;

                if (chunk_pos + sizeof(DBMSAMPLE) >= dwMemPos) break;
                psh = (DBMSAMPLE *)(lpStream+chunk_pos);
                chunk_pos += 8;
                samplesize = BigEndian(psh->samplesize);
                sampleflags = BigEndian(psh->flags);
                pSmp = &Samples[iSmp];
                pSmp->length = samplesize;
                if (sampleflags & 2)
                {
                    pSmp->flags |= CHN_16BIT;
                    samplesize <<= 1;
                }
                if ((chunk_pos+samplesize > dwMemPos) || (samplesize > dwMemLength)) break;
                if (sampleflags & 3)
                {
                    ReadSample(pSmp, (pSmp->flags & CHN_16BIT) ? RS_PCM16M : RS_PCM8S,
                                (LPSTR)(psh->sampledata), samplesize);
                }
                chunk_pos += samplesize;
            }
        }
    }
    return true;
}