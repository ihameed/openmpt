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
