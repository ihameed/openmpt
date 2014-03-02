#include "../openmpt-lib/stdafx.h"
#include "../openmpt-lib/main.h"

#include "../openmpt-lib/tracker/tracker.hpp"
#include "../openmpt-lib/tracker/eval.hpp"

#include "../openmpt-lib/mixgraph/mixer.hpp"

#include <cstdio>

#include <sndfile.h>

template <typename Ty>
Ty inline
mymin(const Ty &x, const Ty &y) {
    return x < y ? x : y;
}

template <typename Ty>
Ty inline
mymax(const Ty &x, const Ty &y) {
    return x > y ? x : y;
}

const std::string outpath1 = "D:\\Users\\imran\\hugeout.wav";
const std::string outpath2 = "D:\\Users\\imran\\hugeout_orig.wav";

using namespace modplug::mixgraph;
using namespace modplug::tracker;
using namespace modplug::pervasives;

CTrackApp theApp;

struct outbuf_ty {
    double *left;
    double *right;
    size_t frames;
};

void
clear_outbuf(outbuf_ty &val) {
    memset(val.left, 0, val.frames * sizeof(sample_t));
    memset(val.right, 0, val.frames * sizeof(sample_t));
}

void
print_outbuf(const outbuf_ty &val, const size_t count) {
    printf("outbuf_ty {\n");

    printf("    left = [ ");
    printf("%f", val.left[0]);
    for (size_t i = 1; i < count; ++i) {
        printf(", %f", val.left[i]);
    }
    printf(" ]\n");

    printf("    right = [ ");
    printf("%f", val.left[0]);
    for (size_t i = 1; i < count; ++i) {
        printf(", %f", val.right[i]);
    }
    printf(" ]\n");

    printf("}\n");
}

int32_t
fp64_to_16_16(const double val) {
    const auto hi = static_cast<int32_t>(val);
    const auto lo = static_cast<int32_t>(val * pow(2, 16));
    return (hi << 16) | (lo & 0xffff);
}

int32_t
hi_16_16(const int32_t val) {
    return val << 16;
}

struct sample_ty {
    int16_t *buf;
    uint32_t num_frames;
    uint32_t channels;
};

void
write_outbuf(const std::string &path, const outbuf_ty &outbuf) {
    SF_INFO info = { 0 };
    info.channels = 2;
    info.format = SF_FORMAT_WAV | SF_FORMAT_DOUBLE | SF_ENDIAN_LITTLE;
    info.samplerate = 44100;
    auto state = sf_open(path.c_str(), SFM_WRITE, &info);
    if (state == nullptr) {
        DEBUG_FUNC("failed opening %s, reason %s", path.c_str(), sf_strerror(nullptr));
        throw new std::runtime_error("oh no      oh no");
    }
    size_t rem = outbuf.frames;
    const size_t tmplen = 1024 * 512;
    auto interleaved = new sample_t[tmplen];
    auto left = outbuf.left;
    auto right = outbuf.right;
    while (rem) {
        const auto max = mymin(tmplen, rem);
        for (size_t i = 0; i < max; i += 2) {
            interleaved[i] = *left;
            interleaved[i + 1] = *right;
            ++left;
            ++right;
        }
        const auto written = sf_writef_double(state, interleaved, max / 2);
        if (written != (max / 2)) {
            DEBUG_FUNC("written = %llu, max = %u\n", written, max); 
            throw new std::runtime_error("hep hep");
        }
        rem -= max;
    }
    delete[] interleaved;
    sf_close(state);
}

sample_ty
load_sample(const std::string &path) {
    sample_ty ret;
    SF_INFO info = { 0 };
    auto state = sf_open(path.c_str(), SFM_READ, &info);
    if (state == nullptr) throw new std::runtime_error("my heimer!");
    auto buf = new int16_t[static_cast<size_t>(info.frames) * info.channels];
    sf_readf_short(state, buf, info.frames);
    sf_close(state);
    ret.buf = buf;
    ret.num_frames = static_cast<uint32_t>(info.frames);
    ret.channels = static_cast<uint32_t>(info.channels);
    return ret;
}

template <typename Ty>
void
update_spill_buf_bidi(
    const modchannel_t &chan, sample_t *dst, int32_t offset, int32_t direction)
{
    const auto buf = reinterpret_cast<const Ty * const>(chan.active_sample_data);
    const int32_t loop_end = chan.loop_end;
    const int32_t loop_start = chan.loop_start;
    for (size_t i = 0; i < SpillMax; ++i) {
        dst[i] = buf[offset] * NORM_INT16;
        if (offset >= (loop_end - 1)) {
            offset = loop_end - 2;
            direction = -1;
        } else if (offset <= loop_start) {
            offset = loop_start + 1;
            direction = 1;
        } else {
            offset += direction;
        }
    }
}

template <typename Ty>
void
update_spill_buf_fwd(
    const modchannel_t &chan, sample_t *dst, int32_t offset, int32_t direction)
{
    const auto buf = reinterpret_cast<const Ty * const>(chan.active_sample_data);
    const int32_t loop_end = chan.loop_end;
    const int32_t loop_start = chan.loop_start;
    for (size_t i = 0; i < SpillMax; ++i) {
        dst[i] = buf[offset] * NORM_INT16;
        if (offset >= (loop_end - 1)) offset = loop_start;
        else if (offset <= loop_start) offset = loop_end - 1;
        else offset += direction;
    }
}

template <typename Ty>
void
update_spill_bufs_in_place(modchannel_t &chan) {
    if (chan.active_sample_data) {
        const bool valid_bidi_loop =
            bitset_is_set(chan.flags, vflag_ty::ScrubBackwards) &&
            (chan.length > 2);
        if (valid_bidi_loop) {
            update_spill_buf_bidi<Ty>(chan, chan.spill_fwd, chan.loop_end - 2, -1);
            update_spill_buf_bidi<Ty>(chan, chan.spill_back, chan.loop_start + 1, 1);
            return;
        }
        const bool valid_fwd_loop =
            bitset_is_set(chan.flags, vflag_ty::Loop) &&
            (chan.length > 0) &&
            ((chan.loop_end - chan.loop_start) > 0);
        if (valid_fwd_loop) {
            update_spill_buf_fwd<Ty>(chan, chan.spill_fwd, chan.loop_start, 1);
            update_spill_buf_fwd<Ty>(chan, chan.spill_back, chan.loop_end - 1, -1);
            return;
        }
    }
    memset(chan.spill_back, 0, SpillMax * sizeof(double));
    memset(chan.spill_fwd, 0, SpillMax * sizeof(double));
}

modchannel_t
sample_to_channel(const sample_ty &val) {
    modchannel_t ret = { 0 };
    ret.m_Freq = 44100;
    ret.flags = vflags_ty(vflag_ty::SixteenBit);
    ret.length = val.num_frames;
    ret.loop_start = 0;
    ret.loop_end = 0;
    ret.left_volume = 4096;
    ret.right_volume = 4096;
    ret.nFilter_A0 = 1;
    ret.active_sample_data = reinterpret_cast<char *>(val.buf);
    ret.sample_data = reinterpret_cast<char *>(val.buf);
    return ret;
}

const size_t TestChunkSize = 32;
//const size_t TestChunkSize = 1024 * 1024 * 50;
const int LoopCount = 1024 * 1024 * 50;
//const int LoopCount = 40;
#define USE_COUNT 1

void
test_resample(const modchannel_t &chan_, outbuf_ty &out) {
    ghettotimer timer(__FUNCTION__);
    auto chan = chan_;
    auto left = out.left;
    auto right = out.right;
    int count = LoopCount;
    while (true) {
        const auto remainder = GetSampleCount(&chan, TestChunkSize, false);
        if (remainder == 0) break;
        //modplug::mixgraph::resample_and_mix(left, right, &chan, remainder);
        //modplug::mixgraph::resample_and_mix_inner<int16_t, false>(left, right, &chan, remainder);
        modplug::mixgraph::resample_and_mix_inner<int16_t, true>(left, right, &chan, remainder);
        //advance_silently(chan, remainder);
        left += remainder;
        right += remainder;
        #if USE_COUNT
        count -= remainder;
        #endif
        if (count < 0) break;
    }
}

const sample_t VolScale = 1.0 / 4096.0;

template <typename Ty>
Ty __forceinline
lookup(const modchannel_t &chan, const int32_t idx, const Ty * const buf) {
    if (idx >= chan.length) {
        if (bitset_is_set(chan.flags, vflag_ty::BidiLoop)) {
            const auto looplen = chan.length - chan.loop_start - 1;
            const auto full = 2 * looplen;
            const auto mod = (looplen + idx - (chan.length - 1)) % full;
            return buf[chan.loop_start + (mod <= looplen ? mod : (full - mod))];
        } else if (bitset_is_set(chan.flags, vflag_ty::BidiLoop)) {
            return buf[(idx - chan.length) + chan.loop_start];
        } else {
            return 0;
        }
    } else if (idx < chan.loop_start) {
        if (chan.position_delta >= 0 && idx >= 0) {
            return buf[idx];
        } else if (bitset_is_set(chan.flags, vflag_ty::BidiLoop)) {
            const auto looplen = chan.length - chan.loop_start - 1;
            const auto full = 2 * looplen;
            const auto mod = (chan.loop_start - idx) % full;
            return buf[chan.loop_start + (mod <= looplen ? mod : (full - mod))];
        } else {
            return 0;
        }
    }
    return buf[idx];
}

sample_t __forceinline
lookup_spill(const modchannel_t &chan, const int32_t idx, const int16_t * const buf) {
    const auto length = chan.length;
    const auto loop_start = chan.loop_start;
    if (idx >= length) {
        if (bitset_is_set(chan.flags, vflag_ty::Loop, vflag_ty::BidiLoop) && ((idx - length) < SpillMax)) {
            const auto i = idx - length;
            return chan.spill_fwd[i];
        } else {
            return 0;
        }
    } else if (idx < loop_start) {
        if (bitset_is_set(chan.flags, vflag_ty::Loop, vflag_ty::BidiLoop) && ((loop_start - idx) < SpillMax)) {
            const auto i = loop_start - idx;
            return chan.spill_back[i];
        } else {
            return 0;
        }
    }
    return buf[idx] * NORM_INT16;
}

template <bool use_sinc>
void
resample_janky(sample_t *left, sample_t *right, size_t out_frames, modchannel_t &src) {
    const auto buf = reinterpret_cast<const int16_t * const>(src.active_sample_data);
    const auto left_vol = static_cast<sample_t>(src.left_volume);
    const auto right_vol = static_cast<sample_t>(src.right_volume);
    int32_t pos = src.sample_position;
    int32_t frac_pos = src.fractional_sample_position;

    while (out_frames--) {
        if (use_sinc) {
            const auto fir_idx = ((pos & 0xfff0) >> 4) * FIR_TAPS;
            const sample_t *fir_table =
                ((src.position_delta > 0x13000 || src.position_delta < -0x13000)
                ? downsample_sinc_table
                : upsample_sinc_table) + fir_idx;
            sample_t sample = 0;
            /*
            for (size_t tap = 0; tap < FIR_TAPS; ++tap) {
                //sample += fir_table[tap] * (lookup(src, tap - FIR_CENTER + pos, buf) * NORM_INT16);
                //sample += fir_table[tap] * buf[pos] * NORM_INT16;
            }
            */
            #if 1
            if ((pos < (src.loop_start + 3)) || (pos >= (src.length - 4))) {
                sample += fir_table[0] * (lookup(src, pos - 3, buf) * NORM_INT16);
                sample += fir_table[1] * (lookup(src, pos - 2, buf) * NORM_INT16);
                sample += fir_table[2] * (lookup(src, pos - 1, buf) * NORM_INT16);
                sample += fir_table[3] * (buf[pos] * NORM_INT16);
                sample += fir_table[4] * (lookup(src, pos + 1, buf) * NORM_INT16);
                sample += fir_table[5] * (lookup(src, pos + 2, buf) * NORM_INT16);
                sample += fir_table[6] * (lookup(src, pos + 3, buf) * NORM_INT16);
                sample += fir_table[7] * (lookup(src, pos + 4, buf) * NORM_INT16);
            #else
            if ((pos < (src.loop_start + 3)) || (pos >= (src.length - 4))) {
                sample += fir_table[0] * (lookup_spill(src, pos - 3, buf) );
                sample += fir_table[1] * (lookup_spill(src, pos - 2, buf) );
                sample += fir_table[2] * (lookup_spill(src, pos - 1, buf) );
                sample += fir_table[3] * (buf[pos] * NORM_INT16);
                sample += fir_table[4] * (lookup_spill(src, pos + 1, buf) );
                sample += fir_table[5] * (lookup_spill(src, pos + 2, buf) );
                sample += fir_table[6] * (lookup_spill(src, pos + 3, buf) );
                sample += fir_table[7] * (lookup_spill(src, pos + 4, buf) );
            #endif
            } else {
                sample += fir_table[0] * (buf[pos - 3] * NORM_INT16);
                sample += fir_table[1] * (buf[pos - 2] * NORM_INT16);
                sample += fir_table[2] * (buf[pos - 1] * NORM_INT16);
                sample += fir_table[3] * (buf[pos] * NORM_INT16);
                sample += fir_table[4] * (buf[pos - 1] * NORM_INT16);
                sample += fir_table[5] * (buf[pos - 2] * NORM_INT16);
                sample += fir_table[6] * (buf[pos - 3] * NORM_INT16);
                sample += fir_table[7] * (buf[pos - 4] * NORM_INT16);
            }
            // */

            *left += left_vol * VolScale * sample;
            *right += right_vol * VolScale * sample;
        } else {
            const auto normalized = buf[pos] * NORM_INT16;
            *left += left_vol * VolScale * normalized;
            *right += right_vol * VolScale * normalized;
        }
        ++left;
        ++right;
        {
            const auto newfrac = frac_pos + src.position_delta;
            pos += newfrac >> 16;
            frac_pos = newfrac & 0xffff;
        }
        if (pos >= src.length) {
            if (bitset_is_set(src.flags, vflag_ty::BidiLoop)) {
                frac_pos = 0x10000 - frac_pos;
                pos = src.length - (pos - src.length) - (frac_pos >> 16) - 1;
                frac_pos &= 0xffff;
                src.position_delta = -src.position_delta;
                bitset_add(src.flags, vflag_ty::ScrubBackwards);
                continue;
            } else if (bitset_is_set(src.flags, vflag_ty::Loop)) {
                pos = src.loop_start + (src.length - pos);
                continue;
            } else {
                break;
            }
        } else if (pos < src.loop_start) {
            if (bitset_is_set(src.flags, vflag_ty::ScrubBackwards) && (src.position_delta < 0)) {
                frac_pos = 0x10000 - frac_pos;
                pos = src.loop_start + (src.loop_start - pos) - (frac_pos >> 16) + 1;
                frac_pos &= 0xffff;
                src.position_delta = -src.position_delta;
                bitset_remove(src.flags, vflag_ty::ScrubBackwards);
                continue;
            } else if (src.position_delta > 0) {
                continue;
            } else {
                break;
            }
        }
    }
    /*
    invariant: src.fractional_sample_position \in [0, 0xffff]
    */
    src.sample_position = pos;
    src.fractional_sample_position = frac_pos;
}

void
resample_janky_no_interpolation(sample_t *left, sample_t *right, size_t out_frames, modchannel_t &src) {
    return resample_janky<false>(left, right, out_frames, src);
}

void
resample_janky_windowed_sinc(sample_t *left, sample_t *right, size_t out_frames, modchannel_t &src) {
    return resample_janky<true>(left, right, out_frames, src);
}

void
test_resample_janky(const outbuf_ty &dst, modchannel_t &chan) {
    ghettotimer timer(__FUNCTION__);
    int count = LoopCount;
    auto left = dst.left;
    auto right = dst.right;
    while (true) {
        const auto remainder = mymin<size_t>(TestChunkSize, chan.length - chan.sample_position);
        if (chan.sample_position >= chan.length) break;
        //resample_janky_no_interpolation(left, right, remainder, chan);
        resample_janky_windowed_sinc(left, right, remainder, chan);
        left += remainder;
        right += remainder;
        #if USE_COUNT
        count -= remainder;
        #endif
        if (count < 0) break;
    }
}

void
chan_update_loop_points(modchannel_t &chan, const int32_t begin, const int32_t end) {
    const auto length = end - begin;
    chan.length = begin + length;
    chan.loop_start = begin;
    chan.loop_end = end;
    update_spill_bufs_in_place<int16_t>(chan);
}

int
main(int argc, char **argv) {
    modplug::mixgraph::init_tables();
    const size_t frames = 63417056 * 2;
    auto left = new sample_t[frames];
    auto right = new sample_t[frames];
    outbuf_ty outbuf = { left, right, frames };
    const auto sample = load_sample("D:\\Users\\imran\\hugedoot.wav");
    printf("sample buf addr = %p, frames = %u, chans = %u\n",
        sample.buf, sample.num_frames, sample.channels);
    fflush(stdout);

    #if 1
    {
        clear_outbuf(outbuf);
        auto chan = sample_to_channel(sample);
        chan.position_delta = hi_16_16(1);
        //chan.position_delta = fp64_to_16_16(2.5);
        /*
        chan.flags = CHN_LOOP | CHN_PINGPONGLOOP;
        chan.length = 8000000;
        chan.loop_start = 7000000;
        chan.loop_end = chan.length;
        // */
        // /*
        chan.position_delta = hi_16_16(10);
        bitset_add(chan.flags, vflag_ty::Loop);
        bitset_add(chan.flags, vflag_ty::BidiLoop);
        chan_update_loop_points(chan, 8000000, 8001001);
        // */
        test_resample(chan, outbuf);
        print_outbuf(outbuf, 40);
        write_outbuf(outpath2, outbuf);
    }
    #endif
    fflush(stdout);

    #if 1
    {
        clear_outbuf(outbuf);
        auto chan = sample_to_channel(sample);
        chan.position_delta = hi_16_16(1);
        //chan.position_delta = fp64_to_16_16(2.5);
        /*
        chan.flags = CHN_LOOP | CHN_PINGPONGLOOP;
        chan.length = 8000000;
        chan.loop_start = 7000000;
        chan.loop_end = chan.length;
        // */
        // /*
        chan.position_delta = hi_16_16(10);
        bitset_add(chan.flags, vflag_ty::Loop);
        bitset_add(chan.flags, vflag_ty::BidiLoop);
        chan_update_loop_points(chan, 8000000, 8001001);
        // */
        test_resample_janky(outbuf, chan);
        print_outbuf(outbuf, 40);
        write_outbuf(outpath1, outbuf);
    }
    #endif
    fflush(stdout);

    #if 0
    {
        clear_outbuf(outbuf);
        int16_t buf[20] =
            /*
            { 1, 0, 2, 0, 3, 0, 4, 0, 5, 0
            , 6, 0, 7, 0, 8, 0, 9, 0, 10, 0
            };
            */
            { 111, 222, 333, 444, 555, 666, 777, 888, 999, 0
            , 6, 0, 7, 0, 8, 0, 9, 0, 10, 0
            };
        sample_ty smp;
        smp.buf = buf;
        smp.channels = 1;
        smp.num_frames = 20;
        auto chan = sample_to_channel(smp);
        chan.flags = CHN_LOOP | CHN_PINGPONGLOOP;
        chan_update_loop_points(chan, 0, 3);
        chan.position_delta = hi_16_16(1);
        test_resample_janky(outbuf, chan);
        print_outbuf(outbuf, 30);
    }
    #endif

    return 0;
}
