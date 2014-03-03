#pragma once

#include <cstdint>
#include "../tracker/voice.hpp"

//#define ANOS 1
int32_t __forceinline
GetSampleCount(modplug::tracker::voice_ty &chan, int32_t nSamples, bool bITBidiMode)
{
    using namespace modplug::pervasives;
    using namespace modplug::tracker;
    const int32_t nLoopStart = bitset_is_set(chan.flags, vflag_ty::Loop) ? unwrap(chan.loop_start) : 0;
    int32_t nInc = chan.position_delta;

    if ((nSamples <= 0) || (!nInc) || (!chan.length)) return 0;
    // Under zero ?
    if (unwrap(chan.sample_position) < nLoopStart)
    {
        if (nInc < 0)
        {
            // Invert loop for bidi loops
            int32_t nDelta = ((nLoopStart - chan.sample_position) << 16) - (chan.fractional_sample_position & 0xffff);
            chan.sample_position = nLoopStart | (nDelta>>16);
            chan.fractional_sample_position = nDelta & 0xffff;
            if (((int32_t)chan.sample_position < nLoopStart) || (chan.sample_position >= (nLoopStart+chan.length)/2))
            {
                chan.sample_position = nLoopStart; chan.fractional_sample_position = 0;
            }
            nInc = -nInc;
            chan.position_delta = nInc;
            bitset_remove(chan.flags, vflag_ty::ScrubBackwards); // go forward
            if ((!bitset_is_set(chan.flags, vflag_ty::Loop)) || (chan.sample_position >= chan.length))
            {
                chan.sample_position = chan.length;
                chan.fractional_sample_position = 0;
                return 0;
            }
        } else
        {
            // We probably didn't hit the loop end yet (first loop), so we do nothing
            if ((int32_t)chan.sample_position < 0) chan.sample_position = 0;
        }
    } else
    // Past the end
    if (chan.sample_position >= chan.length)
    {
        if (!bitset_is_set(chan.flags, vflag_ty::Loop)) return 0; // not looping -> stop this channel
        if (bitset_is_set(chan.flags, vflag_ty::BidiLoop))
        {
            // Invert loop
            if (nInc > 0)
            {
                nInc = -nInc;
                chan.position_delta = nInc;
            }
            bitset_add(chan.flags, vflag_ty::ScrubBackwards);
            // adjust loop position
            int32_t nDeltaHi = (chan.sample_position - chan.length);
            int32_t nDeltaLo = 0x10000 - (chan.fractional_sample_position & 0xffff);
            chan.sample_position = chan.length - nDeltaHi - (nDeltaLo>>16);
            chan.fractional_sample_position = nDeltaLo & 0xffff;
            // Impulse Tracker's software mixer would put a -2 (instead of -1) in the following line (doesn't happen on a GUS)
            if ((chan.sample_position <= chan.loop_start) || (chan.sample_position >= chan.length)) chan.sample_position = chan.length - (bITBidiMode ? 2 : 1);
        } else
        {
            if (nInc < 0) // This is a bug
            {
                nInc = -nInc;
                chan.position_delta = nInc;
            }
            // Restart at loop start
            chan.sample_position += nLoopStart - chan.length;
            if ((int32_t)chan.sample_position < nLoopStart) chan.sample_position = chan.loop_start;
        }
    }
    const int32_t nPos = chan.sample_position;
    // too big increment, and/or too small loop length
    if (nPos < nLoopStart)
    {
        if ((nPos < 0) || (nInc < 0)) return 0;
    }
    if ((nPos < 0) || (nPos >= (int32_t)chan.length)) return 0;
    const int32_t nPosLo = (uint16_t)chan.fractional_sample_position;
    int32_t nSmpCount = nSamples;
    if (nInc < 0)
    {
        const int32_t nInv = -nInc;
        int32_t maxsamples = 16384 / ((nInv>>16)+1);
        if (maxsamples < 2) maxsamples = 2;
        if (nSamples > maxsamples) nSamples = maxsamples;
        const int32_t nDeltaHi = (nInv>>16) * (nSamples - 1);
        const int32_t nDeltaLo = (nInv&0xffff) * (nSamples - 1);
        const int32_t nPosDest = nPos - nDeltaHi + ((nPosLo - nDeltaLo) >> 16);
        if (nPosDest < nLoopStart)
        {
            nSmpCount = (uint32_t)(((((LONGLONG)nPos - nLoopStart) << 16) + nPosLo - 1) / nInv) + 1;
        }
    } else
    {
        #ifdef ANOS
        printf("2nd: nInc >= 0; nInc = %d\n", nInc);
        #endif
        int32_t maxsamples = 16384 / ((nInc>>16)+1);
        if (maxsamples < 2) maxsamples = 2;
        if (nSamples > maxsamples) nSamples = maxsamples;
        const int32_t nDeltaHi = (nInc>>16) * (nSamples - 1);
        const int32_t nDeltaLo = (nInc&0xffff) * (nSamples - 1);
        #ifdef ANOS
        printf("2nd: nInc >= 0; nDeltaHi = %d, nDeltaLo = %d\n", nDeltaHi, nDeltaLo);
        #endif
        const int32_t nPosDest = nPos + nDeltaHi + ((nPosLo + nDeltaLo)>>16);
        #ifdef ANOS
        printf("2nd: nInc >= 0; nPosDest = %d\n", nPosDest);
        #endif
        if (nPosDest >= (int32_t)chan.length)
        {
            nSmpCount = (uint32_t)(((((LONGLONG)chan.length - nPos) << 16) - nPosLo - 1) / nInc) + 1;
        }
    }
#ifdef _DEBUG
    {
        int32_t nDeltaHi = (nInc>>16) * (nSmpCount - 1);
        int32_t nDeltaLo = (nInc&0xffff) * (nSmpCount - 1);
        int32_t nPosDest = nPos + nDeltaHi + ((nPosLo + nDeltaLo)>>16);
        if ((nPosDest < 0) || (nPosDest > (int32_t)chan.length))
        {
            Log("Incorrect delta:\n");
            Log("nSmpCount=%d: nPos=%5d.x%04X Len=%5d Inc=%2d.x%04X\n",
                nSmpCount, nPos, nPosLo, chan.length, chan.position_delta>>16, chan.position_delta&0xffff);
            return 0;
        }
    }
#endif
    if (nSmpCount <= 1) return 1;
    if (nSmpCount > nSamples) return nSamples;
    return nSmpCount;
}

