/*
 * OpenMPT
 *
 * Fastmix.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"
#include "sndfile.h"
#ifdef _DEBUG
#include <math.h>
#endif

#include "../mixgraph/constants.hpp"
#include "../mixgraph/mixer.hpp"
#include "../tracker/mixer.hpp"

#include "../tracker/eval.hpp"

#include "../pervasives/pervasives.hpp"

using namespace modplug::pervasives;
using namespace modplug::tracker;

// rewbs.resamplerConf
#include "../MainFrm.h"
#include "WindowedFIR.h"
// end  rewbs.resamplerConf

#pragma bss_seg(".modplug")

// Front Mix Buffer (Also room for interleaved rear mix)
int MixSoundBuffer[modplug::mixgraph::MIX_BUFFER_SIZE*4];


float MixFloatBuffer[modplug::mixgraph::MIX_BUFFER_SIZE*2];

#pragma bss_seg()



extern LONG gnDryROfsVol;
extern LONG gnDryLOfsVol;
extern LONG gnRvbROfsVol;
extern LONG gnRvbLOfsVol;
typedef VOID (MPPASMCALL * LPMIXINTERFACE)(modplug::tracker::voice_ty *, int *, int *);

UINT module_renderer::CreateStereoMix(int count)
//-----------------------------------------
{
    uint32_t nchused, nchmixed;

    if (!count) return 0;
    nchused = nchmixed = 0;
    for (UINT nChn=0; nChn<m_nMixChannels; nChn++)
    {
        modplug::tracker::voice_ty * const pChannel = &Chn[ChnMix[nChn]];
        LONG nSmpCount;
        int nsamples;



        uint32_t functionNdx = 0;

        size_t channel_i_care_about = pChannel->parent_channel ? (pChannel->parent_channel - 1) : (ChnMix[nChn]);
        auto herp = (channel_i_care_about > modplug::mixgraph::MAX_PHYSICAL_CHANNELS) ?
            (mixgraph.channel_bypass) :
            (mixgraph.channel_vertices[channel_i_care_about]);
        auto herp_left = herp->channels[0];
        auto herp_right = herp->channels[1];

        if (!pChannel->active_sample_data.generic) continue;
        //nFlags |= GetResamplingFlag(pChannel);
        nsamples = count;

        //Look for plugins associated with this implicit tracker channel.
        UINT nMixPlugin = GetBestPlugin(ChnMix[nChn], PRIORITISE_INSTRUMENT, RESPECT_MUTES);

        //rewbs.instroVSTi
/*            UINT nMixPlugin=0;
        if (pChannel->instrument && pChannel->pInstrument) {    // first try intrument VST
            if (!(pChannel->pInstrument->uFlags & ENV_MUTE))
                nMixPlugin = pChannel->instrument->nMixPlug;
        }
        if (!nMixPlugin && (nMasterCh > 0) && (nMasterCh <= m_nChannels)) {     // Then try Channel VST
            if(!(pChannel->dwFlags & CHN_NOFX))
                nMixPlugin = ChnSettings[nMasterCh-1].nMixPlugin;
        }
*/

        //end rewbs.instroVSTi
        //XXXih: vchan mix -> plugin prepopulated state heer
        /*
        if ((nMixPlugin > 0) && (nMixPlugin <= MAX_MIXPLUGINS))
        {
            PSNDMIXPLUGINSTATE pPlugin = m_MixPlugins[nMixPlugin-1].pMixState;
            if ((pPlugin) && (pPlugin->pMixBuffer))
            {
                pbuffer = pPlugin->pMixBuffer;
                pOfsR = &pPlugin->nVolDecayR;
                pOfsL = &pPlugin->nVolDecayL;
                if (!(pPlugin->dwFlags & MIXPLUG_MIXREADY))
                {
                    //modplug::mixer::stereo_fill(pbuffer, count, pOfsR, pOfsL);
                    pPlugin->dwFlags |= MIXPLUG_MIXREADY;
                }
            }
        }
        pbuffer = MixSoundBuffer;
        */
        nchused++;
        ////////////////////////////////////////////////////
    SampleLooping:
        UINT nrampsamples = nsamples;
        if (pChannel->nRampLength > 0)
        {
            if ((LONG)nrampsamples > pChannel->nRampLength) nrampsamples = pChannel->nRampLength;
        }
        nSmpCount = GetSampleCount(pChannel, nrampsamples, m_bITBidiMode);
        //nSmpCount = calc_contiguous_span_in_place_legacy(*pChannel, nrampsamples, m_bITBidiMode);

        if (nSmpCount <= 0) {
            // Stopping the channel
            pChannel->active_sample_data.generic = nullptr;
            pChannel->length = 0;
            pChannel->sample_position = 0;
            pChannel->fractional_sample_position = 0;
            pChannel->nRampLength = 0;

            pChannel->nROfs = pChannel->nLOfs = 0;
            bitset_remove(pChannel->flags, vflag_ty::ScrubBackwards);
            continue;
        }
        // Should we mix this channel ?
        UINT naddmix;
        if (
            ( (nchmixed >= m_nMaxMixChannels)
             && (!(deprecated_global_sound_setup_bitmask & SNDMIX_DIRECTTODISK))
            )
         || ( (pChannel->nRampLength == 0)
             && ( (pChannel->left_volume == 0) && (pChannel->right_volume == 0) )
            )
           )
        {
            /*
            LONG delta = (pChannel->position_delta * (LONG)nSmpCount) + (LONG)pChannel->fractional_sample_position;
            pChannel->fractional_sample_position = delta & 0xFFFF;
            pChannel->sample_position += (delta >> 16);
            pChannel->nROfs = pChannel->nLOfs = 0;
            */
            advance_silently(*pChannel, nSmpCount);

            herp_left += nSmpCount;
            herp_right += nSmpCount;
            naddmix = 0;
        } else
        // Do mixing
        {
            // Choose function for mixing
            //LPMIXINTERFACE pMixFunc;
            //XXXih: disabled legacy garbage
            //pMixFunc = (pChannel->nRampLength) ? pMixFuncTable[nFlags|MIXNDX_RAMP] : pMixFuncTable[nFlags];
            //pMixFunc = (pChannel->nRampLength) ? pMixFuncTable[nFlags|MIXNDX_RAMP] : pMixFuncTable[nFlags];

            int maxlen = nSmpCount;

            pChannel->nROfs = 0;
            pChannel->nLOfs = 0;

            //modplug::mixgraph::resample_and_mix(herp_left, herp_right, pChannel, maxlen);
            modplug::tracker::mix_and_advance(herp_left, herp_right, maxlen, pChannel);

            herp_left += maxlen;
            herp_right += maxlen;

            naddmix = 1;
        }
        nsamples -= nSmpCount;
        if (pChannel->nRampLength)
        {
            pChannel->nRampLength -= nSmpCount;
            if (pChannel->nRampLength <= 0)
            {
                pChannel->nRampLength = 0;
                pChannel->right_volume = pChannel->nNewRightVol;
                pChannel->left_volume = pChannel->nNewLeftVol;
                pChannel->right_ramp = pChannel->left_ramp = 0;
                if (bitset_is_set(pChannel->flags, vflag_ty::NoteFade) && (!(pChannel->nFadeOutVol)))
                {
                    pChannel->length = 0;
                    pChannel->active_sample_data.generic = nullptr;
                }
            }
        }
        if (nsamples > 0) goto SampleLooping;
        nchmixed += naddmix;
    }
    return nchused;
}

extern int gbInitPlugins;

void module_renderer::ProcessPlugins(UINT nCount)
//------------------------------------------
{
    // Setup float inputs
    for (UINT iPlug=0; iPlug<MAX_MIXPLUGINS; iPlug++)
    {
        PSNDMIXPLUGIN pPlugin = &m_MixPlugins[iPlug];
        if ((pPlugin->pMixPlugin) && (pPlugin->pMixState)
         && (pPlugin->pMixState->pMixBuffer)
         && (pPlugin->pMixState->pOutBufferL)
         && (pPlugin->pMixState->pOutBufferR))
        {
            PSNDMIXPLUGINSTATE pState = pPlugin->pMixState;
            // Init plugins ?
            /*if (gbInitPlugins)
            {   //ToDo: do this in resume.
                pPlugin->pMixPlugin->Init(gdwMixingFreq, (gbInitPlugins & 2) ? TRUE : FALSE);
            }*/

            //We should only ever reach this point if the song is playing.
            if (!pPlugin->pMixPlugin->IsSongPlaying())
            {
                //Plugin doesn't know it is in a song that is playing;
                //we must have added it during playback. Initialise it!
                pPlugin->pMixPlugin->NotifySongPlaying(true);
                pPlugin->pMixPlugin->Resume();
            }


            // Setup float input
            if (pState->dwFlags & MIXPLUG_MIXREADY)
            {
                //StereoMixToFloat(pState->pMixBuffer, pState->pOutBufferL, pState->pOutBufferR, nCount);
            } else
            if (pState->nVolDecayR|pState->nVolDecayL)
            {
                //modplug::mixer::stereo_fill(pState->pMixBuffer, nCount, &pState->nVolDecayR, &pState->nVolDecayL);
                //XXXih: indiana jones and the ancient mayan treasure
                //StereoMixToFloat(pState->pMixBuffer, pState->pOutBufferL, pState->pOutBufferR, nCount);
            } else
            {
                memset(pState->pOutBufferL, 0, nCount*sizeof(FLOAT));
                memset(pState->pOutBufferR, 0, nCount*sizeof(FLOAT));
            }
            pState->dwFlags &= ~MIXPLUG_MIXREADY;
        }
    }
    // Convert mix buffer
    //StereoMixToFloat(MixSoundBuffer, MixFloatBuffer, MixFloatBuffer+modplug::mixgraph::MIX_BUFFER_SIZE, nCount);
    FLOAT *pMixL = MixFloatBuffer;
    FLOAT *pMixR = MixFloatBuffer + modplug::mixgraph::MIX_BUFFER_SIZE;

    // Process Plugins
    //XXXih replace
    //XXXih replace
    //XXXih replace
    for (UINT iDoPlug=0; iDoPlug<MAX_MIXPLUGINS; iDoPlug++)
    {
        PSNDMIXPLUGIN pPlugin = &m_MixPlugins[iDoPlug];
        if ((pPlugin->pMixPlugin) && (pPlugin->pMixState)
         && (pPlugin->pMixState->pMixBuffer)
         && (pPlugin->pMixState->pOutBufferL)
         && (pPlugin->pMixState->pOutBufferR))
        {
            BOOL bMasterMix = FALSE;
            if (pMixL == pPlugin->pMixState->pOutBufferL)
            {
                bMasterMix = TRUE;
                pMixL = MixFloatBuffer;
                pMixR = MixFloatBuffer + modplug::mixgraph::MIX_BUFFER_SIZE;
            }
            IMixPlugin *pObject = pPlugin->pMixPlugin;
            PSNDMIXPLUGINSTATE pState = pPlugin->pMixState;
            FLOAT *pOutL = pMixL;
            FLOAT *pOutR = pMixR;

            if (pPlugin->Info.dwOutputRouting & 0x80)
            {
                UINT nOutput = pPlugin->Info.dwOutputRouting & 0x7f;
                if ((nOutput > iDoPlug) && (nOutput < MAX_MIXPLUGINS)
                 && (m_MixPlugins[nOutput].pMixState))
                {
                    PSNDMIXPLUGINSTATE pOutState = m_MixPlugins[nOutput].pMixState;

                    if( (pOutState->pOutBufferL) && (pOutState->pOutBufferR) )
                    {
                        pOutL = pOutState->pOutBufferL;
                        pOutR = pOutState->pOutBufferR;
                    }
                }
            }

            /*
            if (pPlugin->multiRouting) {
                int nOutput=0;
                for (int nOutput=0; nOutput<pPlugin->nOutputs/2; nOutput++) {
                    destinationPlug = pPlugin->multiRoutingDestinations[nOutput];
                    pOutState = m_MixPlugins[destinationPlug].pMixState;
                    pOutputs[2*nOutput] = pOutState->pOutBufferL;
                    pOutputs[2*(nOutput+1)] = pOutState->pOutBufferR;
                }

            }
*/

            if (pPlugin->Info.dwInputRouting & MIXPLUG_INPUTF_MASTEREFFECT)
            {
                if (!bMasterMix)
                {
                    FLOAT *pInL = pState->pOutBufferL;
                    FLOAT *pInR = pState->pOutBufferR;
                    for (UINT i=0; i<nCount; i++)
                    {
                        pInL[i] += pMixL[i];
                        pInR[i] += pMixR[i];
                        pMixL[i] = 0;
                        pMixR[i] = 0;
                    }
                }
                pMixL = pOutL;
                pMixR = pOutR;
            }

            if (pPlugin->Info.dwInputRouting & MIXPLUG_INPUTF_BYPASS)
            {
                const FLOAT * const pInL = pState->pOutBufferL;
                const FLOAT * const pInR = pState->pOutBufferR;
                for (UINT i=0; i<nCount; i++)
                {
                    pOutL[i] += pInL[i];
                    pOutR[i] += pInR[i];
                }
            } else
            {
                pObject->Process(pOutL, pOutR, nCount);
            }
        }
    }
    //XXXih replace
    //XXXih replace
    //XXXih replace
    //FloatToStereoMix(pMixL, pMixR, MixSoundBuffer, nCount);
    gbInitPlugins = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Float <-> Int conversion
//


float module_renderer::m_nMaxSample = 0;

//////////////////////////////////////////////////////////////////////////
// Noise Shaping (Dither)

#pragma warning(disable:4731) // ebp modified

void MPPASMCALL X86_Dither(int *pBuffer, UINT nSamples, UINT nBits)
//-----------------------------------------------------------------
{
    static int gDitherA, gDitherB;

    __asm {
    mov esi, pBuffer    // esi = pBuffer+i
    mov eax, nSamples    // ebp = i
    mov ecx, nBits            // ecx = number of bits of noise
    mov edi, gDitherA    // Noise generation
    mov ebx, gDitherB
    add ecx, modplug::mixgraph::MIXING_ATTENUATION+1
    push ebp
    mov ebp, eax
noiseloop:
    rol edi, 1
    mov eax, dword ptr [esi]
    xor edi, 0x10204080
    add esi, 4
    lea edi, [ebx*4+edi+0x78649E7D]
    mov edx, edi
    rol edx, 16
    lea edx, [edx*4+edx]
    add ebx, edx
    mov edx, ebx
    sar edx, cl
    add eax, edx
/*
    int a = 0, b = 0;
    for (UINT i=0; i<len; i++)
    {
        a = (a << 1) | (((uint32_t)a) >> (uint8_t)31);
        a ^= 0x10204080;
        a += 0x78649E7D + (b << 2);
        b += ((a << 16) | (a >> 16)) * 5;
        int c = a + b;
        p[i] = ((signed char)c ) >> 1;
    }
*/
    dec ebp
    mov dword ptr [esi-4], eax
    jnz noiseloop
    pop ebp
    mov gDitherA, edi
    mov gDitherB, ebx
    }
}

VOID MPPASMCALL X86_MonoFromStereo(int *pMixBuf, UINT nSamples)
//-------------------------------------------------------------
{
    _asm {
    mov ecx, nSamples
    mov esi, pMixBuf
    mov edi, esi
stloop:
    mov eax, dword ptr [esi]
    mov edx, dword ptr [esi+4]
    add edi, 4
    add esi, 8
    add eax, edx
    sar eax, 1
    dec ecx
    mov dword ptr [edi-4], eax
    jnz stloop
    }
}
