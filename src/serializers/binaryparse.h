#pragma once

#include <cstdint>
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
    std::shared_ptr<const std::vector<uint8_t>> buf;
    size_t offset;
};

inline void
check_size(context &ctx, size_t size) {
    if (std::numeric_limits<size_t>::max() - size < ctx.offset) {
        throw overflow_exception();
    } else if (size + ctx.offset >= ctx.buf->size()) {
        throw buffer_too_short_exception();
    }
}

inline context
mkcontext(std::shared_ptr<std::vector<uint8_t>> buf) {
    context ret = { buf, 0 };
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
    auto &buf = *ctx.buf;
    uint8_t ret = buf[ctx.offset];
    ctx.offset += 1;
    return ret;
}

inline uint16_t
read_uint16_be(context &ctx) {
    check_size(ctx, 2);
    auto &buf = *ctx.buf;
    uint16_t ret =
        (static_cast<uint16_t>(buf[ctx.offset]) << 8) |
        (static_cast<uint16_t>(buf[ctx.offset]));
    ctx.offset += 2;
    return ret;
}

inline uint32_t
read_uint32_be(context &ctx) {
    check_size(ctx, 4);
    auto &buf = *ctx.buf;
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
    auto &buf = *ctx.buf;
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
    auto &buf = *ctx.buf;
    uint16_t ret =
        (static_cast<uint16_t>(buf[ctx.offset + 1]) << 8) |
        (static_cast<uint16_t>(buf[ctx.offset]));
    ctx.offset += 2;
    return ret;
}

inline uint32_t
read_uint32_le(context &ctx) {
    check_size(ctx, 4);
    auto &buf = *ctx.buf;
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
    auto &buf = *ctx.buf;
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
    auto &buf = *ctx.buf;
    auto begin = buf.begin() + ctx.offset;
    auto end = begin + length;
    ret.assign(begin, end);
    ctx.offset += length;
    return ret;
}

inline std::string
read_bytestring_str(context &ctx, size_t length) {
    check_size(ctx, length);
    std::string ret;
    ret.reserve(length);
    auto &buf = *ctx.buf;
    auto begin = buf.begin() + ctx.offset;
    auto end = begin + length;
    ret.assign(begin, end);
    ctx.offset += length;
    return ret;
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

}
}
}
