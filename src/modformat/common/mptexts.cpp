#include "stdafx.h"

#include <cstdlib>

#include "mptexts.hpp"
#include "../../pervasives/binaryparse.hpp"

using namespace modplug::pervasives::binaryparse;

namespace modplug {
namespace modformat {
namespace mptexts {

boost::optional<instr_tag_ty>
instr_tag_of_bytes(const uint32_t raw) {
    switch (raw) {
#define MODPLUG_EXPAND_TAG(bytes, datatype, tycon) case ( bytes ) : return instr_tag_ty::tycon;
MODPLUG_FIELD_TAGS(MODPLUG_EXPAND_TAG)
#undef MODPLUG_EXPAND_TAG
    default : return boost::none;
    };
}

uint32_t
bytes_of_instr_tag(const instr_tag_ty tag) {
    switch (tag) {
#define MODPLUG_EXPAND_TAG(bytes, datatype, tycon) case ( instr_tag_ty::tycon ) : return bytes;
MODPLUG_FIELD_TAGS(MODPLUG_EXPAND_TAG);
#undef MODPLUG_EXPAND_TAG
    default: abort(); return 0;
    };
}

boost::optional<song_tag_ty>
song_tag_of_bytes(const uint32_t raw) {
    switch (raw) {
#define MODPLUG_EXPAND_TAG(bytes, datatype, tycon) case ( bytes ) : return song_tag_ty::tycon;
MODPLUG_SONG_FIELD_TAGS(MODPLUG_EXPAND_TAG)
#undef MODPLUG_EXPAND_TAG
    default : return boost::none;
    };
}

uint32_t
bytes_of_song_tag(const song_tag_ty tag) {
    switch (tag) {
#define MODPLUG_EXPAND_TAG(bytes, datatype, tycon) case ( song_tag_ty::tycon ) : return bytes;
MODPLUG_SONG_FIELD_TAGS(MODPLUG_EXPAND_TAG);
#undef MODPLUG_EXPAND_TAG
    default: abort(); return 0;
    };
}

template <typename Ty>
boost::optional<Ty>
readval(context &ctx, const uint16_t size) {
    read_skip(ctx, size);
    return boost::none;
}

template <>
boost::optional<uint8_t>
readval<uint8_t>(context &ctx, const uint16_t size) {
    auto ret = read_uint8(ctx);
    if (1 < size) read_skip(ctx, size - 1);
    return ret;
}

template <>
boost::optional<tempo_mode_ty>
readval<tempo_mode_ty>(context &ctx, const uint16_t size) {
    auto raw = read_uint8(ctx);
    boost::optional<tempo_mode_ty> ret;
    switch (raw) {
    case 0: ret = tempo_mode_ty::Classic; break;
    case 1: ret = tempo_mode_ty::Alternative; break;
    case 2: ret = tempo_mode_ty::Modern; break;
    }
    if (1 < size) read_skip(ctx, size - 1);
    return ret;
}

template <>
boost::optional<uint16_t>
readval<uint16_t>(context &ctx, const uint16_t size) {
    auto ret = read_uint16_le(ctx);
    if (2 < size) read_skip(ctx, size - 2);
    return ret;
}

template <>
boost::optional<uint32_t>
readval<uint32_t>(context &ctx, const uint16_t size) {
    auto ret = read_uint32_le(ctx);
    if (4 < size) read_skip(ctx, size - 4);
    return ret;
}

nullptr_t
inject_extended_instr_data(
    context &ctx, const instr_tag_ty tag, const uint16_t size,
    mpt_instrument_ty &ref)
{
    switch (tag) {
#define MODPLUG_EXPAND_TAG(bytes, datatype, tycon) \
    case ( instr_tag_ty::tycon ) : \
        ref.field_##tycon = readval< datatype > (ctx, size); \
        break;
MODPLUG_FIELD_TAGS(MODPLUG_EXPAND_TAG)
#undef MODPLUG_EXPAND_TAG
    default: abort();
    }
    return nullptr;
}

nullptr_t
inject_extended_song_data(
    context &ctx, const song_tag_ty tag, const uint16_t size,
    mpt_song_ty &ref)
{
    switch (tag) {
#define MODPLUG_EXPAND_TAG(bytes, datatype, tycon) \
    case ( song_tag_ty::tycon ) : \
        ref.field_##tycon = readval< datatype > (ctx, size); \
        break;
MODPLUG_SONG_FIELD_TAGS(MODPLUG_EXPAND_TAG)
#undef MODPLUG_EXPAND_TAG
    default: abort();
    }
    return nullptr;
}

}
}
}
