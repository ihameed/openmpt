/*
 * OpenMPT
 *
 * Snd_flt.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"
#include "sndfile.h"

using namespace modplug::pervasives;
using namespace modplug::tracker;

// AWE32: cutoff = reg[0-255] * 31.25 + 100 -> [100Hz-8060Hz]
// EMU10K1 docs: cutoff = reg[0-127]*62+100
#define FILTER_PRECISION    8192

#include <math.h>


uint32_t module_renderer::CutOffToFrequency(UINT nCutOff, int flt_modifier) const
//-----------------------------------------------------------------------
{
    float Fc;
    ASSERT(nCutOff<128);
    if (m_dwSongFlags & SONG_EXFILTERRANGE)
            Fc = 110.0f * pow(2.0f, 0.25f + ((float)(nCutOff*(flt_modifier+256)))/(20.0f*512.0f));
    else
            Fc = 110.0f * pow(2.0f, 0.25f + ((float)(nCutOff*(flt_modifier+256)))/(24.0f*512.0f));
    LONG freq = (LONG)Fc;
    if (freq < 120) return 120;
    if (freq > 20000) return 20000;
    if (freq*2 > (LONG)deprecated_global_mixing_freq) freq = deprecated_global_mixing_freq>>1;
    return (uint32_t)freq;
}


// Simple 2-poles resonant filter
void module_renderer::SetupChannelFilter(modplug::tracker::voice_ty *pChn, bool bReset, int flt_modifier) const
//----------------------------------------------------------------------------------------
{
    float fs = (float)deprecated_global_mixing_freq;
    float fg, fb0, fb1, fc, dmpfac;

/*    if (pChn->pHeader) {
            fc = (float)CutOffToFrequency(pChn->nCutOff, flt_modifier);
            dmpfac = pow(10.0f, -((24.0f / 128.0f)*(float)pChn->nResonance) / 20.0f);
    } else {*/

            int cutoff = 0;
            if(!GetModFlag(MSF_OLDVOLSWING))
            {
                    if(pChn->nCutSwing)
                    {
                            pChn->nCutOff += pChn->nCutSwing;
                            if(pChn->nCutOff > 127) pChn->nCutOff = 127;
                            if(pChn->nCutOff < 0) pChn->nCutOff = 0;
                            pChn->nCutSwing = 0;
                    }
                    if(pChn->nResSwing)
                    {
                            pChn->nResonance += pChn->nResSwing;
                            if(pChn->nResonance > 127) pChn->nResonance = 127;
                            if(pChn->nResonance < 0) pChn->nResonance = 0;
                            pChn->nResSwing = 0;
                    }
                    cutoff = bad_max( bad_min((int)pChn->nCutOff,127), 0); // cap cutoff
                    fc = (float)CutOffToFrequency(cutoff, flt_modifier);
                    dmpfac = pow(10.0f, -((24.0f / 128.0f)*(float)((pChn->nResonance)&0x7F)) / 20.0f);
            }
            else
            {
                    cutoff = bad_max( bad_min((int)pChn->nCutOff+(int)pChn->nCutSwing,127), 0); // cap cutoff
                    fc = (float)CutOffToFrequency(cutoff, flt_modifier);
                    dmpfac = pow(10.0f, -((24.0f / 128.0f)*(float)((pChn->nResonance+pChn->nResSwing)&0x7F)) / 20.0f);
            }



//    }

    fc *= (float)(2.0*3.14159265358/fs);

    float d = (1.0f-2.0f*dmpfac)* fc;
    if (d>2.0) d = 2.0;
    d = (2.0f*dmpfac - d)/fc;
    float e = pow(1.0f/fc,2.0f);

    fg=1/(1+d+e);
    fb0=(d+e+e)/(1+d+e);
    fb1=-e/(1+d+e);

    switch(pChn->nFilterMode) {
    case FLTMODE_HIGHPASS:
        pChn->flt_coefs.a0 = 1.0f - fg;
        pChn->flt_coefs.b0 = fb0;
        pChn->flt_coefs.b1 = fb1;
        pChn->flt_coefs.hp = 1;
        break;
    default:
        pChn->flt_coefs.a0 = fg;
        pChn->flt_coefs.b0 = fb0;
        pChn->flt_coefs.b1 = fb1;
        pChn->flt_coefs.hp = 0;
    }

    if (bReset) {
        pChn->flt_hist = empty_flt_lr;
    }
    bitset_add(pChn->flags, vflag_ty::Filter);
}
