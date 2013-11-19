#include <cstdint>

#include <type_traits>

namespace modplug {
namespace serializers {
namespace samplecodec {

template <typename Ty>
struct delta_decode {
    delta_decode() : acc(0) { };
    Ty operator () (const Ty in) {
        Ty out = acc + in;
        this->acc = out;
        return out;
    };
    Ty acc;
};

template <typename Ty>
struct fixed_size_type;

template<>
struct fixed_size_type<int16_t> {
    const static size_t width = 2;
};

template <typename InTy, typename OutTy>
struct signed_bytes_ty;

template <>
struct signed_bytes_ty <uint8_t, int16_t> {
    const static size_t incr_by = 2;
    int16_t operator () (const uint8_t *in) {
        return static_cast<int16_t>(
            static_cast<uint16_t>(in[0]) |
            static_cast<uint16_t>(in[1]) << 8);
    };
};

template <>
struct signed_bytes_ty <uint8_t, int8_t> {
    const static size_t incr_by = 1;
    int8_t operator () (const uint8_t *in) {
        return static_cast<int8_t>(*in);
    };
};

template <typename InTy, typename OutTy>
struct unsigned_bytes_ty {
    typedef signed_bytes_ty<InTy, OutTy> wrapped_type;
    typedef typename std::make_unsigned<OutTy>::type u_ty;
    const static size_t incr_by = wrapped_type::incr_by;
    OutTy operator () (const InTy *in) {
        const u_ty rotate = (std::numeric_limits<u_ty>::max() / 2) + 1;
        const u_ty ret = static_cast<u_ty>(wrapped(in)) - rotate;
        return ret;
    }
    wrapped_type wrapped;
};

template <typename InTy, typename OutTy>
struct identity_convert_ty {
    OutTy operator () (const InTy x) { return x; };
};

template <
    template <typename, typename> class PreConvertTy,
    template <typename> class DecodeTy,
    template <typename> class DitherTy,
    template <typename, typename> class FinalConvertTy,
    typename SrcTy,
    typename IntermediateTy,
    typename FinalTy,
    size_t Chans >
struct convert_ty {
    static_assert(Chans > 0, "copy_interleaved on a zero-channel stream is nonsensical!");
    typedef PreConvertTy<SrcTy, IntermediateTy> pre_convert_ty;
    pre_convert_ty pre_convert;
    DecodeTy<IntermediateTy> decode[Chans];
    DitherTy<IntermediateTy> dither;
    FinalConvertTy<IntermediateTy, FinalTy> final_convert;
    const static size_t incr_by = pre_convert_ty::incr_by;

    void interleaved(FinalTy *dst, const SrcTy * src, const size_t num_frames) {
        const size_t channel_entries = num_frames * incr_by;
        for (size_t i = 0; i < num_frames; ++i) {
            for (size_t chan = 0; chan < Chans; ++chan) {
                *dst = final_convert(dither(decode[chan](pre_convert(src))));
                ++dst;
                src += incr_by;
            }
        }
    }

    void non_interleaved(FinalTy *dst, const SrcTy * src, const size_t num_frames) {
        const size_t channel_entries = num_frames * incr_by;
        for (size_t chan = 0; chan < Chans; ++chan) {
            for (size_t i = 0; i < num_frames; ++i) {
                const auto idx = i * Chans;
                dst[idx] = final_convert(dither(decode[chan](pre_convert(src))));
                src += incr_by;
            }
            ++dst;
        }
    }
};

template <
    template <typename, typename> class PreConvertTy,
    template <typename> class DecodeTy,
    size_t Chans,
    typename SrcTy,
    typename FinalTy >
void
copy_interleaved(FinalTy * dst, const SrcTy * const src, const size_t num_frames) {
    convert_ty
        < PreConvertTy, DecodeTy, std::identity, identity_convert_ty,
          SrcTy, FinalTy, FinalTy, Chans >
        conv;
    conv.interleaved(dst, src, num_frames);
}

template <
    template <typename, typename> class PreConvertTy,
    template <typename> class DecodeTy,
    size_t Chans,
    typename SrcTy,
    typename FinalTy >
void
copy_non_interleaved(FinalTy * dst, const SrcTy * const src, const size_t num_frames) {
    convert_ty
        < PreConvertTy, DecodeTy, std::identity, identity_convert_ty,
          SrcTy, FinalTy, FinalTy, Chans >
        conv;
    conv.non_interleaved(dst, src, num_frames);
}


}
}
}
