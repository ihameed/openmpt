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
#include "../Moddoc.h"
#endif //MODPLUG_TRACKER
#include "Wav.h"

#include "misc.h"

using namespace modplug::tracker;

using namespace modplug::legacy_soundlib;

#pragma warning(disable:4244)
// -> CODE#0003
// -> DESC="remove instrument's samples"
// BOOL CSoundFile::DestroyInstrument(UINT nInstr)
bool module_renderer::DestroyInstrument(modplug::tracker::instrumentindex_t nInstr, char removeSamples)
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

    modplug::tracker::modinstrument_t *pIns = Instruments[nInstr];
    Instruments[nInstr] = nullptr;
    for(modplug::tracker::chnindex_t i = 0; i < MAX_VIRTUAL_CHANNELS; i++)
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
bool module_renderer::RemoveInstrumentSamples(modplug::tracker::instrumentindex_t nInstr)
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
        for (modplug::tracker::instrumentindex_t nIns = 1; nIns < MAX_INSTRUMENTS; nIns++) if ((Instruments[nIns]) && (nIns != nInstr))
        {
            p = Instruments[nIns];
            for (UINT r=0; r<128; r++)
            {
                UINT n = p->Keyboard[r];
                if (n <= GetNumSamples()) sampleused[n] = false;
            }
        }
        for (modplug::tracker::sampleindex_t nSmp = 1; nSmp <= GetNumSamples(); nSmp++) if (sampleused[nSmp])
        {
            DestroySample(nSmp);
            strcpy(m_szNames[nSmp], "");
        }
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ITS Samples
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
    ptr += size;                            // jump field

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
    pData += sizeof(size);                            // jump field size

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
            pData += sizeof(code);                             // jump field code
            ReadExtendedInstrumentProperty(pIns, code, pData, pEnd);
        }
    }
}


void ConvertReadExtendedFlags(modplug::tracker::modinstrument_t *pIns)
//------------------------------------------------
{
    const uint32_t dwOldFlags = pIns->flags;
    pIns->flags = pIns->volume_envelope.flags = pIns->panning_envelope.flags = pIns->pitch_envelope.flags = 0;
    if(dwOldFlags & dFdd_VOLUME)            pIns->volume_envelope.flags |= ENV_ENABLED;
    if(dwOldFlags & dFdd_VOLSUSTAIN)    pIns->volume_envelope.flags |= ENV_SUSTAIN;
    if(dwOldFlags & dFdd_VOLLOOP)            pIns->volume_envelope.flags |= ENV_LOOP;
    if(dwOldFlags & dFdd_PANNING)            pIns->panning_envelope.flags |= ENV_ENABLED;
    if(dwOldFlags & dFdd_PANSUSTAIN)    pIns->panning_envelope.flags |= ENV_SUSTAIN;
    if(dwOldFlags & dFdd_PANLOOP)            pIns->panning_envelope.flags |= ENV_LOOP;
    if(dwOldFlags & dFdd_PITCH)                    pIns->pitch_envelope.flags |= ENV_ENABLED;
    if(dwOldFlags & dFdd_PITCHSUSTAIN)    pIns->pitch_envelope.flags |= ENV_SUSTAIN;
    if(dwOldFlags & dFdd_PITCHLOOP)            pIns->pitch_envelope.flags |= ENV_LOOP;
    if(dwOldFlags & dFdd_SETPANNING)    pIns->flags |= INS_SETPANNING;
    if(dwOldFlags & dFdd_FILTER)            pIns->pitch_envelope.flags |= ENV_FILTER;
    if(dwOldFlags & dFdd_VOLCARRY)            pIns->volume_envelope.flags |= ENV_CARRY;
    if(dwOldFlags & dFdd_PANCARRY)            pIns->panning_envelope.flags |= ENV_CARRY;
    if(dwOldFlags & dFdd_PITCHCARRY)    pIns->pitch_envelope.flags |= ENV_CARRY;
    if(dwOldFlags & dFdd_MUTE)                    pIns->flags |= INS_MUTE;
}
