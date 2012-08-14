/*
 * Load_itp.cpp
 * ------------
 * Purpose: Load and save Impulse Tracker Project (ITP) files.
 * Notes  : Despite its name, ITP is not a format supported by Impulse Tracker.
 *          In fact, it's a format invented by the OpenMPT team to allow people to work
 *          with the IT format, but keeping the instrument files with big samples separate
 *          from the pattern data, to keep the work files small and handy.
 *          The current design of the format is quite flawed, though, so expect this to
 *          change in the (far?) future.
 * Authors: OpenMPT Devs
 *
 */

#include "stdafx.h"
#include "Loaders.h"
#include "IT_DEFS.H"
#include "../mptrack.h"
#include "../version.h"


#define ITP_VERSION 0x00000102    // v1.02
#define ITP_FILE_ID 0x2e697470    // .itp ASCII


bool module_renderer::ReadITProject(const uint8_t * lpStream, const uint32_t dwMemLength)
//-----------------------------------------------------------------------
{
    UINT i,n,nsmp;
    uint32_t id,len,size;
    uint32_t dwMemPos = 0;
    uint32_t version;

    ASSERT_CAN_READ(12);

    // Check file ID

    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    if(id != ITP_FILE_ID) return false;
    dwMemPos += sizeof(uint32_t);

    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    version = id;
    dwMemPos += sizeof(uint32_t);

    // max supported version
    if(version > ITP_VERSION)
    {
        return false;
    }

    m_nType = MOD_TYPE_IT;

    // Song name

    // name string length
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    len = id;
    dwMemPos += sizeof(uint32_t);

    // name string
    ASSERT_CAN_READ(len);
    if (len <= MAX_SAMPLENAME)
    {
        assign_without_padding(this->song_name, reinterpret_cast<const char *>(lpStream + dwMemPos), len);
        dwMemPos += len;
    }
    else return false;

    // Song comments

    // comment string length
    ASSERT_CAN_READ(4);
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    dwMemPos += sizeof(uint32_t);
    if(id > UINT16_MAX) return false;

    // allocate and copy comment string
    ASSERT_CAN_READ(id);
    if(id > 0)
    {
        ReadMessage(lpStream + dwMemPos, id - 1, leCR);
    }
    dwMemPos += id;

    // Song global config
    ASSERT_CAN_READ(5*4);

    // m_dwSongFlags
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    m_dwSongFlags = (id & SONG_FILE_FLAGS);
    dwMemPos += sizeof(uint32_t);

    if(!(m_dwSongFlags & SONG_ITPROJECT)) return false;

    // m_nDefaultGlobalVolume
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    m_nDefaultGlobalVolume = id;
    dwMemPos += sizeof(uint32_t);

    // m_nSamplePreAmp
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    m_nSamplePreAmp = id;
    dwMemPos += sizeof(uint32_t);

    // m_nDefaultSpeed
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    m_nDefaultSpeed = id;
    dwMemPos += sizeof(uint32_t);

    // m_nDefaultTempo
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    m_nDefaultTempo = id;
    dwMemPos += sizeof(uint32_t);

    // Song channels data
    ASSERT_CAN_READ(2*4);

    // m_nChannels
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    m_nChannels = (modplug::tracker::chnindex_t)id;
    dwMemPos += sizeof(uint32_t);
    if(m_nChannels > 127) return false;

    // channel name string length (=MAX_CHANNELNAME)
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    len = id;
    dwMemPos += sizeof(uint32_t);
    if(len > MAX_CHANNELNAME) return false;

    // Channels' data
    for(i=0; i<m_nChannels; i++){
        ASSERT_CAN_READ(3*4 + len);

        // ChnSettings[i].nPan
        memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
        ChnSettings[i].nPan = id;
        dwMemPos += sizeof(uint32_t);

        // ChnSettings[i].dwFlags
        memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
        ChnSettings[i].dwFlags = id;
        dwMemPos += sizeof(uint32_t);

        // ChnSettings[i].nVolume
        memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
        ChnSettings[i].nVolume = id;
        dwMemPos += sizeof(uint32_t);

        // ChnSettings[i].szName
        memcpy(&ChnSettings[i].szName[0],lpStream+dwMemPos,len);
        SetNullTerminator(ChnSettings[i].szName);
        dwMemPos += len;
    }

    // Song mix plugins
    // size of mix plugins data
    ASSERT_CAN_READ(4);
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    dwMemPos += sizeof(uint32_t);

    // mix plugins
    ASSERT_CAN_READ(id);
    dwMemPos += LoadMixPlugins(lpStream+dwMemPos, id);

    // Song midi config

    // midi cfg data length
    ASSERT_CAN_READ(4);
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    dwMemPos += sizeof(uint32_t);

    // midi cfg
    ASSERT_CAN_READ(id);
    if (id <= sizeof(m_MidiCfg))
    {
        memcpy(&m_MidiCfg, lpStream + dwMemPos, id);
        SanitizeMacros();
        dwMemPos += id;
    }

    // Song Instruments

    // m_nInstruments
    ASSERT_CAN_READ(4);
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    m_nInstruments = (modplug::tracker::instrumentindex_t)id;
    if(m_nInstruments > MAX_INSTRUMENTS) return false;
    dwMemPos += sizeof(uint32_t);

    // path string length (=_MAX_PATH)
    ASSERT_CAN_READ(4);
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    len = id;
    if(len > _MAX_PATH) return false;
    dwMemPos += sizeof(uint32_t);

    // instruments' paths
    for(i=0; i<m_nInstruments; i++){
        ASSERT_CAN_READ(len);
        memcpy(&m_szInstrumentPath[i][0],lpStream+dwMemPos,len);
        SetNullTerminator(m_szInstrumentPath[i]);
        dwMemPos += len;
    }

    // Song Orders

    // size of order array (=MAX_ORDERS)
    ASSERT_CAN_READ(4);
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    size = id;
    if(size > MAX_ORDERS) return false;
    dwMemPos += sizeof(uint32_t);

    // order data
    ASSERT_CAN_READ(size);
    Order.ReadAsByte(lpStream+dwMemPos, size, dwMemLength-dwMemPos);
    dwMemPos += size;



    // Song Patterns

    ASSERT_CAN_READ(3*4);
    // number of patterns (=MAX_PATTERNS)
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    size = id;
    dwMemPos += sizeof(uint32_t);
    if(size > MAX_PATTERNS) return false;

    // m_nPatternNames
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    const modplug::tracker::patternindex_t numNamedPats = id;
    dwMemPos += sizeof(uint32_t);

    // pattern name string length (=MAX_PATTERNNAME)
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    const uint32_t patNameLen = id;
    dwMemPos += sizeof(uint32_t);

    // m_lpszPatternNames
    ASSERT_CAN_READ(numNamedPats * patNameLen);
    char *patNames = (char *)(lpStream + dwMemPos);
    dwMemPos += numNamedPats * patNameLen;

    // modcommand data length
    ASSERT_CAN_READ(4);
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    n = id;
    if(n != 6) return false;
    dwMemPos += sizeof(uint32_t);

    for(modplug::tracker::patternindex_t npat=0; npat<size; npat++)
    {
        // Patterns[npat].GetNumRows()
        ASSERT_CAN_READ(4);
        memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
        if(id > MAX_PATTERN_ROWS) return false;
        const modplug::tracker::rowindex_t nRows = id;
        dwMemPos += sizeof(uint32_t);

        // Try to allocate & read only sized patterns
        if(nRows)
        {

            // Allocate pattern
            if(Patterns.Insert(npat, nRows))
            {
                dwMemPos += m_nChannels * Patterns[npat].GetNumRows() * n;
                continue;
            }
            if(npat < numNamedPats && patNameLen > 0)
            {
                Patterns[npat].SetName(patNames, patNameLen);
                patNames += patNameLen;
            }

            // Pattern data
            long datasize = m_nChannels * Patterns[npat].GetNumRows() * n;
            //if (streamPos+datasize<=dwMemLength) {
            if(Patterns[npat].ReadITPdata(lpStream, dwMemPos, datasize, dwMemLength))
            {
                ErrorBox(IDS_ERR_FILEOPEN, NULL);
                return false;
            }
            //memcpy(Patterns[npat],lpStream+streamPos,datasize);
            //streamPos += datasize;
            //}
        }
    }

    // Load embeded samples

    ITSAMPLESTRUCT pis;

    // Read original number of samples
    ASSERT_CAN_READ(4);
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    if(id > MAX_SAMPLES) return false;
    m_nSamples = (modplug::tracker::sampleindex_t)id;
    dwMemPos += sizeof(uint32_t);

    // Read number of embeded samples
    ASSERT_CAN_READ(4);
    memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
    if(id > MAX_SAMPLES) return false;
    n = id;
    dwMemPos += sizeof(uint32_t);

    // Read samples
    for(i=0; i<n; i++){

        ASSERT_CAN_READ(4 + sizeof(ITSAMPLESTRUCT) + 4);

        // Sample id number
        memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
        nsmp = id;
        dwMemPos += sizeof(uint32_t);

        if(nsmp < 1 || nsmp >= MAX_SAMPLES)
            return false;

        // Sample struct
        memcpy(&pis,lpStream+dwMemPos,sizeof(ITSAMPLESTRUCT));
        dwMemPos += sizeof(ITSAMPLESTRUCT);

        // Sample length
        memcpy(&id,lpStream+dwMemPos,sizeof(uint32_t));
        len = id;
        dwMemPos += sizeof(uint32_t);
        if(dwMemPos >= dwMemLength || len > dwMemLength - dwMemPos) return false;

        // Copy sample struct data (ut-oh... this code looks very familiar!)
        if(pis.id == LittleEndian(IT_IMPS))
        {
            modsample_t *pSmp = &Samples[nsmp];
            memcpy(pSmp->legacy_filename, pis.filename, 12);
            pSmp->flags = 0;
            pSmp->length = 0;
            pSmp->loop_start = pis.loopbegin;
            pSmp->loop_end = pis.loopend;
            pSmp->sustain_start = pis.susloopbegin;
            pSmp->sustain_end = pis.susloopend;
            pSmp->c5_samplerate = pis.C5Speed;
            if(!pSmp->c5_samplerate) pSmp->c5_samplerate = 8363;
            if(pis.C5Speed < 256) pSmp->c5_samplerate = 256;
            pSmp->default_volume = pis.vol << 2;
            if(pSmp->default_volume > 256) pSmp->default_volume = 256;
            pSmp->global_volume = pis.gvl;
            if(pSmp->global_volume > 64) pSmp->global_volume = 64;
            if(pis.flags & 0x10) pSmp->flags |= CHN_LOOP;
            if(pis.flags & 0x20) pSmp->flags |= CHN_SUSTAINLOOP;
            if(pis.flags & 0x40) pSmp->flags |= CHN_PINGPONGLOOP;
            if(pis.flags & 0x80) pSmp->flags |= CHN_PINGPONGSUSTAIN;
            pSmp->default_pan = (pis.dfp & 0x7F) << 2;
            if(pSmp->default_pan > 256) pSmp->default_pan = 256;
            if(pis.dfp & 0x80) pSmp->flags |= CHN_PANNING;
            pSmp->vibrato_type = autovibit2xm[pis.vit & 7];
            pSmp->vibrato_rate = pis.vis;
            pSmp->vibrato_depth = pis.vid & 0x7F;
            pSmp->vibrato_sweep = pis.vir;
            if(pis.length){
                pSmp->length = pis.length;
                if (pSmp->length > MAX_SAMPLE_LENGTH) pSmp->length = MAX_SAMPLE_LENGTH;
                UINT flags = (pis.cvt & 1) ? RS_PCM8S : RS_PCM8U;
                if (pis.flags & 2){
                    flags += 5;
                    if (pis.flags & 4) flags |= RSF_STEREO;
                    pSmp->flags |= CHN_16BIT;
                }
                else{
                    if (pis.flags & 4) flags |= RSF_STEREO;
                }
                // Read sample data
                ReadSample(&Samples[nsmp], flags, (LPSTR)(lpStream+dwMemPos), len);
                dwMemPos += len;
                memcpy(m_szNames[nsmp], pis.name, 26);
            }
        }
    }

    // Load instruments

    CMappedFile f;
    LPBYTE lpFile;

    for(modplug::tracker::instrumentindex_t i = 0; i < m_nInstruments; i++)
    {

        if(m_szInstrumentPath[i][0] == '\0' || !f.Open(m_szInstrumentPath[i])) continue;

        len = f.GetLength();
        lpFile = f.Lock(len);
        if(!lpFile) { f.Close(); continue; }

        ReadInstrumentFromFile(i+1, lpFile, len);
        f.Unlock();
        f.Close();
    }

    // Extra info data

    __int32 fcode = 0;
    const uint8_t * ptr = lpStream + min(dwMemPos, dwMemLength);

    if (dwMemPos <= dwMemLength - 4) {
        fcode = (*((__int32 *)ptr));
    }

    // Embed instruments' header [v1.01]
    if(version >= 0x00000101 && m_dwSongFlags & SONG_ITPEMBEDIH && fcode == 'EBIH')
    {
        // jump embeded instrument header tag
        ptr += sizeof(__int32);

        // set first instrument's header as current
        i = 1;

        // parse file
        while( uintptr_t(ptr - lpStream) <= dwMemLength - 4 && i <= m_nInstruments )
        {

            fcode = (*((__int32 *)ptr));                    // read field code

            switch( fcode )
            {
            case 'MPTS': goto mpts; //:)            // reached end of instrument headers
            case 'SEP@': case 'MPTX':
                ptr += sizeof(__int32);                    // jump code
                i++;                                                    // switch to next instrument
                break;

            default:
                ptr += sizeof(__int32);                    // jump field code
                ReadExtendedInstrumentProperty(Instruments[i], fcode, ptr, lpStream + dwMemLength);
                break;
            }
        }
    }

    //HACK: if we fail on i <= m_nInstruments above, arrive here without having set fcode as appropriate,
    //      hence the code duplication.
    if ( (uintptr_t)(ptr - lpStream) <= dwMemLength - 4 )
    {
        fcode = (*((__int32 *)ptr));
    }

    // Song extensions
mpts:
    if( fcode == 'MPTS' )
        LoadExtendedSongProperties(MOD_TYPE_IT, ptr, lpStream, dwMemLength);

    m_nMaxPeriod = 0xF000;
    m_nMinPeriod = 8;

    if(m_dwLastSavedWithVersion < MAKE_VERSION_NUMERIC(1, 17, 2, 50))
    {
        SetModFlag(MSF_COMPATIBLE_PLAY, false);
        SetModFlag(MSF_MIDICC_BUGEMULATION, true);
        SetModFlag(MSF_OLDVOLSWING, true);
    }

    return true;
}


#ifndef MODPLUG_NO_FILESAVE

bool module_renderer::SaveITProject(LPCSTR lpszFileName)
//-------------------------------------------------
{
    // Check song type

    if(!(m_dwSongFlags & SONG_ITPROJECT)) return false;

    UINT i,j = 0;
    for(i = 0 ; i < m_nInstruments ; i++) { if(m_szInstrumentPath[i][0] != '\0' || !Instruments[i+1]) j++; }
    if(m_nInstruments && j != m_nInstruments) return false;

    // Open file

    FILE *f;

    if((!lpszFileName) || ((f = fopen(lpszFileName, "wb")) == NULL)) return false;


    // File ID

    uint32_t id = ITP_FILE_ID;
    fwrite(&id, 1, sizeof(id), f);

    id = ITP_VERSION;
    fwrite(&id, 1, sizeof(id), f);

    // Song name

    // name string length
    id = 27;
    fwrite(&id, 1, sizeof(id), f);

    // song name
    char namebuf[27];
    copy_with_padding(namebuf, 27, this->song_name);
    fwrite(namebuf, 1, 27, f);

    // Song comments

    // comment string length
    id = m_lpszSongComments ? strlen(m_lpszSongComments)+1 : 0;
    fwrite(&id, 1, sizeof(id), f);

    // comment string
    if(m_lpszSongComments) fwrite(&m_lpszSongComments[0], 1, strlen(m_lpszSongComments)+1, f);

    // Song global config

    id = (m_dwSongFlags & SONG_FILE_FLAGS);
    fwrite(&id, 1, sizeof(id), f);
    id = m_nDefaultGlobalVolume;
    fwrite(&id, 1, sizeof(id), f);
    id = m_nSamplePreAmp;
    fwrite(&id, 1, sizeof(id), f);
    id = m_nDefaultSpeed;
    fwrite(&id, 1, sizeof(id), f);
    id = m_nDefaultTempo;
    fwrite(&id, 1, sizeof(id), f);

    // Song channels data

    // number of channels
    id = m_nChannels;
    fwrite(&id, 1, sizeof(id), f);

    // channel name string length
    id = MAX_CHANNELNAME;
    fwrite(&id, 1, sizeof(id), f);

    // channel config data
    for(i=0; i<m_nChannels; i++){
        id = ChnSettings[i].nPan;
        fwrite(&id, 1, sizeof(id), f);
        id = ChnSettings[i].dwFlags;
        fwrite(&id, 1, sizeof(id), f);
        id = ChnSettings[i].nVolume;
        fwrite(&id, 1, sizeof(id), f);
        fwrite(&ChnSettings[i].szName[0], 1, MAX_CHANNELNAME, f);
    }

    // Song mix plugins

    // mix plugins data length
    id = SaveMixPlugins(NULL, TRUE);
    fwrite(&id, 1, sizeof(id), f);

    // mix plugins data
    SaveMixPlugins(f, FALSE);

    // Song midi config

    // midi cfg data length
    id = sizeof(MODMIDICFG);
    fwrite(&id, 1, sizeof(id), f);

    // midi cfg
    fwrite(&m_MidiCfg, 1, sizeof(MODMIDICFG), f);

    // Song Instruments

    // number of instruments
    id = m_nInstruments;
    fwrite(&id, 1, sizeof(id), f);

    // path name string length
    id = _MAX_PATH;
    fwrite(&id, 1, sizeof(id), f);

    // instruments' path
    for(i=0; i<m_nInstruments; i++) fwrite(&m_szInstrumentPath[i][0], 1, _MAX_PATH, f);

    // Song Orders

    // order array size
    id = Order.size();
    fwrite(&id, 1, sizeof(id), f);

    // order array
    Order.WriteAsByte(f, id);

    // Song Patterns

    // number of patterns
    id = MAX_PATTERNS;
    fwrite(&id, 1, sizeof(id), f);

    // number of pattern name strings
    modplug::tracker::patternindex_t numNamedPats = Patterns.GetNumNamedPatterns();
    numNamedPats = min(numNamedPats, MAX_PATTERNS);
    id = numNamedPats;
    fwrite(&id, 1, sizeof(id), f);

    // length of a pattern name string
    id = MAX_PATTERNNAME;
    fwrite(&id, 1, sizeof(id), f);
    // pattern name string
    for(modplug::tracker::patternindex_t nPat = 0; nPat < numNamedPats; nPat++)
    {
        char name[MAX_PATTERNNAME];
        MemsetZero(name);
        Patterns[nPat].GetName(name, MAX_PATTERNNAME);
        fwrite(name, 1, MAX_PATTERNNAME, f);
    }

    // modcommand data length
    id = sizeof(modplug::tracker::modevent_t);
    fwrite(&id, 1, sizeof(id), f);

    // patterns data content
    for(UINT npat=0; npat<MAX_PATTERNS; npat++){
        // pattern size (number of rows)
        id = Patterns[npat] ? Patterns[npat].GetNumRows() : 0;
        fwrite(&id, 1, sizeof(id), f);
        // pattern data
        if(Patterns[npat] && Patterns[npat].GetNumRows()) Patterns[npat].WriteITPdata(f);
        //fwrite(Patterns[npat], 1, m_nChannels * Patterns[npat].GetNumRows() * sizeof(modplug::tracker::modcommand_t), f);
    }

    // Song lonely (instrument-less) samples

    // Write original number of samples
    id = m_nSamples;
    fwrite(&id, 1, sizeof(id), f);

    vector<bool> sampleUsed(m_nSamples, false);

    // Mark samples used in instruments
    for(i=0; i<m_nInstruments; i++)
    {
        if(Instruments[i + 1] != nullptr)
        {
            modinstrument_t *p = Instruments[i + 1];
            for(j = 0; j < 128; j++)
            {
                if(p->Keyboard[j] > 0 && p->Keyboard[j] <= m_nSamples)
                    sampleUsed[p->Keyboard[j] - 1] = true;
            }
        }
    }

    // Count samples not used in any instrument
    i = 0;
    for(j = 1; j <= m_nSamples; j++)
        if(!sampleUsed[j - 1] && Samples[j].sample_data) i++;

    id = i;
    fwrite(&id, 1, sizeof(id), f);

    // Write samples not used in any instrument (help, this looks like duplicate code!)
    ITSAMPLESTRUCT itss;
    for(UINT nsmp=1; nsmp<=m_nSamples; nsmp++)
    {
        if(!sampleUsed[nsmp - 1] && Samples[nsmp].sample_data)
        {

            modsample_t *psmp = &Samples[nsmp];
            memset(&itss, 0, sizeof(itss));
            memcpy(itss.filename, psmp->legacy_filename, 12);
            memcpy(itss.name, m_szNames[nsmp], 26);

            itss.id = LittleEndian(IT_IMPS);
            itss.gvl = (uint8_t)psmp->global_volume;
            itss.flags = 0x00;

            if(psmp->flags & CHN_LOOP) itss.flags |= 0x10;
            if(psmp->flags & CHN_SUSTAINLOOP) itss.flags |= 0x20;
            if(psmp->flags & CHN_PINGPONGLOOP) itss.flags |= 0x40;
            if(psmp->flags & CHN_PINGPONGSUSTAIN) itss.flags |= 0x80;
            itss.C5Speed = psmp->c5_samplerate;
            if (!itss.C5Speed) itss.C5Speed = 8363;
            itss.length = psmp->length;
            itss.loopbegin = psmp->loop_start;
            itss.loopend = psmp->loop_end;
            itss.susloopbegin = psmp->sustain_start;
            itss.susloopend = psmp->sustain_end;
            itss.vol = (uint8_t)(psmp->default_volume >> 2);
            itss.dfp = (uint8_t)(psmp->default_pan >> 2);
            itss.vit = autovibxm2it[psmp->vibrato_type & 7];
            itss.vis = min(psmp->vibrato_rate, 64);
            itss.vid = min(psmp->vibrato_depth, 32);
            itss.vir = min(psmp->vibrato_sweep, 255); //(psmp->vibrato_sweep < 64) ? psmp->vibrato_sweep * 4 : 255;
            if (psmp->flags & CHN_PANNING) itss.dfp |= 0x80;
            if ((psmp->sample_data) && (psmp->length)) itss.cvt = 0x01;
            UINT flags = RS_PCM8S;

            if(psmp->flags & CHN_STEREO)
            {
                flags = RS_STPCM8S;
                itss.flags |= 0x04;
            }
            if(psmp->flags & CHN_16BIT)
            {
                itss.flags |= 0x02;
                flags = (psmp->flags & CHN_STEREO) ? RS_STPCM16S : RS_PCM16S;
            }

            id = nsmp;
            fwrite(&id, 1, sizeof(id), f);

            itss.samplepointer = NULL;
            fwrite(&itss, 1, sizeof(ITSAMPLESTRUCT), f);

            id = WriteSample(NULL, psmp, flags);
            fwrite(&id, 1, sizeof(id), f);
            WriteSample(f, psmp, flags);
        }
    }

    // Embed instruments' header [v1.01]

    if(m_dwSongFlags & SONG_ITPEMBEDIH)
    {
        // embeded instrument header tag
        __int32 code = 'EBIH';
        fwrite(&code, 1, sizeof(__int32), f);

        // instruments' header
        for(i=0; i<m_nInstruments; i++)
        {
            if(Instruments[i+1]) WriteInstrumentHeaderStruct(Instruments[i+1], f);
            // write separator tag
            code = 'SEP@';
            fwrite(&code, 1, sizeof(__int32), f);
        }
    }

    SaveExtendedSongProperties(f);

    // Close file
    fclose(f);
    return true;
}

#endif