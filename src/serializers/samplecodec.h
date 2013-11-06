#include <cstdint>

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

template <typename in_t, typename out_t>
struct byte_convert_ty;

template <>
struct byte_convert_ty <uint8_t, int16_t> {
    const static size_t incr_by = 2;
    int16_t operator () (const uint8_t *in) {
        return
            static_cast<uint16_t>(in[0]) |
            static_cast<uint16_t>(in[1]) << 4;
    };
};

template <>
struct byte_convert_ty <uint8_t, int8_t> {
    const static size_t incr_by = 1;
    int8_t operator () (const uint8_t *in) {
        return static_cast<int8_t>(*in);
    };
};

template <typename in_t, typename out_t>
struct identity_convert_ty {
    out_t operator () (const in_t x) { return x; };
};

template <
    template <typename, typename> class ConvertTy,
    template <typename> class DecodeTy,
    template <typename> class DitherTy,
    template <typename, typename> class FinalConvertTy,
    typename IntermediateTy,
    typename FinalTy,
    typename SrcTy
>
void
copy_and_convert(FinalTy * dst, const SrcTy * const src, const size_t num_frames)
{
    typedef ConvertTy<SrcTy, IntermediateTy> convert_ty;
    convert_ty pre_convert;
    DecodeTy<IntermediateTy> decode;
    DitherTy<IntermediateTy> dither;
    FinalConvertTy<IntermediateTy, FinalTy> final_convert;
    const size_t incr_by = convert_ty::incr_by;
    for (size_t i = 0; i < num_frames; i += incr_by) {
        *dst = final_convert(dither(decode(pre_convert(src + i))));
        ++dst;
    }
}


}
}
}
