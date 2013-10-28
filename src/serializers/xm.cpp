#include "stdafx.h"

#include "xm.h"
#include "legacy_util.h"

using namespace modplug::tracker;

namespace modplug {
namespace serializers {
namespace xm {

namespace read_internal {
using namespace modplug::serializers::binaryparse;
using namespace modplug::serializers;

file_header_ty
read_file_header(context &ctx) {
    file_header_ty header;
    auto buf = read_bytestring_str(ctx, 15);
    if (buf != "Extended Module") throw invalid_data_exception();
    read_skip(ctx, 2);
    auto name          = read_bytestring(ctx, 20);
    auto tag           = read_uint8(ctx);
    auto tracker_name  = read_bytestring(ctx, 20);
    header.version     = read_uint16_le(ctx);
    header.size        = read_uint32_le(ctx);
    header.orders      = read_uint16_le(ctx);
    header.restartpos  = read_uint16_le(ctx);
    header.channels    = read_uint16_le(ctx);
    header.patterns    = read_uint16_le(ctx);
    header.instruments = read_uint16_le(ctx);
    header.flags       = read_uint16_le(ctx);
    header.speed       = read_uint16_le(ctx);
    header.tempo       = read_uint16_le(ctx);
    return header;
}

orderlist_t
read_orderlist(context &ctx, file_header_ty &header) {
    return read_uint8_orderlist(ctx, header.orders);
}

pattern_header_ty
read_pattern_header(context &ctx) {
    pattern_header_ty ret;
    ret.length       = read_uint32_le(ctx);
    ret.packing_type = read_uint8(ctx);
    ret.row_length   = read_uint16_le(ctx);
    ret.data_length  = read_uint16_le(ctx);
    return ret;
}

std::vector<uint8_t>
read_pattern_data(context &ctx, pattern_header_ty &header) {
    return read_bytestring(ctx, header.data_length);
}

std::vector<pattern_ty>
read_patterns(context &ctx, file_header_ty &header) {
    std::vector<pattern_ty> ret;
    ret.reserve(header.patterns);
    for (uint16_t i = 0; i < header.patterns; ++i) {
        auto header = read_pattern_header(ctx);
        auto data = read_pattern_data(ctx, header);
        pattern_ty item = { header, data };
        ret.push_back(item);
    }
    return ret;
}

orderlist_t
read(context &ctx) {
    auto header = read_file_header(ctx);
    auto orderlist = read_orderlist(ctx, header);
    auto patterns = read_patterns(ctx, header);
    return orderlist;
}


}

orderlist_t
read(modplug::serializers::binaryparse::context &ctx) {
    return read_internal::read(ctx);
}


}
}
}
