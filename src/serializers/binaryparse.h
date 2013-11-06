#pragma once

#include <cstring>
#include <cstdint>
#include <array>
#include <vector>
#include <memory>
#include <exception>

#include "boost/optional.hpp"

namespace modplug {
namespace serializers {
namespace binaryparse {

struct overflow_exception : public std::exception {
    const char* what() const throw () override {
        return "binaryparse offset overflow!";
    }
};

struct buffer_too_short_exception : public std::exception {
    const char* what() const throw () override {
        return "binaryparse buffer too small!";
    }
};

struct context {
    const std::shared_ptr<const uint8_t> buf;
    const size_t size;
    const uint8_t *data;
    size_t offset;
};

inline void
check_size(context &ctx, size_t requested) {
    if (std::numeric_limits<size_t>::max() - requested < ctx.offset) {
        throw overflow_exception();
    } else if (requested + ctx.offset > ctx.size) {
        throw buffer_too_short_exception();
    }
}

inline context
mkcontext(std::shared_ptr<const uint8_t> buf, size_t size) {
    context ret = { buf, size, buf.get(), 0 };
    return ret;
}

inline void
read_skip(context &ctx, size_t size) {
    check_size(ctx, size);
    ctx.offset += size;
}

inline uint8_t
read_uint8(context &ctx) {
    check_size(ctx, 1);
    auto buf = ctx.data;
    uint8_t ret = buf[ctx.offset];
    ctx.offset += 1;
    return ret;
}

inline uint16_t
read_uint16_be(context &ctx) {
    check_size(ctx, 2);
    auto buf = ctx.data;
    uint16_t ret =
        (static_cast<uint16_t>(buf[ctx.offset]) << 8) |
        (static_cast<uint16_t>(buf[ctx.offset]));
    ctx.offset += 2;
    return ret;
}

inline uint32_t
read_uint32_be(context &ctx) {
    check_size(ctx, 4);
    auto buf = ctx.data;
    uint32_t ret =
        (static_cast<uint32_t>(buf[ctx.offset])     << 24) |
        (static_cast<uint32_t>(buf[ctx.offset + 1]) << 16) |
        (static_cast<uint32_t>(buf[ctx.offset + 2]) << 8 ) |
        (static_cast<uint32_t>(buf[ctx.offset + 3]));
    ctx.offset += 4;
    return ret;
}

inline uint64_t
read_uint64_be(context &ctx) {
    check_size(ctx, 8);
    auto buf = ctx.data;
    uint64_t ret =
        (static_cast<uint64_t>(buf[ctx.offset])     << 56) |
        (static_cast<uint64_t>(buf[ctx.offset + 1]) << 48) |
        (static_cast<uint64_t>(buf[ctx.offset + 2]) << 40) |
        (static_cast<uint64_t>(buf[ctx.offset + 3]) << 32) |
        (static_cast<uint64_t>(buf[ctx.offset + 4]) << 24) |
        (static_cast<uint64_t>(buf[ctx.offset + 5]) << 16) |
        (static_cast<uint64_t>(buf[ctx.offset + 6]) << 8 ) |
        (static_cast<uint64_t>(buf[ctx.offset + 7]));
    ctx.offset += 8;
    return ret;
}

inline uint16_t
read_uint16_le(context &ctx) {
    check_size(ctx, 2);
    auto buf = ctx.data;
    uint16_t ret =
        (static_cast<uint16_t>(buf[ctx.offset + 1]) << 8) |
        (static_cast<uint16_t>(buf[ctx.offset]));
    ctx.offset += 2;
    return ret;
}

inline uint32_t
read_uint32_le(context &ctx) {
    check_size(ctx, 4);
    auto buf = ctx.data;
    uint32_t ret =
        (static_cast<uint32_t>(buf[ctx.offset + 3]) << 24) |
        (static_cast<uint32_t>(buf[ctx.offset + 2]) << 16) |
        (static_cast<uint32_t>(buf[ctx.offset + 1]) << 8 ) |
        (static_cast<uint32_t>(buf[ctx.offset]));
    ctx.offset += 4;
    return ret;
}

inline uint64_t
read_uint64_le(context &ctx) {
    check_size(ctx, 8);
    auto buf = ctx.data;
    uint64_t ret =
        (static_cast<uint64_t>(buf[ctx.offset + 7]) << 56) |
        (static_cast<uint64_t>(buf[ctx.offset + 6]) << 48) |
        (static_cast<uint64_t>(buf[ctx.offset + 5]) << 40) |
        (static_cast<uint64_t>(buf[ctx.offset + 4]) << 32) |
        (static_cast<uint64_t>(buf[ctx.offset + 3]) << 24) |
        (static_cast<uint64_t>(buf[ctx.offset + 2]) << 16) |
        (static_cast<uint64_t>(buf[ctx.offset + 1]) << 8 ) |
        (static_cast<uint64_t>(buf[ctx.offset]));
    ctx.offset += 8;
    return ret;
}

inline std::vector<uint8_t>
read_bytestring(context &ctx, size_t length) {
    check_size(ctx, length);
    std::vector<uint8_t> ret;
    ret.reserve(length);
    auto buf = ctx.data;
    auto begin = buf + ctx.offset;
    auto end = begin + length;
    ret.assign(begin, end);
    ctx.offset += length;
    return ret;
}

inline const uint8_t *
read_bytestring_slice(context &ctx, size_t length) {
    check_size(ctx, length);
    auto ret = &ctx.data[ctx.offset];
    ctx.offset += length;
    return ret;
}

inline std::string
read_bytestring_str(context &ctx, size_t length) {
    check_size(ctx, length);
    std::string ret;
    ret.reserve(length);
    auto buf = ctx.data;
    auto begin = buf + ctx.offset;
    auto end = begin + length;
    ret.assign(begin, end);
    ctx.offset += length;
    return ret;
}

inline void
read_bytestring_ptr(context &ctx, size_t length, uint8_t *dst) {
    check_size(ctx, length);
    memcpy(dst, ctx.data + ctx.offset, length);
    ctx.offset += length;
}

template <size_t length>
inline void
read_bytestring_arr(context &ctx, std::array<uint8_t, length> &arr) {
    check_size(ctx, length);
    memcpy(arr.data(), ctx.data + ctx.offset, length);
    ctx.offset += length;
}

template <size_t length, typename Ty>
inline void zero_fill(std::array<Ty, length> &arr) {
    memset(arr.data(), 0, sizeof(Ty) * length);
}

template <typename FunTy>
auto
read_limit(context &ctx, size_t limit, FunTy fun) -> decltype(fun(ctx)) {
    check_size(ctx, limit);
    context dup_ctx = {
        ctx.buf,
        ctx.offset + limit,
        ctx.data,
        ctx.offset
    };
}

template <typename FunTy>
auto
read_lookahead(context &ctx, FunTy fun) -> decltype(fun(ctx)) {
    context dup_ctx = ctx;
    return fun(dup_ctx);
}

template <typename FunTy>
auto
read_try(context &ctx, FunTy fun) -> boost::optional<decltype(fun(ctx))> {
    context dup_ctx = ctx;
    try {
        return fun(dup_ctx);
    } catch (overflow_exception e) {
        return boost::none;
    } catch (buffer_too_short_exception e) {
        return boost::none;
    }
}

template <typename FunTy>
auto
read_try_limit(context &ctx, size_t limit, FunTy fun) -> decltype(read_try(ctx, fun)) {
    return read_try(ctx, [&] (context &ctx) {
        return read_limit(ctx, limit, fun);
    });
}

}
}
}
