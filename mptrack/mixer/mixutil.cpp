/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2011 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
mixutil.c nabbed from schism
rudely sepple-ized and jammed into openmpt by xaimus  : - [
*/

#include "stdafx.h"

#include <cstring>

#include "sndfile.h"
#include "mixutil.h"

namespace modplug {
namespace mixer {


const unsigned int OFSDECAYSHIFT = 8;
const unsigned int OFSDECAYMASK = 0xFF;

void init_mix_buffer(int *buffer, size_t samples) {
    memset(buffer, 0, samples * sizeof(int));
}


void stereo_fill(int *buffer, size_t samples, long *profs, long *plofs) {
    long rofs = *profs;
    long lofs = *plofs;

    if (!rofs && !lofs) {
        init_mix_buffer(buffer, samples * 2);
        return;
    }

    for (size_t i = 0; i < samples; i++) {
        int x_r = (rofs + (((-rofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;
        int x_l = (lofs + (((-lofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;

        rofs -= x_r;
        lofs -= x_l;
        buffer[i * 2 ]    = x_r;
        buffer[i * 2 + 1] = x_l;
    }

    *profs = rofs;
    *plofs = lofs;
}


void end_channel_ofs(MODCHANNEL *channel, int *buffer, size_t samples) {
    int rofs = channel->nROfs;
    int lofs = channel->nLOfs;

    if (!rofs && !lofs)
        return;

    for (size_t i = 0; i < samples; i++) {
        int x_r = (rofs + (((-rofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;
        int x_l = (lofs + (((-lofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;

        rofs -= x_r;
        lofs -= x_l;
        buffer[i * 2]     += x_r;
        buffer[i * 2 + 1] += x_l;
    }

    channel->nROfs = rofs;
    channel->nLOfs = lofs;
}


void mono_from_stereo(int *mix_buf, size_t samples) {
    for (size_t j, i = 0; i < samples; i++) {
        j = i << 1;
        mix_buf[i] = (mix_buf[j] + mix_buf[j + 1]) >> 1;
    }
}


static const float f2ic = (float) (1 << 28);
static const float i2fc = (float) (1.0 / (1 << 28));


void stereo_mix_to_float(const int *src, float *out1, float *out2, size_t count) {
    for (size_t i = 0; i < count; i++) {
        *out1++ = *src * i2fc;
        src++;

        *out2++ = *src * i2fc;
        src++;
    }
}


void float_to_stereo_mix(const float *in1, const float *in2, int *out, size_t count) {
    for (size_t i = 0; i < count; i++) {
        *out++ = (int) (*in1 * f2ic);
        *out++ = (int) (*in2 * f2ic);
        in1++;
        in2++;
    }
}


void mono_mix_to_float(const int *src, float *out, size_t count) {
    for (size_t i = 0; i < count; i++) {
        *out++ = *src * i2fc;
        src++;
    }
}


void float_to_mono_mix(const float *in, int *out, size_t count) {
    for (size_t i = 0; i < count; i++) {
        *out++ = (int) (*in * f2ic);
        in++;
    }
}


// ----------------------------------------------------------------------------
// Clip and convert functions
// ----------------------------------------------------------------------------
// XXX mins/max were int[2]
//
// The original C version was written by Rani Assaf <rani@magic.metawire.com>


// Clip and convert to 8 bit
size_t clip_32_to_8(void *ptr, int *buffer, size_t samples, int *mins, int *maxs) {
    unsigned char *p = (unsigned char *) ptr;

    for (size_t i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;

        // 8-bit unsigned
        p[i] = static_cast<unsigned char>((n >> (24 - MIXING_ATTENUATION)) ^ 0x80);
    }

    return samples;
}


size_t clip_32_to_16(void *ptr, int *buffer, size_t samples, int *mins, int *maxs) {
    signed short *p = (signed short *) ptr;

    for (size_t i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;

        // 16-bit signed
        p[i] = static_cast<signed short>(n >> (16 - MIXING_ATTENUATION));
    }

    return samples * 2;
}


// 24-bit might not work...
size_t clip_32_to_24(void *ptr, int *buffer, size_t samples, int *mins, int *maxs) {
    /* the inventor of 24bit anything should be shot */
    unsigned char *p = (unsigned char *) ptr;

    for (size_t i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;

        // 24-bit signed
        n = n >> (8 - MIXING_ATTENUATION);

        /* err, assume same endian */
        memcpy(p, &n, 3);
        p += 3;
    }

    return samples * 2;
}


// 32-bit might not work...
size_t clip_32_to_32(void *ptr, int *buffer, size_t samples, int *mins, int *maxs) {
    signed int *p = (signed int *) ptr;

    for (size_t i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;

        // 32-bit signed
        p[i] = (n >> MIXING_ATTENUATION);
    }

    return samples * 2;
}


}
}