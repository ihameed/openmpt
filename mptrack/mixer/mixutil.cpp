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

//TODO: replace this with a clean bsdl rewrite

#include "stdafx.h"

#include <cstring>

#include "sndfile.h"
#include "../mixgraph/constants.h"
#include "mixutil.h"

namespace modplug {
namespace mixer {


static const unsigned int OFSDECAYSHIFT = 8;
static const unsigned int OFSDECAYMASK = 0xFF;

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

void end_channel_ofs(modplug::tracker::modchannel_t *channel, int *buffer, size_t samples) {
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




void mono_mix_to_float(const int *src, float *out, size_t count, const float i2fc) {
    for (size_t i = 0; i < count; i++) {
        *out++ = *src * i2fc;
        src++;
    }
}

void float_to_mono_mix(const float *in, int *out, size_t count, const float f2ic) {
    for (size_t i = 0; i < count; i++) {
        *out++ = (int) (*in * f2ic);
        in++;
    }
}


// ----------------------------------------------------------------------------
// Clip and convert functions
// ----------------------------------------------------------------------------
// The original C version was written by Rani Assaf <rani@magic.metawire.com>


//XXXih: write tests!
size_t clip_32_to_8(void *ptr, int *buffer, size_t samples) {
    unsigned char *p = (unsigned char *) ptr;

    for (size_t i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        // 8-bit unsigned
        p[i] = static_cast<unsigned char>((n >> (24 - modplug::mixgraph::MIXING_ATTENUATION)) ^ 0x80);
    }

    return samples;
}

size_t clip_32_to_16(void *ptr, int *buffer, size_t samples) {
    signed short *p = (signed short *) ptr;

    for (size_t i = 0; i < samples; i++) {
        int n = buffer[i];
        n += 1 << (15 - modplug::mixgraph::MIXING_ATTENUATION);

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        // 16-bit signed
        p[i] = static_cast<signed short>(n >> (16 - modplug::mixgraph::MIXING_ATTENUATION));
    }

    return samples * 2;
}

// XXXih: untested
size_t clip_32_to_24(void *ptr, int *buffer, size_t samples) {
    /* the inventor of 24bit anything should be shot */
    unsigned char *p = (unsigned char *) ptr;

    for (size_t i = 0; i < samples; i++) {
        int n = buffer[i];
        n += 1 << (7 - modplug::mixgraph::MIXING_ATTENUATION);

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        // 24-bit signed
        n = n >> (8 - modplug::mixgraph::MIXING_ATTENUATION);

        /* err, assume same endian */
        memcpy(p, &n, 3);
        p += 3;
    }

    return samples * 3;
}

size_t clip_32_to_32(void *ptr, int *buffer, size_t samples) {
    signed int *p = (signed int *) ptr;

    for (size_t i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        // 32-bit signed
        p[i] = (n << modplug::mixgraph::MIXING_ATTENUATION);
    }

    return samples * 4;
}


}
}
