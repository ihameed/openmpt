/*
 * OpenMPT
 *
 * Sampleio.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"
#include "sndfile.h"
#include "it_defs.h"
#include "wavConverter.h"
#ifdef MODPLUG_TRACKER
#include "../mptrack/Moddoc.h"
#endif //MODPLUG_TRACKER
#include "Wav.h"

#pragma warning(disable:4244)

bool CSoundFile::ReadSampleFromFile(SAMPLEINDEX nSample, LPBYTE lpMemFile, DWORD dwFileLength)
//--------------------------------------------------------------------------------------------
{
    if ((!nSample) || (nSample >= MAX_SAMPLES)) return false;
    if ((!ReadWAVSample(nSample, lpMemFile, dwFileLength))
     && (!ReadXISample(nSample, lpMemFile, dwFileLength))
     && (!ReadAIFFSample(nSample, lpMemFile, dwFileLength))
     && (!ReadITSSample(nSample, lpMemFile, dwFileLength))
     && (!ReadPATSample(nSample, lpMemFile, dwFileLength))
     && (!Read8SVXSample(nSample, lpMemFile, dwFileLength))
     && (!ReadS3ISample(nSample, lpMemFile, dwFileLength)))
        return false;
    return true;
}


bool CSoundFile::ReadInstrumentFromFile(INSTRUMENTINDEX nInstr, LPBYTE lpMemFile, DWORD dwFileLength)
//---------------------------------------------------------------------------------------------------
{
    if ((!nInstr) || (nInstr >= MAX_INSTRUMENTS)) return false;
    if ((!ReadXIInstrument(nInstr, lpMemFile, dwFileLength))
     && (!ReadPATInstrument(nInstr, lpMemFile, dwFileLength))
     && (!ReadITIInstrument(nInstr, lpMemFile, dwFileLength))
    // Generic read
     && (!ReadSampleAsInstrument(nInstr, lpMemFile, dwFileLength))) return false;
    if (nInstr > m_nInstruments) m_nInstruments = nInstr;
    return true;
}


bool CSoundFile::ReadSampleAsInstrument(INSTRUMENTINDEX nInstr, LPBYTE lpMemFile, DWORD dwFileLength)
//---------------------------------------------------------------------------------------------------
{
    uint32_t *psig = (uint32_t *)lpMemFile;
    if ((!lpMemFile) || (dwFileLength < 128)) return false;
    if (((psig[0] == LittleEndian(0x46464952)) && (psig[2] == LittleEndian(0x45564157)))    // RIFF....WAVE signature
     || ((psig[0] == LittleEndian(0x5453494C)) && (psig[2] == LittleEndian(0x65766177)))    // LIST....wave
     || (psig[76/4] == LittleEndian(0x53524353))    										// S3I signature
     || ((psig[0] == LittleEndian(0x4D524F46)) && (psig[2] == LittleEndian(0x46464941)))    // AIFF signature
     || ((psig[0] == LittleEndian(0x4D524F46)) && (psig[2] == LittleEndian(0x58565338)))    // 8SVX signature
     || (psig[0] == LittleEndian(LittleEndian(IT_IMPS)))    								// ITS signature
    )
    {
        // Loading Instrument
        modplug::tracker::modinstrument_t *pIns = new modplug::tracker::modinstrument_t;
        if (!pIns) return false;
        memset(pIns, 0, sizeof(modplug::tracker::modinstrument_t));
        pIns->pTuning = pIns->s_DefaultTuning;
// -> CODE#0003
// -> DESC="remove instrument's samples"
//    	RemoveInstrumentSamples(nInstr);
        DestroyInstrument(nInstr,1);
// -! BEHAVIOUR_CHANGE#0003
        Instruments[nInstr] = pIns;
        // Scanning free sample
        UINT nSample = 0;
        for (UINT iscan=1; iscan<MAX_SAMPLES; iscan++)
        {
            if ((!Samples[iscan].sample_data) && (!m_szNames[iscan][0]))
            {
                nSample = iscan;
                if (nSample > m_nSamples) m_nSamples = nSample;
                break;
            }
        }
        // Default values
        pIns->fadeout = 1024;
        pIns->global_volume = 64;
        pIns->default_pan = 128;
        pIns->pitch_pan_center = 5*12;
        SetDefaultInstrumentValues(pIns);
        for (UINT iinit=0; iinit<128; iinit++)
        {
            pIns->Keyboard[iinit] = nSample;
            pIns->NoteMap[iinit] = iinit+1;
        }
        if (nSample) ReadSampleFromFile(nSample, lpMemFile, dwFileLength);
        return true;
    }
    return false;
}


// -> CODE#0003
// -> DESC="remove instrument's samples"
// BOOL CSoundFile::DestroyInstrument(UINT nInstr)
bool CSoundFile::DestroyInstrument(INSTRUMENTINDEX nInstr, char removeSamples)
//----------------------------------------------------------------------------
{
    if ((!nInstr) || (nInstr > m_nInstruments)) return false;
    if (!Instruments[nInstr]) return true;

// -> CODE#0003
// -> DESC="remove instrument's samples"
    //rewbs: changed message
    if(removeSamples > 0 || (removeSamples == 0 && ::MessageBox(NULL, "Remove samples associated with an instrument if they are unused?", "Removing instrument", MB_YESNO | MB_ICONQUESTION) == IDYES))
    {
        RemoveInstrumentSamples(nInstr);
    }
// -! BEHAVIOUR_CHANGE#0003

// -> CODE#0023
// -> DESC="IT project files (.itp)"
    m_szInstrumentPath[nInstr - 1][0] = '\0';
#ifdef MODPLUG_TRACKER
    if(GetpModDoc())
    {
        GetpModDoc()->m_bsInstrumentModified.reset(nInstr - 1);
    }
#endif // MODPLUG_TRACKER
// -! NEW_FEATURE#0023

    modplug::tracker::modinstrument_t *pIns = Instruments[nInstr];
    Instruments[nInstr] = nullptr;
    for(CHANNELINDEX i = 0; i < MAX_CHANNELS; i++)
    {
        if (Chn[i].instrument == pIns)
        {
            Chn[i].instrument = nullptr;
        }
    }
    delete pIns;
    return true;
}


// Removing all unused samples
bool CSoundFile::RemoveInstrumentSamples(INSTRUMENTINDEX nInstr)
//--------------------------------------------------------------
{
    vector<bool> sampleused(GetNumSamples() + 1, false);

    if (Instruments[nInstr])
    {
        modplug::tracker::modinstrument_t *p = Instruments[nInstr];
        for (UINT r=0; r<128; r++)
        {
            UINT n = p->Keyboard[r];
            if (n <= GetNumSamples()) sampleused[n] = true;
        }
        for (INSTRUMENTINDEX nIns = 1; nIns < MAX_INSTRUMENTS; nIns++) if ((Instruments[nIns]) && (nIns != nInstr))
        {
            p = Instruments[nIns];
            for (UINT r=0; r<128; r++)
            {
                UINT n = p->Keyboard[r];
                if (n <= GetNumSamples()) sampleused[n] = false;
            }
        }
        for (SAMPLEINDEX nSmp = 1; nSmp <= GetNumSamples(); nSmp++) if (sampleused[nSmp])
        {
            DestroySample(nSmp);
            strcpy(m_szNames[nSmp], "");
        }
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//
// I/O From another song
//

bool CSoundFile::ReadInstrumentFromSong(INSTRUMENTINDEX nInstr, CSoundFile *pSrcSong, UINT nSrcInstr)
//---------------------------------------------------------------------------------------------------
{
    if ((!pSrcSong) || (!nSrcInstr) || (nSrcInstr > pSrcSong->m_nInstruments)
     || (nInstr >= MAX_INSTRUMENTS) || (!pSrcSong->Instruments[nSrcInstr])) return false;
    if (m_nInstruments < nInstr) m_nInstruments = nInstr;
// -> CODE#0003
// -> DESC="remove instrument's samples"
//    RemoveInstrumentSamples(nInstr);
    DestroyInstrument(nInstr, 1);
// -! BEHAVIOUR_CHANGE#0003
    if (!Instruments[nInstr]) Instruments[nInstr] = new modplug::tracker::modinstrument_t;
    modplug::tracker::modinstrument_t *pIns = Instruments[nInstr];
    if (pIns)
    {
        uint16_t samplemap[32];
        uint16_t samplesrc[32];
        UINT nSamples = 0;
        UINT nsmp = 1;
        *pIns = *pSrcSong->Instruments[nSrcInstr];
        for (UINT i=0; i<128; i++)
        {
            UINT n = pIns->Keyboard[i];
            if ((n) && (n <= pSrcSong->m_nSamples) && (i < NOTE_MAX))
            {
                UINT j = 0;
                for (j=0; j<nSamples; j++)
                {
                    if (samplesrc[j] == n) break;
                }
                if (j >= nSamples)
                {
                    while ((nsmp < MAX_SAMPLES) && ((Samples[nsmp].sample_data) || (m_szNames[nsmp][0]))) nsmp++;
                    if ((nSamples < 32) && (nsmp < MAX_SAMPLES))
                    {
                        samplesrc[nSamples] = (uint16_t)n;
                        samplemap[nSamples] = (uint16_t)nsmp;
                        nSamples++;
                        pIns->Keyboard[i] = (uint16_t)nsmp;
                        if (m_nSamples < nsmp) m_nSamples = nsmp;
                        nsmp++;
                    } else
                    {
                        pIns->Keyboard[i] = 0;
                    }
                } else
                {
                    pIns->Keyboard[i] = samplemap[j];
                }
            } else
            {
                pIns->Keyboard[i] = 0;
            }
        }
        // Load Samples
        for (UINT k=0; k<nSamples; k++)
        {
            ReadSampleFromSong(samplemap[k], pSrcSong, samplesrc[k]);
        }
        return true;
    }
    return false;
}


bool CSoundFile::ReadSampleFromSong(SAMPLEINDEX nSample, CSoundFile *pSrcSong, UINT nSrcSample)
//---------------------------------------------------------------------------------------------
{
    if ((!pSrcSong) || (!nSrcSample) || (nSrcSample > pSrcSong->m_nSamples) || (nSample >= MAX_SAMPLES)) return false;
    modplug::tracker::modsample_t *psmp = &pSrcSong->Samples[nSrcSample];
    UINT nSize = psmp->length;
    if (psmp->flags & CHN_16BIT) nSize *= 2;
    if (psmp->flags & CHN_STEREO) nSize *= 2;
    if (m_nSamples < nSample) m_nSamples = nSample;
    if (Samples[nSample].sample_data)
    {
        Samples[nSample].length = 0;
        FreeSample(Samples[nSample].sample_data);
    }
    Samples[nSample] = *psmp;
    if (psmp->sample_data)
    {
        Samples[nSample].sample_data = AllocateSample(nSize+8);
        if (Samples[nSample].sample_data)
        {
            memcpy(Samples[nSample].sample_data, psmp->sample_data, nSize);
            AdjustSampleLoop(&Samples[nSample]);
        }
    }
    if ((!(m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM))) && (pSrcSong->m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM)))
    {
        modplug::tracker::modsample_t *pSmp = &Samples[nSample];
        pSmp->c5_samplerate = TransposeToFrequency(pSmp->RelativeTone, pSmp->nFineTune);
        pSmp->RelativeTone = 0;
        pSmp->nFineTune = 0;
    } else
    if ((m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM)) && (!(pSrcSong->m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM))))
    {
        FrequencyToTranspose(&Samples[nSample]);
    }
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// WAV Open

#define IFFID_pcm    0x206d6370
#define IFFID_fact    0x74636166

extern BOOL IMAADPCMUnpack16(signed short *pdest, UINT nLen, LPBYTE psrc, DWORD dwBytes, UINT pkBlkAlign);

bool CSoundFile::ReadWAVSample(SAMPLEINDEX nSample, LPBYTE lpMemFile, DWORD dwFileLength, DWORD *pdwWSMPOffset)
//-------------------------------------------------------------------------------------------------------------
{
    DWORD dwMemPos = 0, dwDataPos;
    WAVEFILEHEADER *phdr = (WAVEFILEHEADER *)lpMemFile;
    WAVEFORMATHEADER *pfmt, *pfmtpk;
    WAVEDATAHEADER *pdata;
    WAVESMPLHEADER *psh;
    WAVEEXTRAHEADER *pxh;
    DWORD dwInfoList, dwFact, dwSamplesPerBlock;
    
    if ((!nSample) || (!lpMemFile) || (dwFileLength < (DWORD)(sizeof(WAVEFILEHEADER)+sizeof(WAVEFORMATHEADER)))) return false;
    if (((phdr->id_RIFF != IFFID_RIFF) && (phdr->id_RIFF != IFFID_LIST))
     || ((phdr->id_WAVE != IFFID_WAVE) && (phdr->id_WAVE != IFFID_wave))) return false;
    dwMemPos = sizeof(WAVEFILEHEADER);
    dwDataPos = 0;
    pfmt = NULL;
    pfmtpk = NULL;
    pdata = NULL;
    psh = NULL;
    pxh = NULL;
    dwSamplesPerBlock = 0;
    dwInfoList = 0;
    dwFact = 0;
    while ((dwMemPos + 8 < dwFileLength) && (dwMemPos < phdr->filesize))
    {
        DWORD dwLen = *((LPDWORD)(lpMemFile+dwMemPos+4));
        DWORD dwIFFID = *((LPDWORD)(lpMemFile+dwMemPos));
        if ((dwLen > dwFileLength) || (dwMemPos+8+dwLen > dwFileLength)) break;
        switch(dwIFFID)
        {
        // "fmt "
        case IFFID_fmt:
            if (pfmt) break;
            if (dwLen+8 >= sizeof(WAVEFORMATHEADER))
            {
                pfmt = (WAVEFORMATHEADER *)(lpMemFile + dwMemPos);
                if (dwLen+8 >= sizeof(WAVEFORMATHEADER)+4)
                {
                    dwSamplesPerBlock = *((uint16_t *)(lpMemFile+dwMemPos+sizeof(WAVEFORMATHEADER)+2));
                }
            }
            break;
        // "pcm "
        case IFFID_pcm:
            if (pfmtpk) break;
            if (dwLen+8 >= sizeof(WAVEFORMATHEADER)) pfmtpk = (WAVEFORMATHEADER *)(lpMemFile + dwMemPos);
            break;
        // "fact"
        case IFFID_fact:
            if (!dwFact) dwFact = *((LPDWORD)(lpMemFile+dwMemPos+8));
            break;
        // "data"
        case IFFID_data:
            if ((dwLen+8 >= sizeof(WAVEDATAHEADER)) && (!pdata))
            {
                pdata = (WAVEDATAHEADER *)(lpMemFile + dwMemPos);
                dwDataPos = dwMemPos + 8;
            }
            break;
        // "xtra"
        case 0x61727478:
            if (dwLen+8 >= sizeof(WAVEEXTRAHEADER)) pxh = (WAVEEXTRAHEADER *)(lpMemFile + dwMemPos);
            break;
        // "smpl"
        case 0x6C706D73:
            if (dwLen+8 >= sizeof(WAVESMPLHEADER)) psh = (WAVESMPLHEADER *)(lpMemFile + dwMemPos);
            break;
        // "LIST"."info"
        case IFFID_LIST:
            if (*((LPDWORD)(lpMemFile+dwMemPos+8)) == 0x4F464E49)    // "INFO"
                dwInfoList = dwMemPos;
            break;
        // "wsmp":
        case IFFID_wsmp:
            if (pdwWSMPOffset) *pdwWSMPOffset = dwMemPos;
            break;
        }
        dwMemPos += dwLen + 8;
    }
    if ((!pdata) || (!pfmt) || (pdata->length < 4)) return false;
    if ((pfmtpk) && (pfmt))
    {
        if (pfmt->format != 1)
        {
            WAVEFORMATHEADER *tmp = pfmt;
            pfmt = pfmtpk;
            pfmtpk = tmp;
            if ((pfmtpk->format != 0x11) || (pfmtpk->bitspersample != 4)
             || (pfmtpk->channels != 1)) return false;
        } else pfmtpk = NULL;
    }
    // WAVE_FORMAT_PCM, WAVE_FORMAT_IEEE_FLOAT, WAVE_FORMAT_EXTENSIBLE
    if ((((pfmt->format != 1) && (pfmt->format != 0xFFFE))
     && (pfmt->format != 3 || pfmt->bitspersample != 32)) //Microsoft IEEE FLOAT
     || (pfmt->channels > 2)
     || (!pfmt->channels)
     || (pfmt->bitspersample & 7)
     || (!pfmt->bitspersample)
     || (pfmt->bitspersample > 32)
    ) return false;

    DestroySample(nSample);
    UINT nType = RS_PCM8U;
    if (pfmt->channels == 1)
    {
        if (pfmt->bitspersample == 24) nType = RS_PCM24S; 
        else if (pfmt->bitspersample == 32) nType = RS_PCM32S; 
            else nType = (pfmt->bitspersample == 16) ? RS_PCM16S : RS_PCM8U;
    } else
    {
        if (pfmt->bitspersample == 24) nType = RS_STIPCM24S; 
        else if (pfmt->bitspersample == 32) nType = RS_STIPCM32S; 
            else nType = (pfmt->bitspersample == 16) ? RS_STIPCM16S : RS_STIPCM8U;
    }
    UINT samplesize = pfmt->channels * (pfmt->bitspersample >> 3);
    modplug::tracker::modsample_t *pSmp = &Samples[nSample];
    if (pSmp->sample_data)
    {
        FreeSample(pSmp->sample_data);
        pSmp->sample_data = nullptr;
        pSmp->length = 0;
    }
    pSmp->length = pdata->length / samplesize;
    pSmp->loop_start = pSmp->loop_end = 0;
    pSmp->sustain_start = pSmp->sustain_end = 0;
    pSmp->c5_samplerate = pfmt->freqHz;
    pSmp->default_pan = 128;
    pSmp->default_volume = 256;
    pSmp->global_volume = 64;
    pSmp->flags = (pfmt->bitspersample > 8) ? CHN_16BIT : 0;
    if (m_nType & MOD_TYPE_XM) pSmp->flags |= CHN_PANNING;
    pSmp->RelativeTone = 0;
    pSmp->nFineTune = 0;
    if (m_nType & MOD_TYPE_XM) FrequencyToTranspose(pSmp);
    pSmp->vibrato_type = pSmp->vibrato_sweep = pSmp->vibrato_depth = pSmp->vibrato_rate = 0;
    pSmp->legacy_filename[0] = 0;
    memset(m_szNames[nSample], 0, 32);
    if (pSmp->length > MAX_SAMPLE_LENGTH) pSmp->length = MAX_SAMPLE_LENGTH;
    // IMA ADPCM 4:1
    if (pfmtpk)
    {
        if (dwFact < 4) dwFact = pdata->length * 2;
        pSmp->length = dwFact;
        pSmp->sample_data = AllocateSample(pSmp->length*2+16);
        IMAADPCMUnpack16((signed short *)pSmp->sample_data, pSmp->length,
                         (LPBYTE)(lpMemFile+dwDataPos), dwFileLength-dwDataPos, pfmtpk->samplesize);
        AdjustSampleLoop(pSmp);
    } else
    {
        ReadSample(pSmp, nType, (LPSTR)(lpMemFile+dwDataPos), dwFileLength-dwDataPos, pfmt->format);
    }
    // smpl field
    if (psh)
    {
        pSmp->loop_start = pSmp->loop_end = 0;
        if ((psh->dwSampleLoops) && (sizeof(WAVESMPLHEADER) + psh->dwSampleLoops * sizeof(SAMPLELOOPSTRUCT) <= psh->smpl_len + 8))
        {
            SAMPLELOOPSTRUCT *psl = (SAMPLELOOPSTRUCT *)(&psh[1]);
            if (psh->dwSampleLoops > 1)
            {
                pSmp->flags |= CHN_LOOP | CHN_SUSTAINLOOP;
                if (psl[0].dwLoopType) pSmp->flags |= CHN_PINGPONGSUSTAIN;
                if (psl[1].dwLoopType) pSmp->flags |= CHN_PINGPONGLOOP;
                pSmp->sustain_start = psl[0].dwLoopStart;
                pSmp->sustain_end = psl[0].dwLoopEnd;
                pSmp->loop_start = psl[1].dwLoopStart;
                pSmp->loop_end = psl[1].dwLoopEnd;
            } else
            {
                pSmp->flags |= CHN_LOOP;
                if (psl->dwLoopType) pSmp->flags |= CHN_PINGPONGLOOP;
                pSmp->loop_start = psl->dwLoopStart;
                pSmp->loop_end = psl->dwLoopEnd;
            }
            if (pSmp->loop_start >= pSmp->loop_end) pSmp->flags &= ~(CHN_LOOP|CHN_PINGPONGLOOP);
            if (pSmp->sustain_start >= pSmp->sustain_end) pSmp->flags &= ~(CHN_PINGPONGLOOP|CHN_PINGPONGSUSTAIN);
        }
    }
    // LIST field
    if (dwInfoList)
    {
        DWORD dwLSize = *((DWORD *)(lpMemFile+dwInfoList+4)) + 8;
        DWORD d = 12;
        while (d+8 < dwLSize)
        {
            if (!lpMemFile[dwInfoList+d]) d++;
            DWORD id = *((DWORD *)(lpMemFile+dwInfoList+d));
            DWORD len = *((DWORD *)(lpMemFile+dwInfoList+d+4));
            if (id == 0x4D414E49) // "INAM"
            {
                if ((dwInfoList+d+8+len <= dwFileLength) && (len))
                {
                    DWORD dwNameLen = len;
                    if (dwNameLen > 31) dwNameLen = 31;
                    memcpy(m_szNames[nSample], lpMemFile+dwInfoList+d+8, dwNameLen);
                    if (phdr->id_RIFF != 0x46464952)
                    {
                        // DLS sample -> sample legacy_filename
                        if (dwNameLen > 21) dwNameLen = 21;
                        memcpy(pSmp->legacy_filename, lpMemFile+dwInfoList+d+8, dwNameLen);
                    }
                }
                break;
            }
            d += 8 + len;
        }
    }
    // xtra field
    if (pxh)
    {
        if (!(GetType() & (MOD_TYPE_MOD|MOD_TYPE_S3M)))
        {
            if (pxh->dwFlags & CHN_PINGPONGLOOP) pSmp->flags |= CHN_PINGPONGLOOP;
            if (pxh->dwFlags & CHN_SUSTAINLOOP) pSmp->flags |= CHN_SUSTAINLOOP;
            if (pxh->dwFlags & CHN_PINGPONGSUSTAIN) pSmp->flags |= CHN_PINGPONGSUSTAIN;
            if (pxh->dwFlags & CHN_PANNING) pSmp->flags |= CHN_PANNING;
        }
        pSmp->default_pan = pxh->wPan;
        pSmp->default_volume = pxh->wVolume;
        pSmp->global_volume = pxh->wGlobalVol;
        pSmp->vibrato_type = pxh->nVibType;
        pSmp->vibrato_sweep = pxh->nVibSweep;
        pSmp->vibrato_depth = pxh->nVibDepth;
        pSmp->vibrato_rate = pxh->nVibRate;
        // Name present (clipboard only)
        UINT xtrabytes = pxh->xtra_len + 8 - sizeof(WAVEEXTRAHEADER);
        LPSTR pszTextEx = (LPSTR)(pxh+1); 
        if (xtrabytes >= MAX_SAMPLENAME)
        {
            memcpy(m_szNames[nSample], pszTextEx, MAX_SAMPLENAME - 1);
            pszTextEx += MAX_SAMPLENAME;
            xtrabytes -= MAX_SAMPLENAME;
            if (xtrabytes >= MAX_SAMPLEFILENAME)
            {
                memcpy(pSmp->legacy_filename, pszTextEx, MAX_SAMPLEFILENAME);
                xtrabytes -= MAX_SAMPLEFILENAME;
            }
        }
    }
    return true;
}


///////////////////////////////////////////////////////////////
// Save WAV

bool CSoundFile::SaveWAVSample(UINT nSample, LPCSTR lpszFileName)
//---------------------------------------------------------------
{
    LPCSTR lpszMPT = "Modplug Tracker\0";
    WAVEFILEHEADER header;
    WAVEFORMATHEADER format;
    WAVEDATAHEADER data;
    WAVESAMPLERINFO smpl;
    WAVELISTHEADER list;
    WAVEEXTRAHEADER extra;
    modplug::tracker::modsample_t *pSmp = &Samples[nSample];
    FILE *f;

    if ((f = fopen(lpszFileName, "wb")) == NULL) return false;
    memset(&extra, 0, sizeof(extra));
    memset(&smpl, 0, sizeof(smpl));
    header.id_RIFF = IFFID_RIFF;
    header.filesize = sizeof(header)+sizeof(format)+sizeof(data)+sizeof(extra)-8;
    header.filesize += 12+8+16+8+32; // LIST(INAM, ISFT)
    header.id_WAVE = IFFID_WAVE;
    format.id_fmt = IFFID_fmt;
    format.hdrlen = 16;
    format.format = 1;
    format.freqHz = pSmp->c5_samplerate;
    if (m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM)) format.freqHz = TransposeToFrequency(pSmp->RelativeTone, pSmp->nFineTune);
    format.channels = pSmp->GetNumChannels();
    format.bitspersample = pSmp->GetElementarySampleSize() * 8;
    format.samplesize = pSmp->GetBytesPerSample() * 8;
    format.bytessec = format.freqHz*format.samplesize;
    data.id_data = IFFID_data;
    UINT nType;
    data.length = pSmp->GetSampleSizeInBytes();
    if (pSmp->flags & CHN_STEREO)
    {
        nType = (pSmp->flags & CHN_16BIT) ? RS_STIPCM16S : RS_STIPCM8U;
    } else
    {
        nType = (pSmp->flags & CHN_16BIT) ? RS_PCM16S : RS_PCM8U;
    }
    header.filesize += data.length;
    fwrite(&header, 1, sizeof(header), f);
    fwrite(&format, 1, sizeof(format), f);
    fwrite(&data, 1, sizeof(data), f);
    WriteSample(f, pSmp, nType);
    // "smpl" field
    smpl.wsiHdr.smpl_id = 0x6C706D73;
    smpl.wsiHdr.smpl_len = sizeof(WAVESMPLHEADER) - 8;
    smpl.wsiHdr.dwSamplePeriod = 22675;
    if (pSmp->c5_samplerate >= 256) smpl.wsiHdr.dwSamplePeriod = 1000000000 / pSmp->c5_samplerate;
    smpl.wsiHdr.dwBaseNote = 60;
    if (pSmp->flags & (CHN_LOOP|CHN_SUSTAINLOOP))
    {
        if (pSmp->flags & CHN_SUSTAINLOOP)
        {
            smpl.wsiHdr.dwSampleLoops = 2;
            smpl.wsiLoops[0].dwLoopType = (pSmp->flags & CHN_PINGPONGSUSTAIN) ? 1 : 0;
            smpl.wsiLoops[0].dwLoopStart = pSmp->sustain_start;
            smpl.wsiLoops[0].dwLoopEnd = pSmp->sustain_end;
            smpl.wsiLoops[1].dwLoopType = (pSmp->flags & CHN_PINGPONGLOOP) ? 1 : 0;
            smpl.wsiLoops[1].dwLoopStart = pSmp->loop_start;
            smpl.wsiLoops[1].dwLoopEnd = pSmp->loop_end;
        } else
        {
            smpl.wsiHdr.dwSampleLoops = 1;
            smpl.wsiLoops[0].dwLoopType = (pSmp->flags & CHN_PINGPONGLOOP) ? 1 : 0;
            smpl.wsiLoops[0].dwLoopStart = pSmp->loop_start;
            smpl.wsiLoops[0].dwLoopEnd = pSmp->loop_end;
        }
        smpl.wsiHdr.smpl_len += sizeof(SAMPLELOOPSTRUCT) * smpl.wsiHdr.dwSampleLoops;
    }
    fwrite(&smpl, 1, smpl.wsiHdr.smpl_len + 8, f);
    // "LIST" field
    list.list_id = IFFID_LIST;
    list.list_len = sizeof(list) - 8    // LIST
                    + 8 + 32    		// "INAM".dwLen.szSampleName
                    + 8 + 16;    		// "ISFT".dwLen."ModPlug Tracker".0
    list.info = IFFID_INFO;
    fwrite(&list, 1, sizeof(list), f);
    list.list_id = IFFID_INAM;    		// "INAM"
    list.list_len = 32;
    fwrite(&list, 1, 8, f);
    fwrite(m_szNames[nSample], 1, 32, f);
    list.list_id = IFFID_ISFT;    		// "ISFT"
    list.list_len = 16;
    fwrite(&list, 1, 8, f);
    fwrite(lpszMPT, 1, list.list_len, f);
    // "xtra" field
    extra.xtra_id = IFFID_xtra;
    extra.xtra_len = sizeof(extra) - 8;
    extra.dwFlags = pSmp->flags;
    extra.wPan = pSmp->default_pan;
    extra.wVolume = pSmp->default_volume;
    extra.wGlobalVol = pSmp->global_volume;
    extra.wReserved = 0;
    extra.nVibType = pSmp->vibrato_type;
    extra.nVibSweep = pSmp->vibrato_sweep;
    extra.nVibDepth = pSmp->vibrato_depth;
    extra.nVibRate = pSmp->vibrato_rate;
    fwrite(&extra, 1, sizeof(extra), f);
    fclose(f);
    return true;
}

///////////////////////////////////////////////////////////////
// Save RAW

bool CSoundFile::SaveRAWSample(UINT nSample, LPCSTR lpszFileName)
//---------------------------------------------------------------
{
    modplug::tracker::modsample_t *pSmp = &Samples[nSample];
    FILE *f;

    if ((f = fopen(lpszFileName, "wb")) == NULL) return false;

    UINT nType;
    if (pSmp->flags & CHN_STEREO)
        nType = (pSmp->flags & CHN_16BIT) ? RS_STIPCM16S : RS_STIPCM8S;
    else
        nType = (pSmp->flags & CHN_16BIT) ? RS_PCM16S : RS_PCM8S;
    WriteSample(f, pSmp, nType);
    fclose(f);
    return true;
}

/////////////////////////////////////////////////////////////
// GUS Patches

#pragma pack(1)

typedef struct GF1PATCHFILEHEADER
{
    DWORD gf1p;    			// "GF1P"
    DWORD atch;    			// "ATCH"
    CHAR version[4];    	// "100", or "110"
    CHAR id[10];    		// "ID#000002"
    CHAR copyright[60];    	// Copyright
    uint8_t instrum;    		// Number of instruments in patch
    uint8_t voices;    		// Number of voices, usually 14
    uint8_t channels;    		// Number of wav channels that can be played concurently to the patch
    uint16_t samples;    		// Total number of waveforms for all the .PAT
    uint16_t volume;    		// Master volume
    DWORD data_size;
    uint8_t reserved2[36];
} GF1PATCHFILEHEADER;


typedef struct GF1INSTRUMENT
{
    uint16_t id;    			// Instrument id: 0-65535
    CHAR name[16];    		// Name of instrument. Gravis doesn't seem to use it
    DWORD size;    			// Number of bytes for the instrument with header. (To skip to next instrument)
    uint8_t layers;    		// Number of layers in instrument: 1-4
    uint8_t reserved[40];
} GF1INSTRUMENT;


typedef struct GF1SAMPLEHEADER
{
    CHAR name[7];    		// null terminated string. name of the wave.
    uint8_t fractions;    		// Start loop point fraction in 4 bits + End loop point fraction in the 4 other bits.
    DWORD length;    		// total size of wavesample. limited to 65535 now by the drivers, not the card.
    DWORD loopstart;    	// start loop position in the wavesample
    DWORD loopend;    		// end loop position in the wavesample
    uint16_t freq;    			// Rate at which the wavesample has been sampled
    DWORD low_freq, high_freq, root_freq;    // check note.h for the correspondance.
    SHORT finetune;    		// fine tune. -512 to +512, EXCLUDING 0 cause it is a multiplier. 512 is one octave off, and 1 is a neutral value
    uint8_t balance;    		// Balance: 0-15. 0=full left, 15 = full right
    uint8_t env_rate[6];    	// attack rates
    uint8_t env_volume[6];    	// attack volumes
    uint8_t tremolo_sweep, tremolo_rate, tremolo_depth;
    uint8_t vibrato_sweep, vibrato_rate, vibrato_depth;
    uint8_t flags;
    SHORT scale_frequency;
    uint16_t scale_factor;
    uint8_t reserved[36];
} GF1SAMPLEHEADER;

// -- GF1 Envelopes --
//
//    It can be represented like this (the enveloppe is totally bogus, it is
//    just to show the concept):
//    					  
//    |                               
//    |           /----`               | |
//    |   /------/      `\         | | | | |
//    |  /                 \       | | | | |
//    | /                    \     | | | | |
//    |/                       \   | | | | |
//    ---------------------------- | | | | | |
//    <---> attack rate 0          0 1 2 3 4 5 amplitudes
//         <----> attack rate 1
//    	     <> attack rate 2
//    		 <--> attack rate 3
//    		     <> attack rate 4
//    			 <-----> attack rate 5
//
// -- GF1 Flags --
//
// bit 0: 8/16 bit
// bit 1: Signed/Unsigned
// bit 2: off/on looping
// bit 3: off/on bidirectionnal looping
// bit 4: off/on backward looping
// bit 5: off/on sustaining (3rd point in env.)
// bit 6: off/on enveloppes
// bit 7: off/on clamped release (6th point, env)

typedef struct GF1LAYER
{
    uint8_t previous;    		// If !=0 the wavesample to use is from the previous layer. The waveheader is still needed
    uint8_t id;    			// Layer id: 0-3
    DWORD size;    			// data size in bytes in the layer, without the header. to skip to next layer for example:
    uint8_t samples;    		// number of wavesamples
    uint8_t reserved[40];
} GF1LAYER;

#pragma pack()

// returns 12*Log2(nFreq/2044)
LONG PatchFreqToNote(ULONG nFreq)
//-------------------------------
{
    const float k_base = 1.0f / 2044.0f;
    const float k_12 = 12;
    LONG result;
    if (nFreq < 1) return 0;
    _asm {
    fld k_12
    fild nFreq
    fld k_base
    fmulp ST(1), ST(0)
    fyl2x
    fistp result
    }
    return result;
}


void PatchToSample(CSoundFile *that, UINT nSample, LPBYTE lpStream, DWORD dwMemLength)
//------------------------------------------------------------------------------------
{
    modplug::tracker::modsample_t *pIns = &that->Samples[nSample];
    DWORD dwMemPos = sizeof(GF1SAMPLEHEADER);
    GF1SAMPLEHEADER *psh = (GF1SAMPLEHEADER *)(lpStream);
    UINT nSmpType;
    
    if (dwMemLength < sizeof(GF1SAMPLEHEADER)) return;
    if (psh->name[0])
    {
        memcpy(that->m_szNames[nSample], psh->name, 7);
        that->m_szNames[nSample][7] = 0;
    }
    pIns->legacy_filename[0] = 0;
    pIns->global_volume = 64;
    pIns->flags = (psh->flags & 1) ? CHN_16BIT : 0;
    if (psh->flags & 4) pIns->flags |= CHN_LOOP;
    if (psh->flags & 8) pIns->flags |= CHN_PINGPONGLOOP;
    pIns->length = psh->length;
    pIns->loop_start = psh->loopstart;
    pIns->loop_end = psh->loopend;
    pIns->c5_samplerate = psh->freq;
    pIns->RelativeTone = 0;
    pIns->nFineTune = 0;
    pIns->default_volume = 256;
    pIns->default_pan = (psh->balance << 4) + 8;
    if (pIns->default_pan > 256) pIns->default_pan = 128;
    pIns->vibrato_type = 0;
    pIns->vibrato_sweep = psh->vibrato_sweep;
    pIns->vibrato_depth = psh->vibrato_depth;
    pIns->vibrato_rate = psh->vibrato_rate/4;
    that->FrequencyToTranspose(pIns);
    pIns->RelativeTone += 84 - PatchFreqToNote(psh->root_freq);
    if (psh->scale_factor) pIns->RelativeTone -= psh->scale_frequency - 60;
    pIns->c5_samplerate = that->TransposeToFrequency(pIns->RelativeTone, pIns->nFineTune);
    if (pIns->flags & CHN_16BIT)
    {
        nSmpType = (psh->flags & 2) ? RS_PCM16U : RS_PCM16S;
        pIns->length >>= 1;
        pIns->loop_start >>= 1;
        pIns->loop_end >>= 1;
    } else
    {
        nSmpType = (psh->flags & 2) ? RS_PCM8U : RS_PCM8S;
    }
    that->ReadSample(pIns, nSmpType, (LPSTR)(lpStream+dwMemPos), dwMemLength-dwMemPos);
}


bool CSoundFile::ReadPATSample(SAMPLEINDEX nSample, LPBYTE lpStream, DWORD dwMemLength)
//-------------------------------------------------------------------------------------
{
    DWORD dwMemPos = sizeof(GF1PATCHFILEHEADER)+sizeof(GF1INSTRUMENT)+sizeof(GF1LAYER);
    GF1PATCHFILEHEADER *phdr = (GF1PATCHFILEHEADER *)lpStream;
    GF1INSTRUMENT *pinshdr = (GF1INSTRUMENT *)(lpStream+sizeof(GF1PATCHFILEHEADER));

    if ((!lpStream) || (dwMemLength < 512)
     || (phdr->gf1p != 0x50314647) || (phdr->atch != 0x48435441)
     || (phdr->version[3] != 0) || (phdr->id[9] != 0) || (phdr->instrum < 1)
     || (!phdr->samples) || (!pinshdr->layers)) return false;
    DestroySample(nSample);
    PatchToSample(this, nSample, lpStream+dwMemPos, dwMemLength-dwMemPos);
    if (pinshdr->name[0] > ' ')
    {
        memcpy(m_szNames[nSample], pinshdr->name, 16);
        m_szNames[nSample][16] = 0;
    }
    return true;
}


// PAT Instrument
bool CSoundFile::ReadPATInstrument(INSTRUMENTINDEX nInstr, LPBYTE lpStream, DWORD dwMemLength)
//--------------------------------------------------------------------------------------------
{
    GF1PATCHFILEHEADER *phdr = (GF1PATCHFILEHEADER *)lpStream;
    GF1INSTRUMENT *pih = (GF1INSTRUMENT *)(lpStream+sizeof(GF1PATCHFILEHEADER));
    GF1LAYER *plh = (GF1LAYER *)(lpStream+sizeof(GF1PATCHFILEHEADER)+sizeof(GF1INSTRUMENT));
    modplug::tracker::modinstrument_t *pIns;
    DWORD dwMemPos = sizeof(GF1PATCHFILEHEADER)+sizeof(GF1INSTRUMENT)+sizeof(GF1LAYER);
    UINT nSamples;

    if ((!lpStream) || (dwMemLength < 512)
     || (phdr->gf1p != 0x50314647) || (phdr->atch != 0x48435441)
     || (phdr->version[3] != 0) || (phdr->id[9] != 0)
     || (phdr->instrum < 1) || (!phdr->samples)
     || (!pih->layers) || (!plh->samples)) return false;
    if (nInstr > m_nInstruments) m_nInstruments = nInstr;
// -> CODE#0003
// -> DESC="remove instrument's samples"
//    RemoveInstrumentSamples(nInstr);
    DestroyInstrument(nInstr,1);
// -! BEHAVIOUR_CHANGE#0003
    pIns = new modplug::tracker::modinstrument_t;
    if (!pIns) return false;
    MemsetZero(*pIns);
    pIns->pTuning = pIns->s_DefaultTuning;
    Instruments[nInstr] = pIns;
    nSamples = plh->samples;
    if (nSamples > 16) nSamples = 16;
    memcpy(pIns->name, pih->name, 16);
    pIns->name[16] = 0;
    pIns->fadeout = 2048;
    pIns->global_volume = 64;
    pIns->default_pan = 128;
    pIns->pitch_pan_center = 60;
    pIns->resampling_mode = SRCMODE_DEFAULT;
    pIns->default_filter_mode = FLTMODE_UNCHANGED;
    if (m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))
    {
        pIns->new_note_action = NNA_NOTEOFF;
        pIns->duplicate_note_action = DNA_NOTEFADE;
    }
    UINT nFreeSmp = 1;
    UINT nMinSmpNote = 0xff;
    UINT nMinSmp = 0;
    for (UINT iSmp=0; iSmp<nSamples; iSmp++)
    {
        // Find a free sample
        while ((nFreeSmp < MAX_SAMPLES) && ((Samples[nFreeSmp].sample_data) || (m_szNames[nFreeSmp][0]))) nFreeSmp++;
        if (nFreeSmp >= MAX_SAMPLES) break;
        if (m_nSamples < nFreeSmp) m_nSamples = nFreeSmp;
        if (!nMinSmp) nMinSmp = nFreeSmp;
        // Load it
        GF1SAMPLEHEADER *psh = (GF1SAMPLEHEADER *)(lpStream+dwMemPos);
        PatchToSample(this, nFreeSmp, lpStream+dwMemPos, dwMemLength-dwMemPos);
        LONG nMinNote = (psh->low_freq > 100) ? PatchFreqToNote(psh->low_freq) : 0;
        LONG nMaxNote = (psh->high_freq > 100) ? PatchFreqToNote(psh->high_freq) : NOTE_MAX;
        LONG nBaseNote = (psh->root_freq > 100) ? PatchFreqToNote(psh->root_freq) : -1;
        if ((!psh->scale_factor) && (nSamples == 1)) { nMinNote = 0; nMaxNote = NOTE_MAX; }
        // Fill Note Map
        for (UINT k=0; k<NOTE_MAX; k++)
        {
            if (((LONG)k == nBaseNote)
             || ((!pIns->Keyboard[k])
              && ((LONG)k >= nMinNote)
              && ((LONG)k <= nMaxNote)))
            {
                if (psh->scale_factor)
                    pIns->NoteMap[k] = (uint8_t)(k+1);
                else
                    pIns->NoteMap[k] = 5*12+1;
                pIns->Keyboard[k] = nFreeSmp;
                if (k < nMinSmpNote)
                {
                    nMinSmpNote = k;
                    nMinSmp = nFreeSmp;
                }
            }
        }
    /*
        // Create dummy envelope
        if (!iSmp)
        {
            pIns->flags |= ENV_VOLUME;
            if (psh->flags & 32) pIns->flags |= ENV_VOLSUSTAIN;
            pIns->volume_envelope.Values[0] = 64;
            pIns->volume_envelope.Ticks[0] = 0;
            pIns->volume_envelope.Values[1] = 64;
            pIns->volume_envelope.Ticks[1] = 1;
            pIns->volume_envelope.Values[2] = 32;
            pIns->volume_envelope.Ticks[2] = 20;
            pIns->volume_envelope.Values[3] = 0;
            pIns->volume_envelope.Ticks[3] = 100;
            pIns->volume_envelope.num_nodes = 4;
        }
    */
        // Skip to next sample
        dwMemPos += sizeof(GF1SAMPLEHEADER)+psh->length;
        if (dwMemPos + sizeof(GF1SAMPLEHEADER) >= dwMemLength) break;
    }
    if (nMinSmp)
    {
        // Fill note map and missing samples
        for (UINT k=0; k<NOTE_MAX; k++)
        {
            if (!pIns->NoteMap[k]) pIns->NoteMap[k] = (uint8_t)(k+1);
            if (!pIns->Keyboard[k])
            {
                pIns->Keyboard[k] = nMinSmp;
            } else
            {
                nMinSmp = pIns->Keyboard[k];
            }
        }
    }
    return true;
}


/////////////////////////////////////////////////////////////
// S3I Samples

typedef struct S3ISAMPLESTRUCT
{
    uint8_t id;
    CHAR filename[12];
    uint8_t reserved1;
    uint16_t offset;
    DWORD length;
    DWORD loopstart;
    DWORD loopend;
    uint8_t volume;
    uint8_t reserved2;
    uint8_t pack;
    uint8_t flags;
    DWORD nC5Speed;
    DWORD reserved3;
    DWORD reserved4;
    DWORD date;
    CHAR name[28];
    DWORD scrs;
} S3ISAMPLESTRUCT;

bool CSoundFile::ReadS3ISample(SAMPLEINDEX nSample, LPBYTE lpMemFile, DWORD dwFileLength)
//---------------------------------------------------------------------------------------
{
    S3ISAMPLESTRUCT *pss = (S3ISAMPLESTRUCT *)lpMemFile;
    modplug::tracker::modsample_t *pSmp = &Samples[nSample];
    DWORD dwMemPos;
    UINT flags;

    if ((!lpMemFile) || (dwFileLength < sizeof(S3ISAMPLESTRUCT))
     || (pss->id != 0x01) || (((DWORD)pss->offset << 4) >= dwFileLength)
     || (pss->scrs != 0x53524353)) return false;
    DestroySample(nSample);
    dwMemPos = pss->offset << 4;
    memcpy(pSmp->legacy_filename, pss->filename, 12);
    memcpy(m_szNames[nSample], pss->name, 28);
    m_szNames[nSample][28] = 0;
    pSmp->length = pss->length;
    pSmp->loop_start = pss->loopstart;
    pSmp->loop_end = pss->loopend;
    pSmp->global_volume = 64;
    pSmp->default_volume = pss->volume << 2;
    pSmp->flags = 0;
    pSmp->default_pan = 128;
    pSmp->c5_samplerate = pss->nC5Speed;
    pSmp->RelativeTone = 0;
    pSmp->nFineTune = 0;
    if (m_nType & MOD_TYPE_XM) FrequencyToTranspose(pSmp);
    if (pss->flags & 0x01) pSmp->flags |= CHN_LOOP;
    flags = (pss->flags & 0x04) ? RS_PCM16U : RS_PCM8U;
    if (pss->flags & 0x02) flags |= RSF_STEREO;
    ReadSample(pSmp, flags, (LPSTR)(lpMemFile+dwMemPos), dwFileLength-dwMemPos);
    return true;
}


/////////////////////////////////////////////////////////////
// XI Instruments

typedef struct XIFILEHEADER
{
    CHAR extxi[21];    	// Extended Instrument:
    CHAR name[23];    	// Name, 1Ah
    CHAR trkname[20];    // FastTracker v2.00
    uint16_t shsize;    	// 0x0102
} XIFILEHEADER;


typedef struct XIINSTRUMENTHEADER
{
    uint8_t snum[96];
    uint16_t venv[24];
    uint16_t pIns[24];
    uint8_t vnum, pnum;
    uint8_t vsustain, vloops, vloope, psustain, ploops, ploope;
    uint8_t vtype, ptype;
    uint8_t vibtype, vibsweep, vibdepth, vibrate;
    uint16_t volfade;
    uint16_t res;
    uint8_t reserved1[20];
    uint16_t reserved2;
} XIINSTRUMENTHEADER;


typedef struct XISAMPLEHEADER
{
    DWORD samplen;
    DWORD loopstart;
    DWORD looplen;
    uint8_t vol;
    signed char finetune;
    uint8_t type;
    uint8_t pan;
    signed char relnote;
    uint8_t res;
    char name[22];
} XISAMPLEHEADER;




bool CSoundFile::ReadXIInstrument(INSTRUMENTINDEX nInstr, LPBYTE lpMemFile, DWORD dwFileLength)
//---------------------------------------------------------------------------------------------
{
    XIFILEHEADER *pxh = (XIFILEHEADER *)lpMemFile;
    XIINSTRUMENTHEADER *pih = (XIINSTRUMENTHEADER *)(lpMemFile+sizeof(XIFILEHEADER));
    UINT samplemap[32];
    UINT sampleflags[32];
    DWORD samplesize[32];
    DWORD dwMemPos = sizeof(XIFILEHEADER)+sizeof(XIINSTRUMENTHEADER);
    UINT nsamples;

    if ((!lpMemFile) || (dwFileLength < sizeof(XIFILEHEADER)+sizeof(XIINSTRUMENTHEADER))) return false;
    if (dwMemPos + pxh->shsize - 0x102 >= dwFileLength) return false;
    if (memcmp(pxh->extxi, "Extended Instrument", 19)) return false;
    dwMemPos += pxh->shsize - 0x102;
    if ((dwMemPos < sizeof(XIFILEHEADER)) || (dwMemPos >= dwFileLength)) return false;
    if (nInstr > m_nInstruments) m_nInstruments = nInstr;
// -> CODE#0003
// -> DESC="remove instrument's samples"
//    RemoveInstrumentSamples(nInstr);
    DestroyInstrument(nInstr,1);
// -! BEHAVIOUR_CHANGE#0003
    Instruments[nInstr] = new modplug::tracker::modinstrument_t;
    modplug::tracker::modinstrument_t *pIns = Instruments[nInstr];
    if (!pIns) return false;
    memset(pIns, 0, sizeof(modplug::tracker::modinstrument_t));
    pIns->pTuning = pIns->s_DefaultTuning;
    memcpy(pIns->name, pxh->name, 22);
    nsamples = 0;
    for (UINT i=0; i<96; i++)
    {
        pIns->NoteMap[i+12] = i+1+12;
        if (pih->snum[i] > nsamples) nsamples = pih->snum[i];
    }
    nsamples++;
    if (nsamples > 32) nsamples = 32;
    // Allocate samples
    memset(samplemap, 0, sizeof(samplemap));
    UINT nsmp = 1;
    for (UINT j=0; j<nsamples; j++)
    {
        while ((nsmp < MAX_SAMPLES) && ((Samples[nsmp].sample_data) || (m_szNames[nsmp][0]))) nsmp++;
        if (nsmp >= MAX_SAMPLES) break;
        samplemap[j] = nsmp;
        if (m_nSamples < nsmp) m_nSamples = nsmp;
        nsmp++;
    }
    // Setting up instrument header
    for (UINT k=0; k<96; k++)
    {
        UINT n = pih->snum[k];
        if (n < nsamples) pIns->Keyboard[k+12] = samplemap[n];
    }
    pIns->fadeout = pih->volfade;
    if (pih->vtype & 1) pIns->volume_envelope.flags |= ENV_ENABLED;
    if (pih->vtype & 2) pIns->volume_envelope.flags |= ENV_SUSTAIN;
    if (pih->vtype & 4) pIns->volume_envelope.flags |= ENV_LOOP;
    if (pih->ptype & 1) pIns->panning_envelope.flags |= ENV_ENABLED;
    if (pih->ptype & 2) pIns->panning_envelope.flags |= ENV_SUSTAIN;
    if (pih->ptype & 4) pIns->panning_envelope.flags |= ENV_LOOP;
    pIns->volume_envelope.num_nodes = pih->vnum;
    pIns->panning_envelope.num_nodes = pih->pnum;
    if (pIns->volume_envelope.num_nodes > 12) pIns->volume_envelope.num_nodes = 12;
    if (pIns->panning_envelope.num_nodes > 12) pIns->panning_envelope.num_nodes = 12;
    if (!pIns->volume_envelope.num_nodes) pIns->volume_envelope.flags &= ~ENV_ENABLED;
    if (!pIns->panning_envelope.num_nodes) pIns->panning_envelope.flags &= ~ENV_ENABLED;
    pIns->volume_envelope.sustain_start = pih->vsustain;
    pIns->volume_envelope.sustain_end = pih->vsustain;
    if (pih->vsustain >= 12) pIns->volume_envelope.flags &= ~ENV_SUSTAIN;
    pIns->volume_envelope.loop_start = pih->vloops;
    pIns->volume_envelope.loop_end = pih->vloope;
    if (pIns->volume_envelope.loop_end >= 12) pIns->volume_envelope.loop_end = 0;
    if (pIns->volume_envelope.loop_start >= pIns->volume_envelope.loop_end) pIns->volume_envelope.flags &= ~ENV_LOOP;
    pIns->panning_envelope.sustain_start = pih->psustain;
    pIns->panning_envelope.sustain_end = pih->psustain;
    if (pih->psustain >= 12) pIns->panning_envelope.flags &= ~ENV_SUSTAIN;
    pIns->panning_envelope.loop_start = pih->ploops;
    pIns->panning_envelope.loop_end = pih->ploope;
    if (pIns->panning_envelope.loop_end >= 12) pIns->panning_envelope.loop_end = 0;
    if (pIns->panning_envelope.loop_start >= pIns->panning_envelope.loop_end) pIns->panning_envelope.flags &= ~ENV_LOOP;
    pIns->global_volume = 64;
    pIns->pitch_pan_center = 5*12;
    SetDefaultInstrumentValues(pIns);
    for (UINT ienv=0; ienv<12; ienv++)
    {
        pIns->volume_envelope.Ticks[ienv] = (uint16_t)pih->venv[ienv*2];
        pIns->volume_envelope.Values[ienv] = (uint8_t)pih->venv[ienv*2+1];
        pIns->panning_envelope.Ticks[ienv] = (uint16_t)pih->pIns[ienv*2];
        pIns->panning_envelope.Values[ienv] = (uint8_t)pih->pIns[ienv*2+1];
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
    // Reading samples
    memset(sampleflags, 0, sizeof(sampleflags));
    memset(samplesize, 0, sizeof(samplesize));
    UINT maxsmp = nsamples;
    if ((pih->reserved2 > maxsmp) && (pih->reserved2 <= 32)) maxsmp = pih->reserved2;
    for (UINT ismp=0; ismp<maxsmp; ismp++)
    {
        if (dwMemPos + sizeof(XISAMPLEHEADER) > dwFileLength) break;
        XISAMPLEHEADER *psh = (XISAMPLEHEADER *)(lpMemFile+dwMemPos);
        dwMemPos += sizeof(XISAMPLEHEADER);
        if (ismp >= nsamples) continue;
        sampleflags[ismp] = RS_PCM8S;
        samplesize[ismp] = psh->samplen;
        if (!samplemap[ismp]) continue;
        modplug::tracker::modsample_t *pSmp = &Samples[samplemap[ismp]];
        pSmp->flags = 0;
        pSmp->length = psh->samplen;
        pSmp->loop_start = psh->loopstart;
        pSmp->loop_end = psh->loopstart + psh->looplen;
        if (psh->type & 0x10)
        {
            pSmp->length /= 2;
            pSmp->loop_start /= 2;
            pSmp->loop_end /= 2;
        }
        if (psh->type & 0x20)
        {
            pSmp->length /= 2;
            pSmp->loop_start /= 2;
            pSmp->loop_end /= 2;
        }
        if (pSmp->length > MAX_SAMPLE_LENGTH) pSmp->length = MAX_SAMPLE_LENGTH;
        if (psh->type & 3) pSmp->flags |= CHN_LOOP;
        if (psh->type & 2) pSmp->flags |= CHN_PINGPONGLOOP;
        if (pSmp->loop_end > pSmp->length) pSmp->loop_end = pSmp->length;
        if (pSmp->loop_start >= pSmp->loop_end)
        {
            pSmp->flags &= ~CHN_LOOP;
            pSmp->loop_start = 0;
        }
        pSmp->default_volume = psh->vol << 2;
        if (pSmp->default_volume > 256) pSmp->default_volume = 256;
        pSmp->global_volume = 64;
        sampleflags[ismp] = (psh->type & 0x10) ? RS_PCM16D : RS_PCM8D;
        if (psh->type & 0x20) sampleflags[ismp] = (psh->type & 0x10) ? RS_STPCM16D : RS_STPCM8D;
        pSmp->nFineTune = psh->finetune;
        pSmp->c5_samplerate = 8363;
        pSmp->RelativeTone = (int)psh->relnote;
        if (m_nType != MOD_TYPE_XM)
        {
            pSmp->c5_samplerate = TransposeToFrequency(pSmp->RelativeTone, pSmp->nFineTune);
            pSmp->RelativeTone = 0;
            pSmp->nFineTune = 0;
        }
        pSmp->default_pan = psh->pan;
        pSmp->flags |= CHN_PANNING;
        pSmp->vibrato_type = pih->vibtype;
        pSmp->vibrato_sweep = pih->vibsweep;
        pSmp->vibrato_depth = pih->vibdepth;
        pSmp->vibrato_rate = pih->vibrate;
        memset(m_szNames[samplemap[ismp]], 0, 32);
        memcpy(m_szNames[samplemap[ismp]], psh->name, 22);
        memcpy(pSmp->legacy_filename, psh->name, 22);
        pSmp->legacy_filename[21] = 0;
    }
    // Reading sample data
    for (UINT dsmp=0; dsmp<nsamples; dsmp++)
    {
        if (dwMemPos >= dwFileLength) break;
        if (samplemap[dsmp])
            ReadSample(&Samples[samplemap[dsmp]], sampleflags[dsmp], (LPSTR)(lpMemFile+dwMemPos), dwFileLength-dwMemPos);
        dwMemPos += samplesize[dsmp];
    }

// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"

    // Leave if no extra instrument settings are available (end of file reached)
    if(dwMemPos >= dwFileLength) return true;

    ReadExtendedInstrumentProperties(pIns, lpMemFile + dwMemPos, dwFileLength - dwMemPos);

// -! NEW_FEATURE#0027

    return true;
}


bool CSoundFile::SaveXIInstrument(INSTRUMENTINDEX nInstr, LPCSTR lpszFileName)
//----------------------------------------------------------------------------
{
    XIFILEHEADER xfh;
    XIINSTRUMENTHEADER xih;
    XISAMPLEHEADER xsh;
    modplug::tracker::modinstrument_t *pIns = Instruments[nInstr];
    UINT smptable[32];
    UINT nsamples;
    FILE *f;

    if ((!pIns) || (!lpszFileName)) return false;
    if ((f = fopen(lpszFileName, "wb")) == NULL) return false;
    // XI File Header
    memset(&xfh, 0, sizeof(xfh));
    memset(&xih, 0, sizeof(xih));
    memcpy(xfh.extxi, "Extended Instrument: ", 21);
    memcpy(xfh.name, pIns->name, 22);
    xfh.name[22] = 0x1A;
    memcpy(xfh.trkname, "Created by OpenMPT  ", 20);
    xfh.shsize = 0x102;
    fwrite(&xfh, 1, sizeof(xfh), f);
    // XI Instrument Header
    xih.volfade = pIns->fadeout;
    xih.vnum = pIns->volume_envelope.num_nodes;
    xih.pnum = pIns->panning_envelope.num_nodes;
    if (xih.vnum > 12) xih.vnum = 12;
    if (xih.pnum > 12) xih.pnum = 12;
    for (UINT ienv=0; ienv<12; ienv++)
    {
        xih.venv[ienv*2] = (uint8_t)pIns->volume_envelope.Ticks[ienv];
        xih.venv[ienv*2+1] = pIns->volume_envelope.Values[ienv];
        xih.pIns[ienv*2] = (uint8_t)pIns->panning_envelope.Ticks[ienv];
        xih.pIns[ienv*2+1] = pIns->panning_envelope.Values[ienv];
    }
    if (pIns->volume_envelope.flags & ENV_ENABLED) xih.vtype |= 1;
    if (pIns->volume_envelope.flags & ENV_SUSTAIN) xih.vtype |= 2;
    if (pIns->volume_envelope.flags & ENV_LOOP) xih.vtype |= 4;
    if (pIns->panning_envelope.flags & ENV_ENABLED) xih.ptype |= 1;
    if (pIns->panning_envelope.flags & ENV_SUSTAIN) xih.ptype |= 2;
    if (pIns->panning_envelope.flags & ENV_LOOP) xih.ptype |= 4;
    xih.vsustain = (uint8_t)pIns->volume_envelope.sustain_start;
    xih.vloops = (uint8_t)pIns->volume_envelope.loop_start;
    xih.vloope = (uint8_t)pIns->volume_envelope.loop_end;
    xih.psustain = (uint8_t)pIns->panning_envelope.sustain_start;
    xih.ploops = (uint8_t)pIns->panning_envelope.loop_start;
    xih.ploope = (uint8_t)pIns->panning_envelope.loop_end;
    nsamples = 0;
    for (UINT j=0; j<96; j++) if (pIns->Keyboard[j+12])
    {
        UINT n = pIns->Keyboard[j+12];
        UINT k = 0;
        for (k=0; k<nsamples; k++)    if (smptable[k] == n) break;
        if (k == nsamples)
        {
            if (!k)
            {
                xih.vibtype = Samples[n].vibrato_type;
                xih.vibsweep = min(Samples[n].vibrato_sweep, 255);
                xih.vibdepth = min(Samples[n].vibrato_depth, 15);
                xih.vibrate = min(Samples[n].vibrato_rate, 63);
            }
            if (nsamples < 32) smptable[nsamples++] = n;
            k = nsamples - 1;
        }
        xih.snum[j] = k;
    }
    xih.reserved2 = nsamples;
    fwrite(&xih, 1, sizeof(xih), f);
    // XI Sample Headers
    for (UINT ismp=0; ismp<nsamples; ismp++)
    {
        modplug::tracker::modsample_t *pSmp = &Samples[smptable[ismp]];
        xsh.samplen = pSmp->length;
        xsh.loopstart = pSmp->loop_start;
        xsh.looplen = pSmp->loop_end - pSmp->loop_start;
        xsh.vol = pSmp->default_volume >> 2;
        xsh.finetune = (signed char)pSmp->nFineTune;
        xsh.type = 0;
        if (pSmp->flags & CHN_16BIT)
        {
            xsh.type |= 0x10;
            xsh.samplen *= 2;
            xsh.loopstart *= 2;
            xsh.looplen *= 2;
        }
        if (pSmp->flags & CHN_STEREO)
        {
            xsh.type |= 0x20;
            xsh.samplen *= 2;
            xsh.loopstart *= 2;
            xsh.looplen *= 2;
        }
        if (pSmp->flags & CHN_LOOP)
        {
            xsh.type |= (pSmp->flags & CHN_PINGPONGLOOP) ? 0x02 : 0x01;
        }
        xsh.pan = (uint8_t)pSmp->default_pan;
        if (pSmp->default_pan > 0xFF) xsh.pan = 0xFF;
        if ((m_nType & MOD_TYPE_XM) || (!pSmp->c5_samplerate))
            xsh.relnote = (signed char) pSmp->RelativeTone;
        else
        {
            int f2t = FrequencyToTranspose(pSmp->c5_samplerate);
            xsh.relnote = (signed char)(f2t >> 7);
            xsh.finetune = (signed char)(f2t & 0x7F);
        }
        xsh.res = 0;
        memcpy(xsh.name, pSmp->legacy_filename, 22);
        fwrite(&xsh, 1, sizeof(xsh), f);
    }
    // XI Sample Data
    for (UINT dsmp=0; dsmp<nsamples; dsmp++)
    {
        modplug::tracker::modsample_t *pSmp = &Samples[smptable[dsmp]];
        UINT smpflags = (pSmp->flags & CHN_16BIT) ? RS_PCM16D : RS_PCM8D;
        if (pSmp->flags & CHN_STEREO) smpflags = (pSmp->flags & CHN_16BIT) ? RS_STPCM16D : RS_STPCM8D;
        WriteSample(f, pSmp, smpflags);
    }

    __int32 code = 'MPTX';
    fwrite(&code, 1, sizeof(__int32), f);    // Write extension tag
    WriteInstrumentHeaderStruct(pIns, f);    // Write full extended header.


    fclose(f);
    return true;
}


bool CSoundFile::ReadXISample(SAMPLEINDEX nSample, LPBYTE lpMemFile, DWORD dwFileLength)
//--------------------------------------------------------------------------------------
{
    XIFILEHEADER *pxh = (XIFILEHEADER *)lpMemFile;
    XIINSTRUMENTHEADER *pih = (XIINSTRUMENTHEADER *)(lpMemFile+sizeof(XIFILEHEADER));
    UINT sampleflags = 0;
    DWORD dwMemPos = sizeof(XIFILEHEADER)+sizeof(XIINSTRUMENTHEADER);
    modplug::tracker::modsample_t *pSmp = &Samples[nSample];
    UINT nsamples;

    if ((!lpMemFile) || (dwFileLength < sizeof(XIFILEHEADER)+sizeof(XIINSTRUMENTHEADER))) return false;
    if (memcmp(pxh->extxi, "Extended Instrument", 19)) return false;
    dwMemPos += pxh->shsize - 0x102;
    if ((dwMemPos < sizeof(XIFILEHEADER)) || (dwMemPos >= dwFileLength)) return false;
    DestroySample(nSample);
    nsamples = 0;
    for (UINT i=0; i<96; i++)
    {
        if (pih->snum[i] > nsamples) nsamples = pih->snum[i];
    }
    nsamples++;
    memcpy(m_szNames[nSample], pxh->name, 22);
    pSmp->flags = 0;
    pSmp->global_volume = 64;
    // Reading sample
    UINT maxsmp = nsamples;
    if ((pih->reserved2 <= 16) && (pih->reserved2 > maxsmp)) maxsmp = pih->reserved2;
    for (UINT ismp=0; ismp<maxsmp; ismp++)
    {
        if (dwMemPos + sizeof(XISAMPLEHEADER) > dwFileLength) break;
        XISAMPLEHEADER *psh = (XISAMPLEHEADER *)(lpMemFile+dwMemPos);
        dwMemPos += sizeof(XISAMPLEHEADER);
        if (ismp) continue;
        sampleflags = RS_PCM8S;
        pSmp->length = psh->samplen;
        pSmp->loop_start = psh->loopstart;
        pSmp->loop_end = psh->loopstart + psh->looplen;
        if (psh->type & 0x10)
        {
            pSmp->length /= 2;
            pSmp->loop_start /= 2;
            pSmp->loop_end /= 2;
        }
        if (psh->type & 0x20)
        {
            pSmp->length /= 2;
            pSmp->loop_start /= 2;
            pSmp->loop_end /= 2;
        }
        if (pSmp->length > MAX_SAMPLE_LENGTH) pSmp->length = MAX_SAMPLE_LENGTH;
        if (psh->type & 3) pSmp->flags |= CHN_LOOP;
        if (psh->type & 2) pSmp->flags |= CHN_PINGPONGLOOP;
        if (pSmp->loop_end > pSmp->length) pSmp->loop_end = pSmp->length;
        if (pSmp->loop_start >= pSmp->loop_end)
        {
            pSmp->flags &= ~CHN_LOOP;
            pSmp->loop_start = 0;
        }
        pSmp->default_volume = psh->vol << 2;
        if (pSmp->default_volume > 256) pSmp->default_volume = 256;
        pSmp->global_volume = 64;
        sampleflags = (psh->type & 0x10) ? RS_PCM16D : RS_PCM8D;
        if (psh->type & 0x20) sampleflags = (psh->type & 0x10) ? RS_STPCM16D : RS_STPCM8D;
        pSmp->nFineTune = psh->finetune;
        pSmp->c5_samplerate = 8363;
        pSmp->RelativeTone = (int)psh->relnote;
        if (m_nType != MOD_TYPE_XM)
        {
            pSmp->c5_samplerate = TransposeToFrequency(pSmp->RelativeTone, pSmp->nFineTune);
            pSmp->RelativeTone = 0;
            pSmp->nFineTune = 0;
        }
        pSmp->default_pan = psh->pan;
        pSmp->flags |= CHN_PANNING;
        pSmp->vibrato_type = pih->vibtype;
        pSmp->vibrato_sweep = pih->vibsweep;
        pSmp->vibrato_depth = pih->vibdepth;
        pSmp->vibrato_rate = pih->vibrate;
        memcpy(pSmp->legacy_filename, psh->name, 22);
        pSmp->legacy_filename[21] = 0;
    }
    if (dwMemPos >= dwFileLength) return true;
    ReadSample(pSmp, sampleflags, (LPSTR)(lpMemFile+dwMemPos), dwFileLength-dwMemPos);
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////////
// AIFF File I/O

typedef struct AIFFFILEHEADER
{
    DWORD dwFORM;    // "FORM" -> 0x4D524F46
    DWORD dwLen;
    DWORD dwAIFF;    // "AIFF" -> 0x46464941
} AIFFFILEHEADER;

typedef struct AIFFCOMM
{
    DWORD dwCOMM;    // "COMM" -> 0x4D4D4F43
    DWORD dwLen;
    uint16_t wChannels;
    uint16_t wFramesHi;    // Align!
    uint16_t wFramesLo;
    uint16_t wSampleSize;
    uint8_t xSampleRate[10];
} AIFFCOMM;

typedef struct AIFFSSND
{
    DWORD dwSSND;    // "SSND" -> 0x444E5353
    DWORD dwLen;
    DWORD dwOffset;
    DWORD dwBlkSize;
} AIFFSSND;


static DWORD FetchLong(LPBYTE p)
{
    DWORD d = p[0];
    d = (d << 8) | p[1];
    d = (d << 8) | p[2];
    d = (d << 8) | p[3];
    return d;
}


static DWORD Ext2Long(LPBYTE p)
{
    DWORD mantissa, last=0;
    uint8_t exp;

    mantissa = FetchLong(p+2);
    exp = 30 - p[1];
    while (exp--)
    {
        last = mantissa;
        mantissa >>= 1;
    }
    if (last & 1) mantissa++;
    return mantissa;
}



bool CSoundFile::ReadAIFFSample(SAMPLEINDEX nSample, LPBYTE lpMemFile, DWORD dwFileLength)
//----------------------------------------------------------------------------------------
{
    DWORD dwMemPos = sizeof(AIFFFILEHEADER);
    DWORD dwFORMLen, dwCOMMLen, dwSSNDLen;
    AIFFFILEHEADER *phdr = (AIFFFILEHEADER *)lpMemFile;
    AIFFCOMM *pcomm;
    AIFFSSND *psnd;
    UINT nType;

    if ((!lpMemFile) || (dwFileLength < (DWORD)sizeof(AIFFFILEHEADER))) return false;
    dwFORMLen = BigEndian(phdr->dwLen);
    if ((phdr->dwFORM != 0x4D524F46) || (phdr->dwAIFF != 0x46464941)
     || (dwFORMLen > dwFileLength) || (dwFORMLen < (DWORD)sizeof(AIFFCOMM))) return false;
    pcomm = (AIFFCOMM *)(lpMemFile+dwMemPos);
    dwCOMMLen = BigEndian(pcomm->dwLen);
    if ((pcomm->dwCOMM != 0x4D4D4F43) || (dwCOMMLen < 0x12) || (dwCOMMLen >= dwFileLength)) return false;
    if ((pcomm->wChannels != 0x0100) && (pcomm->wChannels != 0x0200)) return false;
    if ((pcomm->wSampleSize != 0x0800) && (pcomm->wSampleSize != 0x1000)) return false;
    dwMemPos += dwCOMMLen + 8;
    if (dwMemPos + sizeof(AIFFSSND) >= dwFileLength) return false;
    psnd = (AIFFSSND *)(lpMemFile+dwMemPos);
    dwSSNDLen = BigEndian(psnd->dwLen);
    if ((psnd->dwSSND != 0x444E5353) || (dwSSNDLen >= dwFileLength) || (dwSSNDLen < 8)) return false;
    dwMemPos += sizeof(AIFFSSND);
    if (dwMemPos >= dwFileLength) return false;
    DestroySample(nSample);
    if (pcomm->wChannels == 0x0100)
    {
        nType = (pcomm->wSampleSize == 0x1000) ? RS_PCM16M : RS_PCM8S;
    } else
    {
        nType = (pcomm->wSampleSize == 0x1000) ? RS_STIPCM16M : RS_STIPCM8S;
    }
    UINT samplesize = (pcomm->wSampleSize >> 11) * (pcomm->wChannels >> 8);
    if (!samplesize) samplesize = 1;
    modplug::tracker::modsample_t *pSmp = &Samples[nSample];
    if (pSmp->sample_data)
    {
        FreeSample(pSmp->sample_data);
        pSmp->sample_data = nullptr;
        pSmp->length = 0;
    }
    pSmp->length = dwSSNDLen / samplesize;
    pSmp->loop_start = pSmp->loop_end = 0;
    pSmp->sustain_start = pSmp->sustain_end = 0;
    pSmp->c5_samplerate = Ext2Long(pcomm->xSampleRate);
    pSmp->default_pan = 128;
    pSmp->default_volume = 256;
    pSmp->global_volume = 64;
    pSmp->flags = (pcomm->wSampleSize > 0x0800) ? CHN_16BIT : 0;
    if (pcomm->wChannels >= 0x0200) pSmp->flags |= CHN_STEREO;
    if (m_nType & MOD_TYPE_XM) pSmp->flags |= CHN_PANNING;
    pSmp->RelativeTone = 0;
    pSmp->nFineTune = 0;
    if (m_nType & MOD_TYPE_XM) FrequencyToTranspose(pSmp);
    pSmp->vibrato_type = pSmp->vibrato_sweep = pSmp->vibrato_depth = pSmp->vibrato_rate = 0;
    pSmp->legacy_filename[0] = 0;
    m_szNames[nSample][0] = 0;
    if (pSmp->length > MAX_SAMPLE_LENGTH) pSmp->length = MAX_SAMPLE_LENGTH;
    ReadSample(pSmp, nType, (LPSTR)(lpMemFile+dwMemPos), dwFileLength-dwMemPos);
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////////
// ITS Samples

// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
//BOOL CSoundFile::ReadITSSample(UINT nSample, LPBYTE lpMemFile, DWORD dwFileLength, DWORD dwOffset)
UINT CSoundFile::ReadITSSample(SAMPLEINDEX nSample, LPBYTE lpMemFile, DWORD dwFileLength, DWORD dwOffset)
//-------------------------------------------------------------------------------------------------------
{
    ITSAMPLESTRUCT *pis = (ITSAMPLESTRUCT *)lpMemFile;
    modplug::tracker::modsample_t *pSmp = &Samples[nSample];
    DWORD dwMemPos;

// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
//    if ((!lpMemFile) || (dwFileLength < sizeof(ITSAMPLESTRUCT))
//     || (pis->id != LittleEndian(IT_IMPS)) || (((DWORD)pis->samplepointer) >= dwFileLength + dwOffset)) return FALSE;
    if ((!lpMemFile) || (dwFileLength < sizeof(ITSAMPLESTRUCT))
     || (pis->id != LittleEndian(IT_IMPS)) || (((DWORD)pis->samplepointer) >= dwFileLength + dwOffset)) return 0;
// -! NEW_FEATURE#0027
    DestroySample(nSample);
    dwMemPos = pis->samplepointer - dwOffset;
    memcpy(pSmp->legacy_filename, pis->filename, 12);
    memcpy(m_szNames[nSample], pis->name, 26);
    m_szNames[nSample][26] = 0;
    pSmp->length = pis->length;
    if (pSmp->length > MAX_SAMPLE_LENGTH) pSmp->length = MAX_SAMPLE_LENGTH;
    pSmp->loop_start = pis->loopbegin;
    pSmp->loop_end = pis->loopend;
    pSmp->sustain_start = pis->susloopbegin;
    pSmp->sustain_end = pis->susloopend;
    pSmp->c5_samplerate = pis->C5Speed;
    if (!pSmp->c5_samplerate) pSmp->c5_samplerate = 8363;
    if (pis->C5Speed < 256) pSmp->c5_samplerate = 256;
    pSmp->RelativeTone = 0;
    pSmp->nFineTune = 0;
    if (GetType() == MOD_TYPE_XM) FrequencyToTranspose(pSmp);
    pSmp->default_volume = pis->vol << 2;
    if (pSmp->default_volume > 256) pSmp->default_volume = 256;
    pSmp->global_volume = pis->gvl;
    if (pSmp->global_volume > 64) pSmp->global_volume = 64;
    pSmp->flags = 0;
    if (pis->flags & 0x10) pSmp->flags |= CHN_LOOP;
    if (pis->flags & 0x20) pSmp->flags |= CHN_SUSTAINLOOP;
    if (pis->flags & 0x40) pSmp->flags |= CHN_PINGPONGLOOP;
    if (pis->flags & 0x80) pSmp->flags |= CHN_PINGPONGSUSTAIN;
    pSmp->default_pan = (pis->dfp & 0x7F) << 2;
    if (pSmp->default_pan > 256) pSmp->default_pan = 256;
    if (pis->dfp & 0x80) pSmp->flags |= CHN_PANNING;
    pSmp->vibrato_type = autovibit2xm[pis->vit & 7];
    pSmp->vibrato_sweep = pis->vir;
    pSmp->vibrato_depth = pis->vid;
    pSmp->vibrato_rate = pis->vis;
    UINT flags = (pis->cvt & 1) ? RS_PCM8S : RS_PCM8U;
    if (pis->flags & 2)
    {
        flags += 5;
        if (pis->flags & 4)
        {
            flags |= RSF_STEREO;
// -> CODE#0001
// -> DESC="enable saving stereo ITI"
            pSmp->flags |= CHN_STEREO;
// -! BUG_FIX#0001
        }
        pSmp->flags |= CHN_16BIT;
        // IT 2.14 16-bit packed sample ?
        if (pis->flags & 8) flags = RS_IT21416;
    } else
    {
        if (pis->flags & 4) flags |= RSF_STEREO;
        if (pis->cvt == 0xFF) flags = RS_ADPCM4; else
        // IT 2.14 8-bit packed sample ?
        if (pis->flags & 8)    flags =	RS_IT2148;
    }
// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
//    ReadSample(pSmp, flags, (LPSTR)(lpMemFile+dwMemPos), dwFileLength + dwOffset - dwMemPos);
//    return TRUE;
    return ReadSample(pSmp, flags, (LPSTR)(lpMemFile+dwMemPos), dwFileLength + dwOffset - dwMemPos);
// -! NEW_FEATURE#0027
}


bool CSoundFile::ReadITIInstrument(INSTRUMENTINDEX nInstr, LPBYTE lpMemFile, DWORD dwFileLength)
//----------------------------------------------------------------------------------------------
{
    ITINSTRUMENT *pinstr = (ITINSTRUMENT *)lpMemFile;
    uint16_t samplemap[NOTE_MAX];    //rewbs.noSamplePerInstroLimit (120 was 64)
    DWORD dwMemPos;
    UINT nsmp, nsamples;

    if ((!lpMemFile) || (dwFileLength < sizeof(ITINSTRUMENT))
     || (pinstr->id != LittleEndian(IT_IMPI))) return false;
    if (nInstr > m_nInstruments) m_nInstruments = nInstr;
// -> CODE#0003
// -> DESC="remove instrument's samples"
//    RemoveInstrumentSamples(nInstr);
    DestroyInstrument(nInstr,1);
// -! BEHAVIOUR_CHANGE#0003
    Instruments[nInstr] = new modplug::tracker::modinstrument_t;
    modplug::tracker::modinstrument_t *pIns = Instruments[nInstr];
    if (!pIns) return false;
    memset(pIns, 0, sizeof(modplug::tracker::modinstrument_t));
    pIns->pTuning = pIns->s_DefaultTuning;
    memset(samplemap, 0, sizeof(samplemap));
    dwMemPos = 554;
    dwMemPos += ITInstrToMPT(pinstr, pIns, pinstr->trkvers);
    nsamples = pinstr->nos;
// -> CODE#0019
// -> DESC="correctly load ITI & XI instruments sample note map"
//    if (nsamples >= 64) nsamples = 64;
// -! BUG_FIX#0019
    nsmp = 1;

// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
    // In order to properly compute the position, in file, of eventual extended settings
    // such as "attack" we need to keep the "real" size of the last sample as those extra
    // setting will follow this sample in the file
    UINT lastSampleSize = 0;
// -! NEW_FEATURE#0027

    // Reading Samples
    for (UINT i=0; i<nsamples; i++)
    {
        while ((nsmp < MAX_SAMPLES) && ((Samples[nsmp].sample_data) || (m_szNames[nsmp][0]))) nsmp++;
        if (nsmp >= MAX_SAMPLES) break;
        if (m_nSamples < nsmp) m_nSamples = nsmp;
        samplemap[i] = nsmp;
// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
//    	ReadITSSample(nsmp, lpMemFile+dwMemPos, dwFileLength-dwMemPos, dwMemPos);
        lastSampleSize = ReadITSSample(nsmp, lpMemFile+dwMemPos, dwFileLength-dwMemPos, dwMemPos);
// -! NEW_FEATURE#0027
        dwMemPos += sizeof(ITSAMPLESTRUCT);
        nsmp++;
    }
    for (UINT j=0; j<128; j++)
    {
// -> CODE#0019
// -> DESC="correctly load ITI & XI instruments sample note map"
//    	if ((pIns->Keyboard[j]) && (pIns->Keyboard[j] <= 64))
        if (pIns->Keyboard[j])
// -! BUG_FIX#0019
        {
            pIns->Keyboard[j] = samplemap[pIns->Keyboard[j]-1];
        }
    }

// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"

    // Rewind file pointer offset (dwMemPos) to last sample header position
    dwMemPos -= sizeof(ITSAMPLESTRUCT);
    uint8_t * ptr = (uint8_t *)(lpMemFile+dwMemPos);

    // Update file pointer offset (dwMemPos) to match the end of the sample datas
    ITSAMPLESTRUCT *pis = (ITSAMPLESTRUCT *)ptr;
    dwMemPos += pis->samplepointer - dwMemPos + lastSampleSize;
    // Leave if no extra instrument settings are available (end of file reached)
    if(dwMemPos >= dwFileLength) return true;

    ReadExtendedInstrumentProperties(pIns, lpMemFile + dwMemPos, dwFileLength - dwMemPos);

// -! NEW_FEATURE#0027

    return true;
}


bool CSoundFile::SaveITIInstrument(INSTRUMENTINDEX nInstr, LPCSTR lpszFileName)
//-----------------------------------------------------------------------------
{
    uint8_t buffer[554];
    ITINSTRUMENT *iti = (ITINSTRUMENT *)buffer;
    ITSAMPLESTRUCT itss;
    modplug::tracker::modinstrument_t *pIns = Instruments[nInstr];
    vector<bool> smpcount(GetNumSamples(), false);
    UINT smptable[MAX_SAMPLES], smpmap[MAX_SAMPLES];
    DWORD dwPos;
    FILE *f;

    if ((!pIns) || (!lpszFileName)) return false;
    if ((f = fopen(lpszFileName, "wb")) == NULL) return false;
    memset(buffer, 0, sizeof(buffer));
    memset(smptable, 0, sizeof(smptable));
    memset(smpmap, 0, sizeof(smpmap));
    iti->id = LittleEndian(IT_IMPI);    // "IMPI"
    memcpy(iti->filename, pIns->legacy_filename, 12);
    memcpy(iti->name, pIns->name, 26);
    SetNullTerminator(iti->name);
    iti->mpr = pIns->midi_program;
    iti->mch = pIns->midi_channel;
    iti->mbank = pIns->midi_bank; //rewbs.MidiBank
    iti->nna = pIns->new_note_action;
    iti->dct = pIns->duplicate_check_type;
    iti->dca = pIns->duplicate_note_action;
    iti->fadeout = pIns->fadeout >> 5;
    iti->pps = pIns->pitch_pan_separation;
    iti->ppc = pIns->pitch_pan_center;
    iti->gbv = (uint8_t)(pIns->global_volume << 1);
    iti->dfp = (uint8_t)pIns->default_pan >> 2;
    if (!(pIns->flags & INS_SETPANNING)) iti->dfp |= 0x80;
    iti->rv = pIns->random_volume_weight;
    iti->rp = pIns->random_pan_weight;
    iti->ifc = pIns->default_filter_cutoff;
    iti->ifr = pIns->default_filter_resonance;
    //iti->trkvers = 0x202;
    iti->trkvers =    0x220;	 //rewbs.ITVersion (was 0x202)
    iti->nos = 0;
    for (UINT i=0; i<NOTE_MAX; i++) if (pIns->Keyboard[i] < MAX_SAMPLES)
    {
        const UINT smp = pIns->Keyboard[i];
        if (smp && smp <= GetNumSamples() && !smpcount[smp - 1])
        {
            smpcount[smp - 1] = true;
            smptable[iti->nos] = smp;
            smpmap[smp] = iti->nos;
            iti->nos++;
        }
        iti->keyboard[i*2] = pIns->NoteMap[i] - 1;
// -> CODE#0019
// -> DESC="correctly load ITI & XI instruments sample note map"
//    	iti->keyboard[i*2+1] = smpmap[smp] + 1;
        iti->keyboard[i*2+1] = smp ? smpmap[smp] + 1 : 0;
// -! BUG_FIX#0019
    }
    // Writing Volume envelope
    if (pIns->volume_envelope.flags & ENV_ENABLED) iti->volenv.flags |= 0x01;
    if (pIns->volume_envelope.flags & ENV_LOOP) iti->volenv.flags |= 0x02;
    if (pIns->volume_envelope.flags & ENV_SUSTAIN) iti->volenv.flags |= 0x04;
    if (pIns->volume_envelope.flags & ENV_CARRY) iti->volenv.flags |= 0x08;
    iti->volenv.num = (uint8_t)pIns->volume_envelope.num_nodes;
    iti->volenv.lpb = (uint8_t)pIns->volume_envelope.loop_start;
    iti->volenv.lpe = (uint8_t)pIns->volume_envelope.loop_end;
    iti->volenv.slb = pIns->volume_envelope.sustain_start;
    iti->volenv.sle = pIns->volume_envelope.sustain_end;
    // Writing Panning envelope
    if (pIns->panning_envelope.flags & ENV_ENABLED) iti->panenv.flags |= 0x01;
    if (pIns->panning_envelope.flags & ENV_LOOP) iti->panenv.flags |= 0x02;
    if (pIns->panning_envelope.flags & ENV_SUSTAIN) iti->panenv.flags |= 0x04;
    if (pIns->panning_envelope.flags & ENV_CARRY) iti->panenv.flags |= 0x08;
    iti->panenv.num = (uint8_t)pIns->panning_envelope.num_nodes;
    iti->panenv.lpb = (uint8_t)pIns->panning_envelope.loop_start;
    iti->panenv.lpe = (uint8_t)pIns->panning_envelope.loop_end;
    iti->panenv.slb = pIns->panning_envelope.sustain_start;
    iti->panenv.sle = pIns->panning_envelope.sustain_end;
    // Writing Pitch Envelope
    if (pIns->pitch_envelope.flags & ENV_ENABLED) iti->pitchenv.flags |= 0x01;
    if (pIns->pitch_envelope.flags & ENV_LOOP) iti->pitchenv.flags |= 0x02;
    if (pIns->pitch_envelope.flags & ENV_SUSTAIN) iti->pitchenv.flags |= 0x04;
    if (pIns->pitch_envelope.flags & ENV_CARRY) iti->pitchenv.flags |= 0x08;
    if (pIns->pitch_envelope.flags & ENV_FILTER) iti->pitchenv.flags |= 0x80;
    iti->pitchenv.num = (uint8_t)pIns->pitch_envelope.num_nodes;
    iti->pitchenv.lpb = (uint8_t)pIns->pitch_envelope.loop_start;
    iti->pitchenv.lpe = (uint8_t)pIns->pitch_envelope.loop_end;
    iti->pitchenv.slb = (uint8_t)pIns->pitch_envelope.sustain_start;
    iti->pitchenv.sle = (uint8_t)pIns->pitch_envelope.sustain_end;
    // Writing Envelopes data
    for (UINT ev=0; ev<25; ev++)
    {
        iti->volenv.data[ev*3] = pIns->volume_envelope.Values[ev];
        iti->volenv.data[ev*3+1] = pIns->volume_envelope.Ticks[ev] & 0xFF;
        iti->volenv.data[ev*3+2] = pIns->volume_envelope.Ticks[ev] >> 8;
        iti->panenv.data[ev*3] = pIns->panning_envelope.Values[ev] - 32;
        iti->panenv.data[ev*3+1] = pIns->panning_envelope.Ticks[ev] & 0xFF;
        iti->panenv.data[ev*3+2] = pIns->panning_envelope.Ticks[ev] >> 8;
        iti->pitchenv.data[ev*3] = pIns->pitch_envelope.Values[ev] - 32;
        iti->pitchenv.data[ev*3+1] = pIns->pitch_envelope.Ticks[ev] & 0xFF;
        iti->pitchenv.data[ev*3+2] = pIns->pitch_envelope.Ticks[ev] >> 8;
    }
    dwPos = 554;
    fwrite(buffer, 1, dwPos, f);
    dwPos += iti->nos * sizeof(ITSAMPLESTRUCT);

    // Writing sample headers
    for (UINT j=0; j<iti->nos; j++) if (smptable[j])
    {
        UINT smpsize = 0;
        UINT nsmp = smptable[j];
        memset(&itss, 0, sizeof(itss));
        modplug::tracker::modsample_t *psmp = &Samples[nsmp];
        itss.id = LittleEndian(IT_IMPS);
        memcpy(itss.filename, psmp->legacy_filename, 12);
        memcpy(itss.name, m_szNames[nsmp], 26);
        itss.gvl = (uint8_t)psmp->global_volume;
        itss.flags = 0x01;
        if (psmp->flags & CHN_LOOP) itss.flags |= 0x10;
        if (psmp->flags & CHN_SUSTAINLOOP) itss.flags |= 0x20;
        if (psmp->flags & CHN_PINGPONGLOOP) itss.flags |= 0x40;
        if (psmp->flags & CHN_PINGPONGSUSTAIN) itss.flags |= 0x80;
        itss.C5Speed = psmp->c5_samplerate;
        if (!itss.C5Speed) itss.C5Speed = 8363;
        itss.length = psmp->length;
        itss.loopbegin = psmp->loop_start;
        itss.loopend = psmp->loop_end;
        itss.susloopbegin = psmp->sustain_start;
        itss.susloopend = psmp->sustain_end;
        itss.vol = psmp->default_volume >> 2;
        itss.dfp = psmp->default_pan >> 2;
        itss.vit = autovibxm2it[psmp->vibrato_type & 7];
        itss.vir = min(psmp->vibrato_sweep, 255);
        itss.vid = min(psmp->vibrato_depth, 32);
        itss.vis = min(psmp->vibrato_rate, 64);
        if (psmp->flags & CHN_PANNING) itss.dfp |= 0x80;
        itss.cvt = 0x01;
        smpsize = psmp->length;
        if (psmp->flags & CHN_16BIT)
        {
            itss.flags |= 0x02; 
            smpsize <<= 1;
        } else
        {
            itss.flags &= ~(0x02);
        }
        //rewbs.enableStereoITI
        if (psmp->flags & CHN_STEREO)
        {
            itss.flags |= 0x04;
            smpsize <<= 1; 
        } else
        {
            itss.flags &= ~(0x04);
        }
        //end rewbs.enableStereoITI
        itss.samplepointer = dwPos;
        fwrite(&itss, 1, sizeof(ITSAMPLESTRUCT), f);
        dwPos += smpsize;
    }
    // Writing Sample Data
    //rewbs.enableStereoITI
    uint16_t sampleType=0;
    if (itss.flags | 0x02) sampleType=RS_PCM16S; else sampleType=RS_PCM8S;    //8 or 16 bit signed
    if (itss.flags | 0x04) sampleType |= RSF_STEREO;    					//mono or stereo
    for (UINT k=0; k<iti->nos; k++)
    {
//rewbs.enableStereoITI - using eric's code as it is better here.
// -> CODE#0001
// -> DESC="enable saving stereo ITI"
        //UINT nsmp = smptable[k];
        //modplug::tracker::modinstrument_t *psmp = &Ins[nsmp];
        //WriteSample(f, psmp, (psmp->flags & CHN_16BIT) ? RS_PCM16S : RS_PCM8S);
        modplug::tracker::modsample_t *pSmp = &Samples[smptable[k]];
        UINT smpflags = (pSmp->flags & CHN_16BIT) ? RS_PCM16S : RS_PCM8S;
        if (pSmp->flags & CHN_STEREO) smpflags = (pSmp->flags & CHN_16BIT) ? RS_STPCM16S : RS_STPCM8S;
        WriteSample(f, pSmp, smpflags);
// -! BUG_FIX#0001
    }

    __int32 code = 'MPTX';
    fwrite(&code, 1, sizeof(__int32), f);    // Write extension tag
    WriteInstrumentHeaderStruct(pIns, f);    // Write full extended header.

    fclose(f);
    return true;
}



bool IsValidSizeField(const uint8_t * const pData, const uint8_t * const pEnd, const int16_t size)
//------------------------------------------------------------------------------
{
    if(size < 0 || (uintptr_t)(pEnd - pData) < (uintptr_t)size)
        return false;
    else 
        return true;
}


void ReadInstrumentExtensionField(modplug::tracker::modinstrument_t* pIns, const uint8_t *& ptr, const int32_t code, const int16_t size)
//------------------------------------------------------------------------------------------------------------
{
    // get field's address in instrument's header
    uint8_t* fadr = GetInstrumentHeaderFieldPointer(pIns, code, size);
     
    if(fadr && code != 'K[..')    // copy field data in instrument's header
        memcpy(fadr,ptr,size);  // (except for keyboard mapping)
    ptr += size;    			// jump field

    if (code == 'dF..' && fadr != nullptr) // 'dF..' field requires additional processing.
        ConvertReadExtendedFlags(pIns);
}


void ReadExtendedInstrumentProperty(modplug::tracker::modinstrument_t* pIns, const int32_t code, const uint8_t *& pData, const uint8_t * const pEnd)
//---------------------------------------------------------------------------------------------------------------
{
    if(pEnd < pData || uintptr_t(pEnd - pData) < 2)
        return;

    int16_t size;
    memcpy(&size, pData, sizeof(size)); // read field size
    pData += sizeof(size);    			// jump field size

    if(IsValidSizeField(pData, pEnd, size) == false)
        return;

    ReadInstrumentExtensionField(pIns, pData, code, size);
}


void ReadExtendedInstrumentProperties(modplug::tracker::modinstrument_t* pIns, const uint8_t * const pDataStart, const size_t nMemLength)
//--------------------------------------------------------------------------------------------------------------
{
    if(pIns == 0 || pDataStart == 0 || nMemLength < 4)
        return;

    const uint8_t * pData = pDataStart;
    const uint8_t * const pEnd = pDataStart + nMemLength;

    int32_t code;
    memcpy(&code, pData, sizeof(code));

    // Seek for supported extended settings header
    if( code == 'MPTX' )
    {
        pData += sizeof(code); // jump extension header code

        while( (uintptr_t)(pData - pDataStart) <= nMemLength - 4)
        {
            memcpy(&code, pData, sizeof(code)); // read field code
            pData += sizeof(code);    			 // jump field code
            ReadExtendedInstrumentProperty(pIns, code, pData, pEnd);
        }
    }
}


void ConvertReadExtendedFlags(modplug::tracker::modinstrument_t *pIns)
//------------------------------------------------
{
    const DWORD dwOldFlags = pIns->flags;
    pIns->flags = pIns->volume_envelope.flags = pIns->panning_envelope.flags = pIns->pitch_envelope.flags = 0;
    if(dwOldFlags & dFdd_VOLUME)    	pIns->volume_envelope.flags |= ENV_ENABLED;
    if(dwOldFlags & dFdd_VOLSUSTAIN)    pIns->volume_envelope.flags |= ENV_SUSTAIN;
    if(dwOldFlags & dFdd_VOLLOOP)    	pIns->volume_envelope.flags |= ENV_LOOP;
    if(dwOldFlags & dFdd_PANNING)    	pIns->panning_envelope.flags |= ENV_ENABLED;
    if(dwOldFlags & dFdd_PANSUSTAIN)    pIns->panning_envelope.flags |= ENV_SUSTAIN;
    if(dwOldFlags & dFdd_PANLOOP)    	pIns->panning_envelope.flags |= ENV_LOOP;
    if(dwOldFlags & dFdd_PITCH)    		pIns->pitch_envelope.flags |= ENV_ENABLED;
    if(dwOldFlags & dFdd_PITCHSUSTAIN)    pIns->pitch_envelope.flags |= ENV_SUSTAIN;
    if(dwOldFlags & dFdd_PITCHLOOP)    	pIns->pitch_envelope.flags |= ENV_LOOP;
    if(dwOldFlags & dFdd_SETPANNING)    pIns->flags |= INS_SETPANNING;
    if(dwOldFlags & dFdd_FILTER)    	pIns->pitch_envelope.flags |= ENV_FILTER;
    if(dwOldFlags & dFdd_VOLCARRY)    	pIns->volume_envelope.flags |= ENV_CARRY;
    if(dwOldFlags & dFdd_PANCARRY)    	pIns->panning_envelope.flags |= ENV_CARRY;
    if(dwOldFlags & dFdd_PITCHCARRY)    pIns->pitch_envelope.flags |= ENV_CARRY;
    if(dwOldFlags & dFdd_MUTE)    		pIns->flags |= INS_MUTE;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// 8SVX Samples

#define IFFID_8SVX    0x58565338
#define IFFID_VHDR    0x52444856
#define IFFID_BODY    0x59444f42
#define IFFID_NAME    0x454d414e
#define IFFID_ANNO    0x4f4e4e41

#pragma pack(1)

typedef struct IFF8SVXFILEHEADER
{
    DWORD dwFORM;    // "FORM"
    DWORD dwSize;
    DWORD dw8SVX;    // "8SVX"
} IFF8SVXFILEHEADER;

typedef struct IFFVHDR
{
    DWORD dwVHDR;    // "VHDR"
    DWORD dwSize;
    ULONG oneShotHiSamples,    		/* # samples in the high octave 1-shot part */
            repeatHiSamples,    	/* # samples in the high octave repeat part */
            samplesPerHiCycle;    	/* # samples/cycle in high octave, else 0 */
    uint16_t samplesPerSec;    			/* data sampling rate */
    uint8_t    ctOctave,				/* # octaves of waveforms */
            sCompression;    		/* data compression technique used */
    DWORD Volume;
} IFFVHDR;

typedef struct IFFBODY
{
    DWORD dwBody;
    DWORD dwSize;
} IFFBODY;


#pragma pack()



bool CSoundFile::Read8SVXSample(UINT nSample, LPBYTE lpMemFile, DWORD dwFileLength)
//---------------------------------------------------------------------------------
{
    IFF8SVXFILEHEADER *pfh = (IFF8SVXFILEHEADER *)lpMemFile;
    IFFVHDR *pvh = (IFFVHDR *)(lpMemFile + 12);
    modplug::tracker::modsample_t *pSmp = &Samples[nSample];
    DWORD dwMemPos = 12;
    
    if ((!lpMemFile) || (dwFileLength < sizeof(IFFVHDR)+12) || (pfh->dwFORM != IFFID_FORM)
     || (pfh->dw8SVX != IFFID_8SVX) || (BigEndian(pfh->dwSize) >= dwFileLength)
     || (pvh->dwVHDR != IFFID_VHDR) || (BigEndian(pvh->dwSize) >= dwFileLength)) return false;
    DestroySample(nSample);
    // Default values
    pSmp->global_volume = 64;
    pSmp->default_pan = 128;
    pSmp->length = 0;
    pSmp->loop_start = BigEndian(pvh->oneShotHiSamples);
    pSmp->loop_end = pSmp->loop_start + BigEndian(pvh->repeatHiSamples);
    pSmp->sustain_start = 0;
    pSmp->sustain_end = 0;
    pSmp->flags = 0;
    pSmp->default_volume = (uint16_t)(BigEndianW((uint16_t)pvh->Volume) >> 8);
    pSmp->c5_samplerate = BigEndianW(pvh->samplesPerSec);
    pSmp->legacy_filename[0] = 0;
    if ((!pSmp->default_volume) || (pSmp->default_volume > 256)) pSmp->default_volume = 256;
    if (!pSmp->c5_samplerate) pSmp->c5_samplerate = 22050;
    pSmp->RelativeTone = 0;
    pSmp->nFineTune = 0;
    if (m_nType & MOD_TYPE_XM) FrequencyToTranspose(pSmp);
    dwMemPos += BigEndian(pvh->dwSize) + 8;
    while (dwMemPos + 8 < dwFileLength)
    {
        DWORD dwChunkId = *((LPDWORD)(lpMemFile+dwMemPos));
        DWORD dwChunkLen = BigEndian(*((LPDWORD)(lpMemFile+dwMemPos+4)));
        LPBYTE pChunkData = (LPBYTE)(lpMemFile+dwMemPos+8);
        // Hack for broken files: Trim claimed length if it's too long
        dwChunkLen = min(dwChunkLen, dwFileLength - dwMemPos);
        //if (dwChunkLen > dwFileLength - dwMemPos) break;
        switch(dwChunkId)
        {
        case IFFID_NAME:
            {
                const UINT len = min(dwChunkLen, MAX_SAMPLENAME - 1);
                MemsetZero(m_szNames[nSample]);
                memcpy(m_szNames[nSample], pChunkData, len);
            }
            break;
        case IFFID_BODY:
            if (!pSmp->sample_data)
            {
                UINT len = dwChunkLen;
                if (len > dwFileLength - dwMemPos - 8) len = dwFileLength - dwMemPos - 8;
                if (len > 4)
                {
                    pSmp->length = len;
                    if ((pSmp->loop_start + 4 < pSmp->loop_end) && (pSmp->loop_end < pSmp->length)) pSmp->flags |= CHN_LOOP;
                    ReadSample(pSmp, RS_PCM8S, (LPSTR)(pChunkData), len);
                }
            }
            break;
        }
        dwMemPos += dwChunkLen + 8;
    }
    return (pSmp->sample_data != nullptr);
}

