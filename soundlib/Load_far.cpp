/*
 * This source code is public domain. 
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *    		OpenMPT dev(s)	(miscellaneous modifications)
*/

////////////////////////////////////////
// Farandole (FAR) module loader      //
////////////////////////////////////////
#include "stdafx.h"
#include "Loaders.h"

#pragma warning(disable:4244) //"conversion from 'type1' to 'type2', possible loss of data"

#define FARFILEMAGIC    0xFE524146	// "FAR"

#pragma pack(1)

typedef struct FARHEADER1
{
    uint32_t id;    			// file magic FAR=
    CHAR songname[40];    	// songname
    CHAR magic2[3];    		// 13,10,26
    uint16_t headerlen;    		// remaining length of header in bytes
    uint8_t version;    		// 0xD1
    uint8_t onoff[16];
    uint8_t edit1[9];
    uint8_t speed;
    uint8_t panning[16];
    uint8_t edit2[4];
    uint16_t stlen;
} FARHEADER1;

typedef struct FARHEADER2
{
    uint8_t orders[256];
    uint8_t numpat;
    uint8_t snglen;
    uint8_t loopto;
    uint16_t patsiz[256];
} FARHEADER2;

typedef struct FARSAMPLE
{
    CHAR samplename[32];
    uint32_t length;
    uint8_t finetune;
    uint8_t volume;
    uint32_t reppos;
    uint32_t repend;
    uint8_t type;
    uint8_t loop;
} FARSAMPLE;

#pragma pack()


bool CSoundFile::ReadFAR(const uint8_t *lpStream, const uint32_t dwMemLength)
//---------------------------------------------------------------------
{
    if(dwMemLength < sizeof(FARHEADER1))
        return false;

    FARHEADER1 farHeader;
    memcpy(&farHeader, lpStream, sizeof(FARHEADER1));
    FARHEADER1 *pmh1 = &farHeader;
    FARHEADER2 *pmh2 = 0;
    uint32_t dwMemPos = sizeof(FARHEADER1);
    UINT headerlen;
    uint8_t samplemap[8];

    if ((!lpStream) || (dwMemLength < 1024) || (LittleEndian(pmh1->id) != FARFILEMAGIC)
     || (pmh1->magic2[0] != 13) || (pmh1->magic2[1] != 10) || (pmh1->magic2[2] != 26)) return false;
    headerlen = LittleEndianW(pmh1->headerlen);
    pmh1->stlen = LittleEndianW( pmh1->stlen ); /* inplace byteswap -- Toad */
    if ((headerlen >= dwMemLength) || (dwMemPos + pmh1->stlen + sizeof(FARHEADER2) >= dwMemLength)) return false;
    // Globals
    m_nType = MOD_TYPE_FAR;
    m_nChannels = 16;
    m_nInstruments = 0;
    m_nSamples = 0;
    m_nSamplePreAmp = 0x20;
    m_nDefaultSpeed = pmh1->speed;
    m_nDefaultTempo = 80;
    m_nDefaultGlobalVolume = MAX_GLOBAL_VOLUME;

    assign_without_padding(this->song_name, pmh1->songname, 31);

    // Channel Setting
    for (UINT nchpan=0; nchpan<16; nchpan++)
    {
        ChnSettings[nchpan].dwFlags = 0;
        ChnSettings[nchpan].nPan = ((pmh1->panning[nchpan] & 0x0F) << 4) + 8;
        ChnSettings[nchpan].nVolume = 64;
    }
    // Reading comment
    if (pmh1->stlen)
    {
        UINT szLen = pmh1->stlen;
        if (szLen > dwMemLength - dwMemPos) szLen = dwMemLength - dwMemPos;
        ReadFixedLineLengthMessage(lpStream + dwMemPos, szLen, 132, 0);    // 132 characters per line... wow. :)
        dwMemPos += pmh1->stlen;
    }
    // Reading orders
    if (sizeof(FARHEADER2) > dwMemLength - dwMemPos) return true;
    FARHEADER2 farHeader2;
    memcpy(&farHeader2, lpStream + dwMemPos, sizeof(FARHEADER2));
    pmh2 = &farHeader2;
    dwMemPos += sizeof(FARHEADER2);
    if (dwMemPos >= dwMemLength) return true;

    Order.ReadAsByte(pmh2->orders, pmh2->snglen, sizeof(pmh2->orders));
    m_nRestartPos = pmh2->loopto;
    // Reading Patterns    
    dwMemPos += headerlen - (869 + pmh1->stlen);
    if (dwMemPos >= dwMemLength) return true;

    // byteswap pattern data.
    for(uint16_t psfix = 0; psfix < 256; psfix++)
    {
        pmh2->patsiz[psfix] = LittleEndianW( pmh2->patsiz[psfix] ) ;
    }
    // end byteswap of pattern data

    uint16_t *patsiz = (uint16_t *)pmh2->patsiz;
    for (UINT ipat=0; ipat<256; ipat++) if (patsiz[ipat])
    {
        UINT patlen = patsiz[ipat];
        if ((ipat >= MAX_PATTERNS) || (patsiz[ipat] < 2))
        {
            dwMemPos += patlen;
            continue;
        }
        if (dwMemPos + patlen >= dwMemLength) return true;
        UINT rows = (patlen - 2) >> 6;
        if (!rows)
        {
            dwMemPos += patlen;
            continue;
        }
        if (rows > 256) rows = 256;
        if (rows < 16) rows = 16;
        if(Patterns.Insert(ipat, rows)) return true;
        modplug::tracker::modcommand_t *m = Patterns[ipat];
        UINT patbrk = lpStream[dwMemPos];
        const uint8_t *p = lpStream + dwMemPos + 2;
        UINT max = rows*16*4;
        if (max > patlen-2) max = patlen-2;
        for (UINT len=0; len<max; len += 4, m++)
        {
            uint8_t note = p[len];
            uint8_t ins = p[len+1];
            uint8_t vol = p[len+2];
            uint8_t eff = p[len+3];
            if (note)
            {
                m->instr = ins + 1;
                m->note = note + 36;
            }
            if (vol & 0x0F)
            {
                m->volcmd = VOLCMD_VOLUME;
                m->vol = (vol & 0x0F) << 2;
                if (m->vol <= 4) m->vol = 0;
            }
            switch(eff & 0xF0)
            {
            // 1.x: Portamento Up
            case 0x10:
                m->command = CMD_PORTAMENTOUP;
                m->param = eff & 0x0F;
                break;
            // 2.x: Portamento Down
            case 0x20:
                m->command = CMD_PORTAMENTODOWN;
                m->param = eff & 0x0F;
                break;
            // 3.x: Tone-Portamento
            case 0x30:
                m->command = CMD_TONEPORTAMENTO;
                m->param = (eff & 0x0F) << 2;
                break;
            // 4.x: Retrigger
            case 0x40:
                m->command = CMD_RETRIG;
                m->param = 6 / (1+(eff&0x0F)) + 1;
                break;
            // 5.x: Set Vibrato Depth
            case 0x50:
                m->command = CMD_VIBRATO;
                m->param = (eff & 0x0F);
                break;
            // 6.x: Set Vibrato Speed
            case 0x60:
                m->command = CMD_VIBRATO;
                m->param = (eff & 0x0F) << 4;
                break;
            // 7.x: Vol Slide Up
            case 0x70:
                m->command = CMD_VOLUMESLIDE;
                m->param = (eff & 0x0F) << 4;
                break;
            // 8.x: Vol Slide Down
            case 0x80:
                m->command = CMD_VOLUMESLIDE;
                m->param = (eff & 0x0F);
                break;
            // A.x: Port to vol
            case 0xA0:
                m->volcmd = VOLCMD_VOLUME;
                m->vol = ((eff & 0x0F) << 2) + 4;
                break;
            // B.x: Set Balance
            case 0xB0:
                m->command = CMD_PANNING8;
                m->param = (eff & 0x0F) << 4;
                break;
            // F.x: Set Speed
            case 0xF0:
                m->command = CMD_SPEED;
                m->param = eff & 0x0F;
                break;
            default:
                if ((patbrk) &&    (patbrk+1 == (len >> 6)) && (patbrk+1 != rows-1))
                {
                    m->command = CMD_PATTERNBREAK;
                    patbrk = 0;
                }
            }
        }
        dwMemPos += patlen;
    }
    // Reading samples
    if (dwMemPos + 8 >= dwMemLength) return true;
    memcpy(samplemap, lpStream+dwMemPos, 8);
    dwMemPos += 8;
    modsample_t *pSmp = &Samples[1];
    for (UINT ismp=0; ismp<64; ismp++, pSmp++) if (samplemap[ismp >> 3] & (1 << (ismp & 7)))
    {
        if (dwMemPos + sizeof(FARSAMPLE) > dwMemLength) return true;
        const FARSAMPLE *pfs = reinterpret_cast<const FARSAMPLE*>(lpStream + dwMemPos);
        dwMemPos += sizeof(FARSAMPLE);
        m_nSamples = ismp + 1;
        memcpy(m_szNames[ismp+1], pfs->samplename, 31);
        SpaceToNullStringFixed<31>(m_szNames[ismp + 1]);
        const uint32_t length = LittleEndian( pfs->length );
        pSmp->length = length;
        pSmp->loop_start = LittleEndian(pfs->reppos) ;
        pSmp->loop_end = LittleEndian(pfs->repend) ;
        pSmp->nFineTune = 0;
        pSmp->c5_samplerate = 8363*2;
        pSmp->global_volume = 64;
        pSmp->default_volume = pfs->volume << 4;
        pSmp->flags = 0;
        if ((pSmp->length > 3) && (dwMemPos + 4 < dwMemLength))
        {
            if (pfs->type & 1)
            {
                pSmp->flags |= CHN_16BIT;
                pSmp->length >>= 1;
                pSmp->loop_start >>= 1;
                pSmp->loop_end >>= 1;
            }
            if ((pfs->loop & 8) && (pSmp->loop_end > 4)) pSmp->flags |= CHN_LOOP;
            ReadSample(pSmp, (pSmp->flags & CHN_16BIT) ? RS_PCM16S : RS_PCM8S,
                        (LPSTR)(lpStream+dwMemPos), dwMemLength - dwMemPos);
        }
        dwMemPos += length;
    }
    return true;
}

