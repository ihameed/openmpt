/*
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
 *                    OpenMPT dev(s)        (miscellaneous modifications)
*/

#include "stdafx.h"
#include "Loaders.h"
#include "../version.h"
#include "../misc_util.h"

////////////////////////////////////////////////////////
// FastTracker II XM file support

#pragma warning(disable:4244) //conversion from 'type1' to 'type2', possible loss of data

#define str_tooMuchPatternData    (GetStrI18N((_TEXT("Warning: File format limit was reached. Some pattern data may not get written to file."))))
#define str_pattern                            (GetStrI18N((_TEXT("pattern"))))


#pragma pack(1)
typedef struct tagXMFILEHEADER
{
    uint16_t xmversion;            // current: 0x0104
    uint32_t size;                    // header size
    uint16_t orders;            // number of orders
    uint16_t restartpos;    // restart position
    uint16_t channels;            // number of channels
    uint16_t patterns;            // number of patterns
    uint16_t instruments;    // number of instruments
    uint16_t flags;                    // song flags
    uint16_t speed;                    // default speed
    uint16_t tempo;                    // default tempo
} XMFILEHEADER;


typedef struct tagXMINSTRUMENTHEADER
{
    uint32_t size; // size of XMINSTRUMENTHEADER + XMSAMPLEHEADER
    CHAR name[22];
    uint8_t type; // should always be 0
    uint16_t samples;
} XMINSTRUMENTHEADER;


typedef struct tagXMSAMPLEHEADER
{
    uint32_t shsize; // size of XMSAMPLESTRUCT
    uint8_t snum[96];
    uint16_t venv[24];
    uint16_t penv[24];
    uint8_t vnum, pnum;
    uint8_t vsustain, vloops, vloope, psustain, ploops, ploope;
    uint8_t vtype, ptype;
    uint8_t vibtype, vibsweep, vibdepth, vibrate;
    uint16_t volfade;
    // midi extensions (not read by MPT)
    uint8_t midienabled;            // 0/1
    uint8_t midichannel;            // 0...15
    uint16_t midiprogram;            // 0...127
    uint16_t pitchwheelrange;    // 0...36 (halftones)
    uint8_t mutecomputer;            // 0/1
    uint8_t reserved1[15];
} XMSAMPLEHEADER;

typedef struct tagXMSAMPLESTRUCT
{
    uint32_t samplen;
    uint32_t loopstart;
    uint32_t looplen;
    uint8_t vol;
    signed char finetune;
    uint8_t type;
    uint8_t pan;
    signed char relnote;
    uint8_t res;
    char name[22];
} XMSAMPLESTRUCT;
#pragma pack()


// Read .XM patterns
uint32_t ReadXMPatterns(const uint8_t *lpStream, uint32_t dwMemLength, uint32_t dwMemPos, XMFILEHEADER *xmheader, module_renderer *pSndFile)
//-------------------------------------------------------------------------------------------------------------------------
{
    uint8_t patterns_used[256];
    uint8_t pattern_map[256];

    memset(patterns_used, 0, sizeof(patterns_used));
    if (xmheader->patterns > MAX_PATTERNS)
    {
        UINT i, j;
        for (i = 0; i < xmheader->orders; i++)
        {
            if (pSndFile->Order[i] < xmheader->patterns) patterns_used[pSndFile->Order[i]] = true;
        }
        j = 0;
        for (i = 0; i < 256; i++)
        {
            if (patterns_used[i]) pattern_map[i] = j++;
        }
        for (i = 0; i < 256; i++)
        {
            if (!patterns_used[i])
            {
                pattern_map[i] = (j < MAX_PATTERNS) ? j : 0xFE;
                j++;
            }
        }
        for (i = 0; i < xmheader->orders; i++)
        {
            pSndFile->Order[i] = pattern_map[pSndFile->Order[i]];
        }
    } else
    {
        for (UINT i = 0; i < 256; i++) pattern_map[i] = i;
    }
    if (dwMemPos + 8 >= dwMemLength) return dwMemPos;
    // Reading patterns
    for (UINT ipat = 0; ipat < xmheader->patterns; ipat++)
    {
        UINT ipatmap = pattern_map[ipat];
        uint32_t dwSize = 0;
        uint16_t rows = 64, packsize = 0;
        dwSize = LittleEndian(*((uint32_t *)(lpStream + dwMemPos)));

        if(xmheader->xmversion == 0x0102)
        {
            rows = *((uint8_t *)(lpStream + dwMemPos + 5)) + 1;
            packsize = LittleEndianW(*((uint16_t *)(lpStream + dwMemPos + 6)));
        }
        else
        {
            rows = LittleEndianW(*((uint16_t *)(lpStream + dwMemPos + 5)));
            packsize = LittleEndianW(*((uint16_t *)(lpStream + dwMemPos + 7)));
        }

        if ((!rows) || (rows > MAX_PATTERN_ROWS)) rows = 64;
        if (dwMemPos + dwSize + 4 > dwMemLength) return 0;
        dwMemPos += dwSize;
        if (dwMemPos + packsize > dwMemLength) return 0;
        modplug::tracker::modevent_t *p;
        if (ipatmap < MAX_PATTERNS)
        {
            if(pSndFile->Patterns.Insert(ipatmap, rows))
                return true;

            if (!packsize) continue;
            p = pSndFile->Patterns[ipatmap];
        } else p = NULL;
        const uint8_t *src = lpStream+dwMemPos;
        UINT j=0;
        for (UINT row=0; row<rows; row++)
        {
            for (UINT chn=0; chn < xmheader->channels; chn++)
            {
                if ((p) && (j < packsize))
                {
                    uint8_t b = src[j++];
                    UINT vol = 0;
                    if (b & 0x80)
                    {
                        if (b & 1) p->note = src[j++];
                        if (b & 2) p->instr = src[j++];
                        if (b & 4) vol = src[j++];
                        //XXXih: gross!!
                        if (b & 8) p->command = (modplug::tracker::cmd_t) src[j++];
                        if (b & 16) p->param = src[j++];
                    } else
                    {
                        p->note = b;
                        p->instr = src[j++];
                        vol = src[j++];
                        //XXXih: gross!!
                        p->command = (modplug::tracker::cmd_t) src[j++];
                        p->param = src[j++];
                    }
                    if (p->note == 97) p->note = NoteKeyOff; else
                    if ((p->note) && (p->note < 97)) p->note += 12;
                    if (p->command | p->param) pSndFile->ConvertModCommand(p);
                    if (p->instr == 0xff) p->instr = 0;
                    if ((vol >= 0x10) && (vol <= 0x50))
                    {
                        p->volcmd = VolCmdVol;
                        p->vol = vol - 0x10;
                    } else
                    if (vol >= 0x60)
                    {
                        UINT v = vol & 0xF0;
                        vol &= 0x0F;
                        p->vol = vol;
                        switch(v)
                        {
                        // 60-6F: Volume Slide Down
                        case 0x60:    p->volcmd = VolCmdSlideDown; break;
                        // 70-7F: Volume Slide Up:
                        case 0x70:    p->volcmd = VolCmdSlideUp; break;
                        // 80-8F: Fine Volume Slide Down
                        case 0x80:    p->volcmd = VolCmdFineDown; break;
                        // 90-9F: Fine Volume Slide Up
                        case 0x90:    p->volcmd = VolCmdFineUp; break;
                        // A0-AF: Set Vibrato Speed
                        case 0xA0:    p->volcmd = VolCmdVibratoSpeed; break;
                        // B0-BF: Vibrato
                        case 0xB0:    p->volcmd = VolCmdVibratoDepth; break;
                        // C0-CF: Set Panning
                        case 0xC0:    p->volcmd = VolCmdPan; p->vol = (vol << 2) + 2; break;
                        // D0-DF: Panning Slide Left
                        case 0xD0:    p->volcmd = VolCmdPanSlideLeft; break;
                        // E0-EF: Panning Slide Right
                        case 0xE0:    p->volcmd = VolCmdPanSlideRight; break;
                        // F0-FF: Tone Portamento
                        case 0xF0:    p->volcmd = VolCmdPortamento; break;
                        }
                    }
                    p++;
                } else
                if (j < packsize)
                {
                    uint8_t b = src[j++];
                    if (b & 0x80)
                    {
                        if (b & 1) j++;
                        if (b & 2) j++;
                        if (b & 4) j++;
                        if (b & 8) j++;
                        if (b & 16) j++;
                    } else j += 4;
                } else break;
            }
        }
        dwMemPos += packsize;
    }
    return dwMemPos;
}

bool module_renderer::ReadXM(const uint8_t *lpStream, const uint32_t dwMemLength)
//--------------------------------------------------------------------
{
    XMFILEHEADER xmheader;
    XMSAMPLEHEADER xmsh;
    XMSAMPLESTRUCT xmss;
    uint32_t dwMemPos;

    bool bMadeWithModPlug = false, bProbablyMadeWithModPlug = false, bProbablyMPT109 = false, bIsFT2 = false;

    m_nChannels = 0;
    if ((!lpStream) || (dwMemLength < 0xAA)) return false; // the smallest XM I know is 174 Bytes
    if (_strnicmp((LPCSTR)lpStream, "Extended Module", 15)) return false;

    // look for null-terminated song name - that's most likely a tune made with modplug
    for(int i = 0; i < 20; i++)
        if(lpStream[17 + i] == 0) bProbablyMadeWithModPlug = true;
    assign_without_padding(this->song_name, reinterpret_cast<const char *>(lpStream + 17), 20);

    // load and convert header
    memcpy(&xmheader, lpStream + 58, sizeof(XMFILEHEADER));
    xmheader.size = LittleEndian(xmheader.size);
    xmheader.xmversion = LittleEndianW(xmheader.xmversion);
    xmheader.orders = LittleEndianW(xmheader.orders);
    xmheader.restartpos = LittleEndianW(xmheader.restartpos);
    xmheader.channels = LittleEndianW(xmheader.channels);
    xmheader.patterns = LittleEndianW(xmheader.patterns);
    xmheader.instruments = LittleEndianW(xmheader.instruments);
    xmheader.flags = LittleEndianW(xmheader.flags);
    xmheader.speed = LittleEndianW(xmheader.speed);
    xmheader.tempo = LittleEndianW(xmheader.tempo);

    m_nType = MOD_TYPE_XM;
    m_nMinPeriod = 27;
    m_nMaxPeriod = 54784;

    if (xmheader.orders > MAX_ORDERS) return false;
    if ((!xmheader.channels) || (xmheader.channels > MAX_BASECHANNELS)) return false;
    if (xmheader.channels > 32) bMadeWithModPlug = true;
    m_nRestartPos = xmheader.restartpos;
    m_nChannels = xmheader.channels;
    m_nInstruments = min(xmheader.instruments, MAX_INSTRUMENTS - 1);
    m_nSamples = 0;
    m_nDefaultSpeed = CLAMP(xmheader.speed, 1, 31);
    m_nDefaultTempo = CLAMP(xmheader.tempo, 32, 512);

    if(xmheader.flags & 1) m_dwSongFlags |= SONG_LINEARSLIDES;
    if(xmheader.flags & 0x1000) m_dwSongFlags |= SONG_EXFILTERRANGE;

    Order.ReadAsByte(lpStream + 80, xmheader.orders, dwMemLength - 80);

    dwMemPos = xmheader.size + 60;

    // set this here already because XMs compressed with BoobieSqueezer will exit the function early
    SetModFlag(MSF_COMPATIBLE_PLAY, true);

    if(xmheader.xmversion >= 0x0104)
    {
        if (dwMemPos + 8 >= dwMemLength) return true;
        dwMemPos = ReadXMPatterns(lpStream, dwMemLength, dwMemPos, &xmheader, this);
        if(dwMemPos == 0) return true;
    }

    vector<bool> samples_used; // for removing unused samples
    modplug::tracker::sampleindex_t unused_samples = 0; // dito

    // Reading instruments
    for (modplug::tracker::instrumentindex_t iIns = 1; iIns <= m_nInstruments; iIns++)
    {
        XMINSTRUMENTHEADER pih;
        uint8_t flags[32];
        uint32_t samplesize[32];
        UINT samplemap[32];
        uint16_t nsamples;

        if (dwMemPos + sizeof(uint32_t) >= dwMemLength) return true;
        uint32_t ihsize = LittleEndian(*((uint32_t *)(lpStream + dwMemPos)));
        if (dwMemPos + ihsize > dwMemLength) return true;

        MemsetZero(pih);
        memcpy(&pih, lpStream + dwMemPos, min(sizeof(pih), ihsize));

        if ((Instruments[iIns] = new modinstrument_t) == nullptr) continue;
        memcpy(Instruments[iIns], &m_defaultInstrument, sizeof(modinstrument_t));
        Instruments[iIns]->nPluginVelocityHandling = PLUGIN_VELOCITYHANDLING_CHANNEL;
        Instruments[iIns]->nPluginVolumeHandling = PLUGIN_VOLUMEHANDLING_IGNORE;

        memcpy(Instruments[iIns]->name, pih.name, 22);
        SpaceToNullStringFixed<22>(Instruments[iIns]->name);

        memset(&xmsh, 0, sizeof(XMSAMPLEHEADER));

        if ((nsamples = pih.samples) > 0)
        {
            /* we have samples, so let's read the rest of this instrument
               the header that is being read here is not the sample header, though,
               it's rather the instrument settings. */

            if (dwMemPos + ihsize >= dwMemLength)
                return true;

            memcpy(&xmsh,
                lpStream + dwMemPos + sizeof(XMINSTRUMENTHEADER),
                min(ihsize - sizeof(XMINSTRUMENTHEADER), sizeof(XMSAMPLEHEADER)));

            xmsh.shsize = LittleEndian(xmsh.shsize);
            if(xmsh.shsize == 0 && bProbablyMadeWithModPlug) bMadeWithModPlug = true;

            for (int i = 0; i < 24; ++i) {
                xmsh.venv[i] = LittleEndianW(xmsh.venv[i]);
                xmsh.penv[i] = LittleEndianW(xmsh.penv[i]);
            }
            xmsh.volfade = LittleEndianW(xmsh.volfade);
            xmsh.midiprogram = LittleEndianW(xmsh.midiprogram);
            xmsh.pitchwheelrange = LittleEndianW(xmsh.pitchwheelrange);

            if(xmsh.midichannel != 0 || xmsh.midienabled != 0 || xmsh.midiprogram != 0 || xmsh.mutecomputer != 0 || xmsh.pitchwheelrange != 0)
                bIsFT2 = true; // definitely not MPT. (or any other tracker)

        }

        if (LittleEndian(pih.size))
            dwMemPos += LittleEndian(pih.size);
        else
            dwMemPos += sizeof(XMINSTRUMENTHEADER);

        memset(samplemap, 0, sizeof(samplemap));
        if (nsamples > 32) return true;
        UINT newsamples = m_nSamples;

        for (UINT nmap = 0; nmap < nsamples; nmap++)
        {
            UINT n = m_nSamples + nmap + 1;
            if (n >= MAX_SAMPLES)
            {
                n = m_nSamples;
                while (n > 0)
                {
                    if (!Samples[n].sample_data)
                    {
                        for (UINT xmapchk=0; xmapchk < nmap; xmapchk++)
                        {
                            if (samplemap[xmapchk] == n) goto alreadymapped;
                        }
                        for (UINT clrs=1; clrs<iIns; clrs++) if (Instruments[clrs])
                        {
                            modinstrument_t *pks = Instruments[clrs];
                            for (UINT ks=0; ks<128; ks++)
                            {
                                if (pks->Keyboard[ks] == n) pks->Keyboard[ks] = 0;
                            }
                        }
                        break;
                    }
                alreadymapped:
                    n--;
                }
#ifndef FASTSOUNDLIB
                // Damn! Too many samples: look for duplicates
                if (!n)
                {
                    if (!unused_samples)
                    {
                        unused_samples = DetectUnusedSamples(samples_used);
                        if (!unused_samples) unused_samples = modplug::tracker::SampleIndexInvalid;
                    }
                    if ((unused_samples) && (unused_samples != modplug::tracker::SampleIndexInvalid))
                    {
                        for (UINT iext=m_nSamples; iext>=1; iext--) if (!samples_used[iext])
                        {
                            unused_samples--;
                            samples_used[iext] = true;
                            DestroySample(iext);
                            n = iext;
                            for (UINT mapchk=0; mapchk<nmap; mapchk++)
                            {
                                if (samplemap[mapchk] == n) samplemap[mapchk] = 0;
                            }
                            for (UINT clrs=1; clrs<iIns; clrs++) if (Instruments[clrs])
                            {
                                modinstrument_t *pks = Instruments[clrs];
                                for (UINT ks=0; ks<128; ks++)
                                {
                                    if (pks->Keyboard[ks] == n) pks->Keyboard[ks] = 0;
                                }
                            }
                            MemsetZero(Samples[n]);
                            break;
                        }
                    }
                }
#endif // FASTSOUNDLIB
            }
            if (newsamples < n) newsamples = n;
            samplemap[nmap] = n;
        }
        m_nSamples = newsamples;
        // Reading Volume Envelope
        modinstrument_t *pIns = Instruments[iIns];
        pIns->midi_program = pih.type;
        pIns->fadeout = xmsh.volfade;
        pIns->default_pan = 128;
        pIns->pitch_pan_center = 5*12;
        SetDefaultInstrumentValues(pIns);
        pIns->nPluginVelocityHandling = PLUGIN_VELOCITYHANDLING_CHANNEL;
        pIns->nPluginVolumeHandling = PLUGIN_VOLUMEHANDLING_IGNORE;
        if (xmsh.vtype & 1) pIns->volume_envelope.flags |= ENV_ENABLED;
        if (xmsh.vtype & 2) pIns->volume_envelope.flags |= ENV_SUSTAIN;
        if (xmsh.vtype & 4) pIns->volume_envelope.flags |= ENV_LOOP;
        if (xmsh.ptype & 1) pIns->panning_envelope.flags |= ENV_ENABLED;
        if (xmsh.ptype & 2) pIns->panning_envelope.flags |= ENV_SUSTAIN;
        if (xmsh.ptype & 4) pIns->panning_envelope.flags |= ENV_LOOP;
        if (xmsh.vnum > 12) xmsh.vnum = 12;
        if (xmsh.pnum > 12) xmsh.pnum = 12;
        pIns->volume_envelope.num_nodes = xmsh.vnum;
        if (!xmsh.vnum) pIns->volume_envelope.flags &= ~ENV_ENABLED;
        if (!xmsh.pnum) pIns->panning_envelope.flags &= ~ENV_ENABLED;
        pIns->panning_envelope.num_nodes = xmsh.pnum;
        pIns->volume_envelope.sustain_start = pIns->volume_envelope.sustain_end = xmsh.vsustain;
        if (xmsh.vsustain >= 12) pIns->volume_envelope.flags &= ~ENV_SUSTAIN;
        pIns->volume_envelope.loop_start = xmsh.vloops;
        pIns->volume_envelope.loop_end = xmsh.vloope;
        if (pIns->volume_envelope.loop_end >= 12) pIns->volume_envelope.loop_end = 0;
        if (pIns->volume_envelope.loop_start >= pIns->volume_envelope.loop_end) pIns->volume_envelope.flags &= ~ENV_LOOP;
        pIns->panning_envelope.sustain_start = pIns->panning_envelope.sustain_end = xmsh.psustain;
        if (xmsh.psustain >= 12) pIns->panning_envelope.flags &= ~ENV_SUSTAIN;
        pIns->panning_envelope.loop_start = xmsh.ploops;
        pIns->panning_envelope.loop_end = xmsh.ploope;
        if (pIns->panning_envelope.loop_end >= 12) pIns->panning_envelope.loop_end = 0;
        if (pIns->panning_envelope.loop_start >= pIns->panning_envelope.loop_end) pIns->panning_envelope.flags &= ~ENV_LOOP;
        pIns->global_volume = 64;
        for (UINT ienv=0; ienv<12; ienv++)
        {
            pIns->volume_envelope.Ticks[ienv] = (uint16_t)xmsh.venv[ienv*2];
            pIns->volume_envelope.Values[ienv] = (uint8_t)xmsh.venv[ienv*2+1];
            pIns->panning_envelope.Ticks[ienv] = (uint16_t)xmsh.penv[ienv*2];
            pIns->panning_envelope.Values[ienv] = (uint8_t)xmsh.penv[ienv*2+1];
            if (ienv)
            {
                if (pIns->volume_envelope.Ticks[ienv] < pIns->volume_envelope.Ticks[ienv-1])
                {
                    pIns->volume_envelope.Ticks[ienv] &= 0xFF;
                    pIns->volume_envelope.Ticks[ienv] += pIns->volume_envelope.Ticks[ienv-1] & 0xFF00;
                    if (pIns->volume_envelope.Ticks[ienv] < pIns->volume_envelope.Ticks[ienv-1]) pIns->volume_envelope.Ticks[ienv] += 0x100;
                }
                if (pIns->panning_envelope.Ticks[ienv] < pIns->panning_envelope.Ticks[ienv-1])
                {
                    pIns->panning_envelope.Ticks[ienv] &= 0xFF;
                    pIns->panning_envelope.Ticks[ienv] += pIns->panning_envelope.Ticks[ienv-1] & 0xFF00;
                    if (pIns->panning_envelope.Ticks[ienv] < pIns->panning_envelope.Ticks[ienv-1]) pIns->panning_envelope.Ticks[ienv] += 0x100;
                }
            }
        }
        for (UINT j=0; j<96; j++)
        {
            pIns->NoteMap[j+12] = j+1+12;
            if (xmsh.snum[j] < nsamples)
                pIns->Keyboard[j+12] = samplemap[xmsh.snum[j]];
        }
        // Reading samples
        for (UINT ins=0; ins<nsamples; ins++)
        {
            if ((dwMemPos + sizeof(xmss) > dwMemLength)
             || (dwMemPos + xmsh.shsize > dwMemLength)) return true;
            memcpy(&xmss, lpStream + dwMemPos, sizeof(xmss));
            xmss.samplen = LittleEndian(xmss.samplen);
            xmss.loopstart = LittleEndian(xmss.loopstart);
            xmss.looplen = LittleEndian(xmss.looplen);
            dwMemPos += sizeof(XMSAMPLESTRUCT);    // was: dwMemPos += xmsh.shsize; (this fixes IFULOVE.XM)
            flags[ins] = (xmss.type & 0x10) ? RS_PCM16D : RS_PCM8D;
            if (xmss.type & 0x20) flags[ins] = (xmss.type & 0x10) ? RS_STPCM16D : RS_STPCM8D;
            samplesize[ins] = xmss.samplen;
            if (!samplemap[ins]) continue;
            if (xmss.type & 0x10)
            {
                xmss.looplen >>= 1;
                xmss.loopstart >>= 1;
                xmss.samplen >>= 1;
            }
            if (xmss.type & 0x20)
            {
                xmss.looplen >>= 1;
                xmss.loopstart >>= 1;
                xmss.samplen >>= 1;
            }
            if (xmss.samplen > MAX_SAMPLE_LENGTH) xmss.samplen = MAX_SAMPLE_LENGTH;
            if (xmss.loopstart >= xmss.samplen) xmss.type &= ~3;
            xmss.looplen += xmss.loopstart;
            if (xmss.looplen > xmss.samplen) xmss.looplen = xmss.samplen;
            if (!xmss.looplen) xmss.type &= ~3;
            UINT imapsmp = samplemap[ins];
            memcpy(m_szNames[imapsmp], xmss.name, 22);
            SpaceToNullStringFixed<22>(m_szNames[imapsmp]);
            modsample_t *pSmp = &Samples[imapsmp];
            pSmp->length = (xmss.samplen > MAX_SAMPLE_LENGTH) ? MAX_SAMPLE_LENGTH : xmss.samplen;
            pSmp->loop_start = xmss.loopstart;
            pSmp->loop_end = xmss.looplen;
            if (pSmp->loop_end > pSmp->length) pSmp->loop_end = pSmp->length;
            if (pSmp->loop_start >= pSmp->loop_end)
            {
                pSmp->loop_start = pSmp->loop_end = 0;
            }
            if (xmss.type & 3) pSmp->flags |= CHN_LOOP;
            if (xmss.type & 2) pSmp->flags |= CHN_PINGPONGLOOP;
            pSmp->default_volume = xmss.vol << 2;
            if (pSmp->default_volume > 256) pSmp->default_volume = 256;
            pSmp->global_volume = 64;
            if ((xmss.res == 0xAD) && (!(xmss.type & 0x30)))
            {
                flags[ins] = RS_ADPCM4;
                samplesize[ins] = (samplesize[ins]+1)/2 + 16;
            }
            pSmp->nFineTune = xmss.finetune;
            pSmp->RelativeTone = (int)xmss.relnote;
            pSmp->default_pan = xmss.pan;
            pSmp->flags |= CHN_PANNING;
            pSmp->vibrato_type = xmsh.vibtype;
            pSmp->vibrato_sweep = xmsh.vibsweep;
            pSmp->vibrato_depth = xmsh.vibdepth;
            pSmp->vibrato_rate = xmsh.vibrate;
            memcpy(pSmp->legacy_filename, xmss.name, 22);
            SpaceToNullStringFixed<21>(pSmp->legacy_filename);

            if ((xmss.type & 3) == 3)    // MPT 1.09 and maybe newer / older versions set both flags for bidi loops
                bProbablyMPT109 = true;
        }
#if 0
        if ((xmsh.reserved2 > nsamples) && (xmsh.reserved2 <= 16))
        {
            dwMemPos += (((UINT)xmsh.reserved2) - nsamples) * xmsh.shsize;
        }
#endif
        if(xmheader.xmversion >= 0x0104)
        {
            for (UINT ismpd = 0; ismpd < nsamples; ismpd++)
            {
                if ((samplemap[ismpd]) && (samplesize[ismpd]) && (dwMemPos < dwMemLength))
                {
                    ReadSample(&Samples[samplemap[ismpd]], flags[ismpd], (LPSTR)(lpStream + dwMemPos), dwMemLength - dwMemPos);
                }
                dwMemPos += samplesize[ismpd];
                if (dwMemPos >= dwMemLength) break;
            }
        }
    }

    if(xmheader.xmversion < 0x0104)
    {
        // Load Patterns and Samples (Version 1.02 and 1.03)
        if (dwMemPos + 8 >= dwMemLength) return true;
        dwMemPos = ReadXMPatterns(lpStream, dwMemLength, dwMemPos, &xmheader, this);
        if(dwMemPos == 0) return true;

        for(modplug::tracker::sampleindex_t iSmp = 1; iSmp <= m_nSamples; iSmp++)
        {
            ReadSample(&Samples[iSmp], (Samples[iSmp].flags & CHN_16BIT) ? RS_PCM16D : RS_PCM8D, (LPSTR)(lpStream + dwMemPos), dwMemLength - dwMemPos);
            dwMemPos += Samples[iSmp].GetSampleSizeInBytes();
            if (dwMemPos >= dwMemLength) break;
        }
    }

    // Read song comments: "TEXT"
    if ((dwMemPos + 8 < dwMemLength) && (LittleEndian(*((uint32_t *)(lpStream+dwMemPos))) == 0x74786574))
    {
        UINT len = *((uint32_t *)(lpStream+dwMemPos+4));
        dwMemPos += 8;
        if ((dwMemPos + len <= dwMemLength) && (len < 16384))
        {
            ReadMessage(lpStream + dwMemPos, len, leCR);
            dwMemPos += len;
        }
        bMadeWithModPlug = true;
    }
    // Read midi config: "MIDI"
    if ((dwMemPos + 8 < dwMemLength) && (LittleEndian(*((uint32_t *)(lpStream+dwMemPos))) == 0x4944494D))
    {
        UINT len = *((uint32_t *)(lpStream+dwMemPos+4));
        dwMemPos += 8;
        if (len == sizeof(MODMIDICFG))
        {
            memcpy(&m_MidiCfg, lpStream + dwMemPos, len);
            SanitizeMacros();
            m_dwSongFlags |= SONG_EMBEDMIDICFG;
            dwMemPos += len;    //rewbs.fix36946
        }
        bMadeWithModPlug = true;
    }
    // Read pattern names: "PNAM"
    if ((dwMemPos + 8 < dwMemLength) && (LittleEndian(*((uint32_t *)(lpStream+dwMemPos))) == 0x4d414e50))
    {
        UINT len = *((uint32_t *)(lpStream + dwMemPos + 4));
        dwMemPos += 8;
        if ((dwMemPos + len <= dwMemLength) && (len <= MAX_PATTERNS * MAX_PATTERNNAME) && (len >= MAX_PATTERNNAME))
        {
            uint32_t pos = 0;
            modplug::tracker::patternindex_t nPat = 0;
            for(pos = 0; pos < len; pos += MAX_PATTERNNAME, nPat++)
            {
                Patterns[nPat].SetName((char *)(lpStream + dwMemPos + pos), min(MAX_PATTERNNAME, len - pos));
            }
            dwMemPos += len;
        }
        bMadeWithModPlug = true;
    }
    // Read channel names: "CNAM"
    if ((dwMemPos + 8 < dwMemLength) && (LittleEndian(*((uint32_t *)(lpStream+dwMemPos))) == 0x4d414e43))
    {
        UINT len = *((uint32_t *)(lpStream+dwMemPos+4));
        dwMemPos += 8;
        if ((dwMemPos + len <= dwMemLength) && (len <= MAX_BASECHANNELS*MAX_CHANNELNAME))
        {
            UINT n = len / MAX_CHANNELNAME;
            for (UINT i=0; i<n; i++)
            {
                memcpy(ChnSettings[i].szName, (lpStream+dwMemPos+i*MAX_CHANNELNAME), MAX_CHANNELNAME);
                ChnSettings[i].szName[MAX_CHANNELNAME-1] = 0;
            }
            dwMemPos += len;
        }
        bMadeWithModPlug = true;
    }
    // Read mix plugins information
    if (dwMemPos + 8 < dwMemLength)
    {
        uint32_t dwOldPos = dwMemPos;
        dwMemPos += LoadMixPlugins(lpStream+dwMemPos, dwMemLength-dwMemPos);
        if(dwMemPos != dwOldPos)
            bMadeWithModPlug = true;
    }

    // Check various things to find out whether this has been made with MPT.
    // Null chars in names -> most likely made with MPT, which disguises as FT2
    if (!memcmp((LPCSTR)lpStream + 0x26, "FastTracker v2.00   ", 20) && bProbablyMadeWithModPlug && !bIsFT2) bMadeWithModPlug = true;
    if (memcmp((LPCSTR)lpStream + 0x26, "FastTracker v2.00   ", 20)) bMadeWithModPlug = false;    // this could happen f.e. with (early?) versions of Sk@le
    if (!memcmp((LPCSTR)lpStream + 0x26, "FastTracker v 2.00  ", 20))
    {
        // Early MPT 1.0 alpha/beta versions
        bMadeWithModPlug = true;
        m_dwLastSavedWithVersion = MAKE_VERSION_NUMERIC(1, 00, 00, 00);
    }

    if (!memcmp((LPCSTR)lpStream + 0x26, "OpenMPT ", 8))
    {
        CHAR sVersion[13];
        memcpy(sVersion, lpStream + 0x26 + 8, 12);
        sVersion[12] = 0;
        m_dwLastSavedWithVersion = MptVersion::ToNum(sVersion);
    }

    if(bMadeWithModPlug)
    {
        m_nMixLevels = mixLevels_original;
        SetModFlag(MSF_COMPATIBLE_PLAY, false);
        if(!m_dwLastSavedWithVersion)
        {
            if(bProbablyMPT109)
                m_dwLastSavedWithVersion = MAKE_VERSION_NUMERIC(1, 09, 00, 00);
            else
                m_dwLastSavedWithVersion = MAKE_VERSION_NUMERIC(1, 16, 00, 00);
        }
    }

// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"

    // Leave if no extra instrument settings are available (end of file reached)
    if(dwMemPos >= dwMemLength) return true;

    bool bInterpretOpenMPTMade = false; // specific for OpenMPT 1.17+ (bMadeWithModPlug is also for MPT 1.16)
    const uint8_t * ptr = lpStream + dwMemPos;
    if(m_nInstruments)
        ptr = LoadExtendedInstrumentProperties(ptr, lpStream+dwMemLength, &bInterpretOpenMPTMade);

    LoadExtendedSongProperties(GetType(), ptr, lpStream, dwMemLength, &bInterpretOpenMPTMade);

    if(bInterpretOpenMPTMade && m_dwLastSavedWithVersion < MAKE_VERSION_NUMERIC(1, 17, 2, 50))
        SetModFlag(MSF_MIDICC_BUGEMULATION, true);

    if(bInterpretOpenMPTMade && m_dwLastSavedWithVersion == 0)
        m_dwLastSavedWithVersion = MAKE_VERSION_NUMERIC(1, 17, 01, 00);    // early versions of OpenMPT had no version indication.

    return true;
}


#ifndef MODPLUG_NO_FILESAVE
#include "../Moddoc.h"    // for logging errors

bool module_renderer::SaveXM(LPCSTR lpszFileName, UINT nPacking, const bool bCompatibilityExport)
//------------------------------------------------------------------------------------------
{
    #define ASSERT_CAN_WRITE(x) \
    if(len > s.size() - x) /*Buffer running out? Make it larger.*/ \
        s.resize(s.size() + 10*1024, 0); \
    \
    if((len > UINT16_MAX - (UINT)x) && GetpModDoc()) /*Reaching the limits of file format?*/ \
    { \
        CString str; str.Format("%s (%s %u)\n", str_tooMuchPatternData, str_pattern, i); \
        GetpModDoc()->AddToLog(str); \
        break; \
    }

    //uint8_t s[64*64*5];
    vector<uint8_t> s(64*64*5, 0);
    XMFILEHEADER xmheader;
    XMINSTRUMENTHEADER xmih;
    XMSAMPLEHEADER xmsh;
    XMSAMPLESTRUCT xmss;
    uint8_t xmph[9];
    FILE *f;
    int i;
    bool bAddChannel = false; // avoid odd channel count for FT2 compatibility

    if(bCompatibilityExport) nPacking = false;

    if ((!m_nChannels) || (!lpszFileName)) return false;
    if ((f = fopen(lpszFileName, "wb")) == NULL) return false;
    fwrite("Extended Module: ", 17, 1, f);
    char namebuf[20];
    copy_with_padding(namebuf, 20, this->song_name);
    fwrite(namebuf, 20, 1, f);
    s[0] = 0x1A;
    lstrcpy((LPSTR)&s[1], (nPacking) ? "MOD Plugin packed   " : "OpenMPT " MPT_VERSION_STR "  ");
    fwrite(&s[0], 21, 1, f);

    // Writing song header
    memset(&xmheader, 0, sizeof(xmheader));
    xmheader.xmversion = LittleEndianW(0x0104); // XM Format v1.04
    xmheader.size = sizeof(XMFILEHEADER) - 2; // minus the version field
    xmheader.restartpos = LittleEndianW(m_nRestartPos);

    xmheader.channels = (m_nChannels + 1) & 0xFFFE; // avoid odd channel count for FT2 compatibility
    if((m_nChannels & 1) && m_nChannels < MAX_BASECHANNELS) bAddChannel = true;
    if(bCompatibilityExport && xmheader.channels > 32)
        xmheader.channels = 32;
    if(xmheader.channels > MAX_BASECHANNELS) xmheader.channels = MAX_BASECHANNELS;
    xmheader.channels = LittleEndianW(xmheader.channels);

    // Find out number of orders and patterns used.
    // +++ and --- patterns are not taken into consideration as FastTracker does not support them.
    modplug::tracker::orderindex_t nMaxOrds = 0;
    modplug::tracker::patternindex_t nPatterns = 0;
    for(modplug::tracker::orderindex_t nOrd = 0; nOrd < Order.GetLengthTailTrimmed(); nOrd++)
    {
        if(Patterns.IsValidIndex(Order[nOrd]))
        {
            nMaxOrds++;
            if(Order[nOrd] >= nPatterns) nPatterns = Order[nOrd] + 1;
        }
    }
    if(!bCompatibilityExport) nMaxOrds = Order.GetLengthTailTrimmed(); // should really be removed at some point

    xmheader.orders = LittleEndianW((uint16_t)nMaxOrds);
    xmheader.patterns = LittleEndianW((uint16_t)nPatterns);
    xmheader.size = LittleEndian((uint32_t)(xmheader.size + nMaxOrds));

    if(m_nInstruments > 0)
        xmheader.instruments = LittleEndianW(m_nInstruments);
    else
        xmheader.instruments = LittleEndianW(m_nSamples);

    xmheader.flags = (m_dwSongFlags & SONG_LINEARSLIDES) ? 0x01 : 0x00;
    if ((m_dwSongFlags & SONG_EXFILTERRANGE) && !bCompatibilityExport) xmheader.flags |= 0x1000;
    xmheader.flags = LittleEndianW(xmheader.flags);

    if(bCompatibilityExport)
        xmheader.tempo = LittleEndianW(CLAMP(m_nDefaultTempo, 32, 255));
    else
        xmheader.tempo = LittleEndianW(CLAMP(m_nDefaultTempo, 32, 512));
    xmheader.speed = LittleEndianW(CLAMP(m_nDefaultSpeed, 1, 31));

    fwrite(&xmheader, 1, sizeof(xmheader), f);
    // write order list (wihout +++ and ---, explained above)
    for(modplug::tracker::orderindex_t nOrd = 0; nOrd < Order.GetLengthTailTrimmed(); nOrd++)
    {
        if(Patterns.IsValidIndex(Order[nOrd]) || !bCompatibilityExport)
        {
            uint8_t nOrdval = static_cast<uint8_t>(Order[nOrd]);
            fwrite(&nOrdval, 1, sizeof(uint8_t), f);
        }
    }

    // Writing patterns
    for (i = 0; i < nPatterns; i++) if (Patterns[i])
    {
        modplug::tracker::modevent_t *p = Patterns[i];
        UINT len = 0;
        // Empty patterns are always loaded as 64-row patterns in FT2, regardless of their real size...
        bool emptyPatNeedsFixing = (Patterns[i].GetNumRows() != 64);

        memset(&xmph, 0, sizeof(xmph));
        xmph[0] = 9;
        xmph[5] = (uint8_t)(Patterns[i].GetNumRows() & 0xFF);
        xmph[6] = (uint8_t)(Patterns[i].GetNumRows() >> 8);
        for (UINT j = m_nChannels * Patterns[i].GetNumRows(); j > 0; j--, p++)
        {
            // Don't write more than 32 channels
            if(bCompatibilityExport && m_nChannels - ((j - 1) % m_nChannels) > 32) continue;

            UINT note = p->note;
            UINT param = ModSaveCommand(p, true, bCompatibilityExport);
            UINT command = param >> 8;
            param &= 0xFF;
            if (note >= NOTE_MIN_SPECIAL) note = 97; else
            if ((note <= 12) || (note > 96+12)) note = 0; else
            note -= 12;
            UINT vol = 0;
            if (p->volcmd)
            {
                switch(p->volcmd)
                {
                case VolCmdVol:                    vol = 0x10 + p->vol; break;
                case VolCmdSlideDown:    vol = 0x60 + (p->vol & 0x0F); break;
                case VolCmdSlideUp:            vol = 0x70 + (p->vol & 0x0F); break;
                case VolCmdFineDown:    vol = 0x80 + (p->vol & 0x0F); break;
                case VolCmdFineUp:            vol = 0x90 + (p->vol & 0x0F); break;
                case VolCmdVibratoSpeed:    vol = 0xA0 + (p->vol & 0x0F); break;
                case VolCmdVibratoDepth:    vol = 0xB0 + (p->vol & 0x0F); break;
                case VolCmdPan:            vol = 0xC0 + (p->vol >> 2); if (vol > 0xCF) vol = 0xCF; break;
                case VolCmdPanSlideLeft:    vol = 0xD0 + (p->vol & 0x0F); break;
                case VolCmdPanSlideRight:    vol = 0xE0 + (p->vol & 0x0F); break;
                case VolCmdPortamento:    vol = 0xF0 + (p->vol & 0x0F); break;
                }
                // Those values are ignored in FT2. Don't save them, also to avoid possible problems with other trackers (or MPT itself)
                if(bCompatibilityExport && p->vol == 0)
                {
                    switch(p->volcmd)
                    {
                    case VolCmdVol:
                    case VolCmdPan:
                    case VolCmdVibratoDepth:
                    case VolCmdPortamento:
                        break;
                    default:
                        // no memory here.
                        vol = 0;
                    }
                }
            }

            // no need to fix non-empty patterns
            if(!p->IsEmpty())
                emptyPatNeedsFixing = false;

            // apparently, completely empty patterns are loaded as empty 64-row patterns in FT2, regardless of their original size.
            // We have to avoid this, so we add a "break to row 0" command in the last row.
            if(j == 1 && emptyPatNeedsFixing)
            {
                command = 0x0D;
                param = 0;
            }

            if ((note) && (p->instr) && (vol > 0x0F) && (command) && (param))
            {
                s[len++] = note;
                s[len++] = p->instr;
                s[len++] = vol;
                s[len++] = command;
                s[len++] = param;
            } else
            {
                uint8_t b = 0x80;
                if (note) b |= 0x01;
                if (p->instr) b |= 0x02;
                if (vol >= 0x10) b |= 0x04;
                if (command) b |= 0x08;
                if (param) b |= 0x10;
                s[len++] = b;
                if (b & 1) s[len++] = note;
                if (b & 2) s[len++] = p->instr;
                if (b & 4) s[len++] = vol;
                if (b & 8) s[len++] = command;
                if (b & 16) s[len++] = param;
            }

            if(bAddChannel && (j % m_nChannels == 1 || m_nChannels == 1))
            {
                ASSERT_CAN_WRITE(1);
                s[len++] = 0x80;
            }

            ASSERT_CAN_WRITE(5);

        }
        xmph[7] = (uint8_t)(len & 0xFF);
        xmph[8] = (uint8_t)(len >> 8);
        fwrite(xmph, 1, 9, f);
        fwrite(&s[0], 1, len, f);
    } else
    {
        memset(&xmph, 0, sizeof(xmph));
        xmph[0] = 9;
        xmph[5] = (uint8_t)(Patterns[i].GetNumRows() & 0xFF);
        xmph[6] = (uint8_t)(Patterns[i].GetNumRows() >> 8);
        fwrite(xmph, 1, 9, f);
    }
    // Writing instruments
    for (i = 1; i <= xmheader.instruments; i++)
    {
        modsample_t *pSmp;
        uint16_t smptable[32];
        uint8_t flags[32];

        memset(&smptable, 0, sizeof(smptable));
        memset(&xmih, 0, sizeof(xmih));
        memset(&xmsh, 0, sizeof(xmsh));
        xmih.size = LittleEndian(sizeof(xmih) + sizeof(xmsh));
        memcpy(xmih.name, m_szNames[i], 22);
        xmih.type = 0;
        xmih.samples = 0;
        if (m_nInstruments)
        {
            modinstrument_t *pIns = Instruments[i];
            if (pIns)
            {
                memcpy(xmih.name, pIns->name, 22);
                xmih.type = pIns->midi_program;
                xmsh.volfade = LittleEndianW(min(pIns->fadeout, 0x7FFF)); // FFF is maximum in the FT2 GUI, but it can also accept other values. MilkyTracker just allows 0...4095 and 32767 ("cut")
                xmsh.vnum = (uint8_t)pIns->volume_envelope.num_nodes;
                xmsh.pnum = (uint8_t)pIns->panning_envelope.num_nodes;
                if (xmsh.vnum > 12) xmsh.vnum = 12;
                if (xmsh.pnum > 12) xmsh.pnum = 12;
                for (UINT ienv=0; ienv<12; ienv++)
                {
                    xmsh.venv[ienv*2] = LittleEndianW(pIns->volume_envelope.Ticks[ienv]);
                    xmsh.venv[ienv*2+1] = LittleEndianW(pIns->volume_envelope.Values[ienv]);
                    xmsh.penv[ienv*2] = LittleEndianW(pIns->panning_envelope.Ticks[ienv]);
                    xmsh.penv[ienv*2+1] = LittleEndianW(pIns->panning_envelope.Values[ienv]);
                }
                if (pIns->volume_envelope.flags & ENV_ENABLED) xmsh.vtype |= 1;
                if (pIns->volume_envelope.flags & ENV_SUSTAIN) xmsh.vtype |= 2;
                if (pIns->volume_envelope.flags & ENV_LOOP) xmsh.vtype |= 4;
                if (pIns->panning_envelope.flags & ENV_ENABLED) xmsh.ptype |= 1;
                if (pIns->panning_envelope.flags & ENV_SUSTAIN) xmsh.ptype |= 2;
                if (pIns->panning_envelope.flags & ENV_LOOP) xmsh.ptype |= 4;
                xmsh.vsustain = (uint8_t)pIns->volume_envelope.sustain_start;
                xmsh.vloops = (uint8_t)pIns->volume_envelope.loop_start;
                xmsh.vloope = (uint8_t)pIns->volume_envelope.loop_end;
                xmsh.psustain = (uint8_t)pIns->panning_envelope.sustain_start;
                xmsh.ploops = (uint8_t)pIns->panning_envelope.loop_start;
                xmsh.ploope = (uint8_t)pIns->panning_envelope.loop_end;
                for (UINT j=0; j<96; j++) if (pIns->Keyboard[j+12]) // for all notes
                {
                    UINT k;
                    UINT sample = pIns->Keyboard[j+12];

                    // Check to see if sample mapped to this note is already accounted for in this instrument
                    for (k=0; k<xmih.samples; k++)
                    {
                        if (smptable[k] == sample)
                        {
                            break;
                        }
                    }

                    if (k == xmih.samples) //we got to the end of the loop: sample unnaccounted for.
                    {
                        smptable[xmih.samples++] = sample; //record in instrument's sample table
                    }

                    if ((xmih.samples >= 32) || (xmih.samples >= 16 && bCompatibilityExport)) break;
                    xmsh.snum[j] = k;    //record sample table offset in instrument's note map
                }
            }
        } else
        {
            xmih.samples = 1;
            smptable[0] = i;
        }
        xmsh.shsize = LittleEndianW((xmih.samples) ? 40 : 0);
        fwrite(&xmih, 1, sizeof(xmih), f);
        if (smptable[0])
        {
            modsample_t *pvib = &Samples[smptable[0]];
            xmsh.vibtype = pvib->vibrato_type;
            xmsh.vibsweep = min(pvib->vibrato_sweep, 0xFF);
            xmsh.vibdepth = min(pvib->vibrato_depth, 0x0F);
            xmsh.vibrate = min(pvib->vibrato_rate, 0x3F);
        }
        uint16_t samples = xmih.samples;
        xmih.samples = LittleEndianW(xmih.samples);
        fwrite(&xmsh, 1, sizeof(xmsh), f);
        if (!xmih.samples) continue;
        for (UINT ins = 0; ins < samples; ins++)
        {
            memset(&xmss, 0, sizeof(xmss));
            if (smptable[ins]) memcpy(xmss.name, m_szNames[smptable[ins]], 22);
            pSmp = &Samples[smptable[ins]];
            xmss.samplen = pSmp->length;
            xmss.loopstart = pSmp->loop_start;
            xmss.looplen = pSmp->loop_end - pSmp->loop_start;
            xmss.vol = pSmp->default_volume / 4;
            xmss.finetune = (char)pSmp->nFineTune;
            xmss.type = 0;
            if (pSmp->flags & CHN_LOOP) xmss.type = (pSmp->flags & CHN_PINGPONGLOOP) ? 2 : 1;
            flags[ins] = RS_PCM8D;
#ifndef NO_PACKING
            if (nPacking)
            {
                if ((!(pSmp->flags & (CHN_16BIT|CHN_STEREO)))
                 && (CanPackSample(pSmp->sample_data, pSmp->length, nPacking)))
                {
                    flags[ins] = RS_ADPCM4;
                    xmss.res = 0xAD;
                }
            } else
#endif
            {
                if (pSmp->flags & CHN_16BIT)
                {
                    flags[ins] = RS_PCM16D;
                    xmss.type |= 0x10;
                    xmss.looplen *= 2;
                    xmss.loopstart *= 2;
                    xmss.samplen *= 2;
                }
                if (pSmp->flags & CHN_STEREO && !bCompatibilityExport)
                {
                    flags[ins] = (pSmp->flags & CHN_16BIT) ? RS_STPCM16D : RS_STPCM8D;
                    xmss.type |= 0x20;
                    xmss.looplen *= 2;
                    xmss.loopstart *= 2;
                    xmss.samplen *= 2;
                }
            }
            xmss.pan = 255;
            if (pSmp->default_pan < 256) xmss.pan = (uint8_t)pSmp->default_pan;
            xmss.relnote = (signed char)pSmp->RelativeTone;
            xmss.samplen = LittleEndianW(xmss.samplen);
            xmss.loopstart = LittleEndianW(xmss.loopstart);
            xmss.looplen = LittleEndianW(xmss.looplen);
            fwrite(&xmss, 1, xmsh.shsize, f);
        }
        for (UINT ismpd=0; ismpd<xmih.samples; ismpd++)
        {
            pSmp = &Samples[smptable[ismpd]];
            if (pSmp->sample_data)
            {
#ifndef NO_PACKING
                if ((flags[ismpd] == RS_ADPCM4) && (xmih.samples>1)) CanPackSample(pSmp->sample_data, pSmp->length, nPacking);
#endif // NO_PACKING
                WriteSample(f, pSmp, flags[ismpd]);
            }
        }
    }

    if(!bCompatibilityExport)
    {
        // Writing song comments
        if ((m_lpszSongComments) && (m_lpszSongComments[0]))
        {
            uint32_t d = 0x74786574;
            fwrite(&d, 1, 4, f);
            d = strlen(m_lpszSongComments);
            fwrite(&d, 1, 4, f);
            fwrite(m_lpszSongComments, 1, d, f);
        }
        // Writing midi cfg
        if (m_dwSongFlags & SONG_EMBEDMIDICFG)
        {
            uint32_t d = 0x4944494D;
            fwrite(&d, 1, 4, f);
            d = sizeof(MODMIDICFG);
            fwrite(&d, 1, 4, f);
            fwrite(&m_MidiCfg, 1, sizeof(MODMIDICFG), f);
        }
        // Writing Pattern Names
        const modplug::tracker::patternindex_t numNamedPats = Patterns.GetNumNamedPatterns();
        if (numNamedPats > 0)
        {
            uint32_t dwLen = numNamedPats * MAX_PATTERNNAME;
            uint32_t d = 0x4d414e50;
            fwrite(&d, 1, 4, f);
            fwrite(&dwLen, 1, 4, f);

            for(modplug::tracker::patternindex_t nPat = 0; nPat < numNamedPats; nPat++)
            {
                char name[MAX_PATTERNNAME];
                MemsetZero(name);
                Patterns[nPat].GetName(name, MAX_PATTERNNAME);
                fwrite(name, 1, MAX_PATTERNNAME, f);
            }
        }
        // Writing Channel Names
        {
            UINT nChnNames = 0;
            for (UINT inam=0; inam<m_nChannels; inam++)
            {
                if (ChnSettings[inam].szName[0]) nChnNames = inam+1;
            }
            // Do it!
            if (nChnNames)
            {
                uint32_t dwLen = nChnNames * MAX_CHANNELNAME;
                uint32_t d = 0x4d414e43;
                fwrite(&d, 1, 4, f);
                fwrite(&dwLen, 1, 4, f);
                for (UINT inam=0; inam<nChnNames; inam++)
                {
                    fwrite(ChnSettings[inam].szName, 1, MAX_CHANNELNAME, f);
                }
            }
        }

        //Save hacked-on extra info
        SaveMixPlugins(f);
        SaveExtendedInstrumentProperties(Instruments, xmheader.instruments, f);
        SaveExtendedSongProperties(f);
    }

    fclose(f);
    return true;
}

#endif // MODPLUG_NO_FILESAVE