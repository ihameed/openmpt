/*
 * OpenMPT
 *
 * Waveform.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"
#include "sndfile.h"


void X86_Cvt16S_8M(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//-------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        *p++ = (char)(((int)s[(n << 2)+1] + (int)s[(n << 2)+3]) >> 1);
        dwPos += dwInc;
    }
}


void X86_Cvt8S_8M(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    unsigned char *s = (unsigned char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        *p++ = (char)((((UINT)s[(n << 1)] + (UINT)s[(n << 1)+1]) >> 1) - 0x80);
        dwPos += dwInc;
    }
}


void X86_Cvt16M_8M(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//-------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        *p++ = (char)s[(n << 1)+1];
        dwPos += dwInc;
    }
}


void X86_Cvt8M_8M(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        *p++ = (char)(s[dwPos >> 16] - 0x80);
        dwPos += dwInc;
    }
}


void X86_Cvt16S_8S(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//-------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        p[0] = s[(n << 2)+1];
        p[1] = s[(n << 2)+3];
        p += 2;
        dwPos += dwInc;
    }
}


void X86_Cvt8S_8S(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        p[0] = (char)(s[n << 1] - 0x80);
        p[1] = (char)(s[(n << 1)+1] - 0x80);
        p += 2;
        dwPos += dwInc;
    }
}


void X86_Cvt16M_8S(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//-------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        p[1] = p[0] = (char)s[(n << 1)+1];
        p += 2;
        dwPos += dwInc;
    }
}


void X86_Cvt8M_8S(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        p[1] = p[0] = (char)(s[dwPos >> 16] - 0x80);
        p += 2;
        dwPos += dwInc;
    }
}



UINT module_renderer::WaveConvert(LPBYTE lpSrc, signed char *lpDest, UINT nSamples)
//----------------------------------------------------------------------------
{
    uint32_t dwInc;
    if ((!lpSrc) || (!lpDest) || (!nSamples)) return 0;
    dwInc = _muldiv(deprecated_global_mixing_freq, 0x10000, 22050);
    if (deprecated_global_channels >= 2)
    {
        if (deprecated_global_bits_per_sample == 16)
        {
            // Stereo, 16-bit
            X86_Cvt16S_8M(lpSrc, lpDest, nSamples, dwInc);
        } else
        {
            // Stereo, 8-bit
            X86_Cvt8S_8M(lpSrc, lpDest, nSamples, dwInc);
        }
    } else
    {
        if (deprecated_global_bits_per_sample == 16)
        {
            // Mono, 16-bit
            X86_Cvt16M_8M(lpSrc, lpDest, nSamples, dwInc);
        } else
        {
            // Mono, 8-bit
            X86_Cvt8M_8M(lpSrc, lpDest, nSamples, dwInc);
        }
    }
    return nSamples;
}


UINT module_renderer::WaveStereoConvert(LPBYTE lpSrc, signed char *lpDest, UINT nSamples)
//----------------------------------------------------------------------------------
{
    uint32_t dwInc;
    if ((!lpSrc) || (!lpDest) || (!nSamples)) return 0;
    dwInc = _muldiv(deprecated_global_mixing_freq, 0x10000, 22050);
    if (deprecated_global_channels >= 2)
    {
        if (deprecated_global_bits_per_sample == 16)
        {
            // Stereo, 16-bit
            X86_Cvt16S_8S(lpSrc, lpDest, nSamples, dwInc);
        } else
        {
            // Stereo, 8-bit
            X86_Cvt8S_8S(lpSrc, lpDest, nSamples, dwInc);
        }
    } else
    {
        if (deprecated_global_bits_per_sample == 16)
        {
            // Mono, 16-bit
            X86_Cvt16M_8S(lpSrc, lpDest, nSamples, dwInc);
        } else
        {
            // Mono, 8-bit
            X86_Cvt8M_8S(lpSrc, lpDest, nSamples, dwInc);
        }
    }
    return nSamples;
}

///////////////////////////////////////////////////////////////////////
// Spectrum Analyzer

extern int SpectrumSinusTable[256*2];


void X86_Spectrum(signed char *pBuffer, UINT nSamples, UINT nInc, UINT nSmpSize, LPLONG lpSinCos)
//-----------------------------------------------------------------------------------------------
{
    signed char *p = pBuffer;
    UINT wt = 0x40;
    LONG ecos = 0, esin = 0;
    for (UINT i=0; i<nSamples; i++)
    {
        int a = *p;
        ecos += a*SpectrumSinusTable[wt & 0x1FF];
        esin += a*SpectrumSinusTable[(wt+0x80) & 0x1FF];
        wt += nInc;
        p += nSmpSize;
    }
    lpSinCos[0] = esin;
    lpSinCos[1] = ecos;
}



LONG module_renderer::SpectrumAnalyzer(signed char *pBuffer, UINT nSamples, UINT nInc, UINT nChannels)
//-----------------------------------------------------------------------------------------------
{
    LONG sincos[2];
    UINT n = nSamples & (~3);
    if ((pBuffer) && (n))
    {
        sincos[0] = sincos[1] = 0;
        X86_Spectrum(pBuffer, n, nInc, nChannels, sincos);
        LONG esin = sincos[0], ecos = sincos[1];
        if (ecos < 0) ecos = -ecos;
        if (esin < 0) esin = -esin;
        int bug = 64*8 + (64*64 / (nInc+5));
        if (ecos > bug) ecos -= bug; else ecos = 0;
        if (esin > bug) esin -= bug; else esin = 0;
        return ((ecos + esin) << 2) / nSamples;
    }
    return 0;
}

extern void MPPASMCALL X86_Dither(int *pBuffer, UINT nSamples, UINT nBits);
extern int MixSoundBuffer[];

UINT module_renderer::Normalize24BitBuffer(LPBYTE pbuffer, UINT dwSize, uint32_t lmax24, uint32_t dwByteInc)
//-----------------------------------------------------------------------------------------------
{
    LONG lMax = (lmax24 / 128) + 1;
    UINT i = 0;
    for (UINT j=0; j<dwSize; j+=3, i+=dwByteInc)
    {
        LONG l = ((((pbuffer[j+2] << 8) + pbuffer[j+1]) << 8) + pbuffer[j]) << 8;
        l /= lMax;
        if (dwByteInc > 1)
        {
            pbuffer[i] = (uint8_t)(l & 0xFF);
            pbuffer[i+1] = (uint8_t)(l >> 8);
        } else
        {
            pbuffer[i] = (uint8_t)((l + 0x8000) >> 8);
        }
    }
    return i;
}
