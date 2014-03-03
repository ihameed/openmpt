#include "../openmpt-lib/stdafx.h"
#include "../openmpt-lib/main.h"

#include "../openmpt-lib/tracker/tracker.hpp"
#include "../openmpt-lib/tracker/eval.hpp"
#include "../openmpt-lib/tracker/mixer.hpp"

#include "../openmpt-lib/mixgraph/mixer.hpp"

#include "../openmpt-lib/legacy_soundlib/old_utils.h"

#include <cstdio>

#include <sndfile.h>

//const size_t TestChunkSize = 32;
const size_t TestChunkSize = 1024 * 1024 * 50;
//const int LoopCount = 1024 * 1024 * 50;
const int LoopCount = 40;
#define USE_COUNT 1

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

using modplug::mixgraph::internal::resample_and_mix_windowed_sinc;
using modplug::mixgraph::upsample_sinc_table;
using modplug::mixgraph::downsample_sinc_table;
using namespace modplug::tracker;
using namespace modplug::pervasives;

CTrackApp theApp;

struct outbuf_ty {
    sample_t *left;
    sample_t *right;
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
    auto interleaved = new double[tmplen];
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
    const voice_ty &chan, sample_t *dst, int32_t offset, int32_t direction)
{
    const auto buf = chan.active_sample_data.int16;
    const int32_t loop_end = chan.loop_end;
    const int32_t loop_start = chan.loop_start;
    for (size_t i = 0; i < SpillMax; ++i) {
        dst[i] = buf[offset] * NormInt16;
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
    const voice_ty &chan, sample_t *dst, int32_t offset, int32_t direction)
{
    const auto buf = chan.active_sample_data.int16;
    const int32_t loop_end = chan.loop_end;
    const int32_t loop_start = chan.loop_start;
    for (size_t i = 0; i < SpillMax; ++i) {
        dst[i] = buf[offset] * NormInt16;
        if (offset >= (loop_end - 1)) offset = loop_start;
        else if (offset <= loop_start) offset = loop_end - 1;
        else offset += direction;
    }
}

template <typename Ty>
void
update_spill_bufs_in_place(voice_ty &chan) {
    if (chan.active_sample_data.generic) {
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

voice_ty
sample_to_channel(const sample_ty &val) {
    voice_ty ret = { 0 };
    ret.m_Freq = 44100;
    ret.sample_tag = stag_ty::Int16;
    ret.length = val.num_frames;
    ret.loop_start = 0;
    ret.loop_end = 0;
    ret.left_volume = 4096;
    ret.right_volume = 4096;
    ret.active_sample_data.int16 = val.buf;
    ret.sample_data.int16 = val.buf;
    return ret;
}

const sample_t VolScale = 1.0 / 4096.0;
void
test_resample(const voice_ty &chan_, outbuf_ty &out) {
    using namespace modplug::tracker;
    ghettotimer timer(__FUNCTION__);
    auto chan = chan_;
    auto left = out.left;
    auto right = out.right;
    int count = LoopCount;
    while (true) {
        const auto remainder = GetSampleCount(chan, TestChunkSize, false);
        if (remainder <= 0) break;
        //modplug::mixgraph::resample_and_mix(left, right, &chan, remainder);
        modplug::mixgraph::internal::resample_and_mix_inner<int16_f, false>(left, right, chan, remainder);
        //modplug::mixgraph::resample_and_mix_inner<int16_f, true>(left, right, chan, remainder);
        //advance_silently(chan, remainder);
        left += remainder;
        right += remainder;
        #if USE_COUNT
        count -= remainder;
        #endif
        if (count < 0) break;
    }
}

void
test_resample_janky(const outbuf_ty &dst, voice_ty &chan) {
    ghettotimer timer(__FUNCTION__);
    int count = LoopCount;
    auto left = dst.left;
    auto right = dst.right;
    while (true) {
        const auto remainder = mymin<size_t>(TestChunkSize, chan.length - chan.sample_position);
        if (remainder <= 0) break;
        modplug::tracker::mix_and_advance(left, right, remainder, &chan);
        left += remainder;
        right += remainder;
        #if USE_COUNT
        count -= remainder;
        #endif
        if (count < 0) break;
    }
}

void
chan_update_loop_points(voice_ty &chan, const int32_t begin, const int32_t end) {
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

    #if 0
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
         /*
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

    #if 0
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
         /*
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

    #if 1
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
        chan.flags = bitset_set(bitset_set(vflags_ty(vflag_ty::Loop),
            vflag_ty::BidiLoop), vflag_ty::DisableInterpolation);
        chan_update_loop_points(chan, 3, 6);
        chan.position_delta = hi_16_16(15);
        test_resample_janky(outbuf, chan);
        print_outbuf(outbuf, 30);
    }
    #endif

    return 0;
}
