#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <exception>

namespace modplug {
namespace pervasives {
namespace binaryprint {

struct overflow_exception : public std::exception {
    const char* what() const throw () override {
        return "binaryprint offset overflow!";
    }
};

struct context {
    std::shared_ptr<std::vector<uint8_t>> buf;
};

inline void check_size(context &ctx, size_t size) {
    auto &buf = *ctx.buf;
    if (std::numeric_limits<size_t>::max() - size < buf.size()) {
        throw overflow_exception();
    }
}

inline context mkcontext(std::shared_ptr<std::vector<uint8_t>> buf) {
    context ret = { buf };
    return ret;
}

inline void write_uint8(context &ctx, uint8_t val) {
    check_size(ctx, 1);
    auto &buf = *ctx.buf;
    buf.push_back(val);
};

inline void write_uint16_be(context &ctx, uint16_t val) {
    check_size(ctx, 2);
    auto &buf = *ctx.buf;
    buf.push_back(static_cast<uint8_t>(val >> 8));
    buf.push_back(static_cast<uint8_t>(val));
};

inline void write_uint32_be(context &ctx, uint32_t val) {
    check_size(ctx, 4);
    auto &buf = *ctx.buf;
    buf.push_back(static_cast<uint8_t>(val >> 24));
    buf.push_back(static_cast<uint8_t>(val >> 16));
    buf.push_back(static_cast<uint8_t>(val >> 8));
    buf.push_back(static_cast<uint8_t>(val));
};

inline void write_uint64_be(context &ctx, uint64_t val) {
    check_size(ctx, 8);
    auto &buf = *ctx.buf;
    buf.push_back(static_cast<uint8_t>(val >> 56));
    buf.push_back(static_cast<uint8_t>(val >> 48));
    buf.push_back(static_cast<uint8_t>(val >> 40));
    buf.push_back(static_cast<uint8_t>(val >> 32));
    buf.push_back(static_cast<uint8_t>(val >> 24));
    buf.push_back(static_cast<uint8_t>(val >> 16));
    buf.push_back(static_cast<uint8_t>(val >> 8));
    buf.push_back(static_cast<uint8_t>(val));
};

inline void write_bytestring(context &ctx, const std::vector<uint8_t> &val) {
    check_size(ctx, val.size());
    auto &buf = *ctx.buf;
    buf.insert(buf.end(), val.begin(), val.end());
}

inline void write_bytestring(context &ctx, const std::string &val) {
    check_size(ctx, val.size());
    auto &buf = *ctx.buf;
    buf.insert(buf.end(), val.begin(), val.end());
}

inline void write_bytestring(context &ctx, const char *val) {
    size_t size = strlen(val);
    check_size(ctx, size);
    auto &buf = *ctx.buf;
    buf.insert(buf.end(), val, val + size);
}

}
}
}
