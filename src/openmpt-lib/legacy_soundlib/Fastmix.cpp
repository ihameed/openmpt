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

#include "old_utils.h"

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

float MixFloatBuffer[modplug::mixgraph::MIX_BUFFER_SIZE*2];

#pragma bss_seg()

bool __forceinline
should_mix(const int chans_mixed, const int max_mix_chans, const voice_ty &chan) {
    //&& (!(deprecated_global_sound_setup_bitmask & SNDMIX_DIRECTTODISK))
    return (chans_mixed >= max_mix_chans) ||
        ( (chan.nRampLength == 0) &&
          ( (chan.left_volume == 0) && (chan.right_volume == 0) ) );
}

uint32_t
module_renderer::CreateStereoMix(const int32_t count) {
    if (!count) return 0;
    uint32_t nchused = 0;
    uint32_t nchmixed = 0;
    for (uint32_t chanidx = 0; chanidx < m_nMixChannels; ++chanidx) {
        auto &chan = Chn[ChnMix[chanidx]];
        auto remaining_frames = count;

        const auto attribution_channel = chan.parent_channel
            ? (chan.parent_channel - 1)
            : ChnMix[chanidx];
            /*
        const auto outnode = attribution_channel > modplug::mixgraph::MAX_PHYSICAL_CHANNELS ?
            mixgraph.channel_bypass :
            mixgraph.channel_vertices[attribution_channel];
            */
        const auto outnode = mixgraph.channel_vertices[0];
        const auto oldleft = outnode->channels[0];
        const auto oldright = outnode->channels[0];
        auto left = outnode->channels[0];
        auto right = outnode->channels[1];

        if (!chan.active_sample_data.generic) continue;
        //nFlags |= GetResamplingFlag(chan);

        //Look for plugins associated with this implicit tracker channel.
        const auto nMixPlugin = GetBestPlugin(ChnMix[chanidx], PRIORITISE_INSTRUMENT, RESPECT_MUTES);

        ++nchused;
        uint32_t naddmix = 0;
        while (remaining_frames > 0) {
            //XXXih: mix-src-revamp
            //const auto contiguous_frames = GetSampleCount(chan, remaining_frames, m_bITBidiMode);
            const auto contiguous_frames = remaining_frames;

            if (contiguous_frames <= 0) {
                // Stopping the channel
                chan.active_sample_data.generic = nullptr;
                chan.length = 0;
                chan.sample_position = 0;
                chan.fractional_sample_position = 0;
                chan.nRampLength = 0;

                chan.nROfs = chan.nLOfs = 0;
                bitset_remove(chan.flags, vflag_ty::ScrubBackwards);
                goto nextchannel;
            }

            if (should_mix(nchmixed, m_nMaxMixChannels, chan)) {
                /*
                LONG delta = (chan->position_delta * (LONG)contiguous_frames) + (LONG)chan->fractional_sample_position;
                chan->fractional_sample_position = delta & 0xFFFF;
                chan->sample_position += (delta >> 16);
                chan->nROfs = chan->nLOfs = 0;
                */
                advance_silently(chan, contiguous_frames);

                left += contiguous_frames;
                right += contiguous_frames;
                naddmix = 0;
            } else {
                const auto do_frames = remaining_frames;
                chan.nROfs = 0;
                chan.nLOfs = 0;

                //modplug::mixgraph::resample_and_mix(left, right, &chan, contiguous_frames);
                const auto unrendered_frames = modplug::tracker::mix_and_advance(left, right, contiguous_frames, &chan);

                left += contiguous_frames;
                right += contiguous_frames;
                naddmix = 1;
            }
            remaining_frames -= contiguous_frames;
            if (chan.nRampLength)
            {
                chan.nRampLength -= contiguous_frames;
                if (chan.nRampLength <= 0)
                {
                    chan.nRampLength = 0;
                    chan.right_volume = chan.nNewRightVol;
                    chan.left_volume = chan.nNewLeftVol;
                    chan.right_ramp = chan.left_ramp = 0;
                    if (bitset_is_set(chan.flags, vflag_ty::NoteFade) && (!(chan.nFadeOutVol)))
                    {
                        chan.length = 0;
                        chan.active_sample_data.generic = nullptr;
                    }
                }
            }
        }
        nextchannel:
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
