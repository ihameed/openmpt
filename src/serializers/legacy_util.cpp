#include "stdafx.h"

#include "legacy_util.hpp"

#include "../legacy_soundlib/Sndfile.h"

using namespace modplug::tracker;
namespace binaryparse = modplug::pervasives::binaryparse;

namespace modplug {
namespace serializers {

orderlist_t
read_uint8_orderlist(binaryparse::context &ctx, size_t length) {
    using namespace binaryparse;
    orderlist_t ret;
    auto rawlist = read_bytestring(ctx, length);
    for (size_t i = 0; i < length; ++i) {
        uint8_t item = rawlist[i];
        switch (item) {
        case 0xff:
            orderlist_insert(ret, i, orderlist_nil_entry);
            break;
        case 0xfe:
            orderlist_insert(ret, i, orderlist_spacer_entry);
            break;
        default:
            orderlist_insert(ret, i, mkentry(item));
        };
    }
    return ret;
}

std::function<CPattern &(const size_t)>
mk_alloc_pattern(module_renderer &ref) {
    auto song = &ref;
    size_t ctr = 0;
    return [=] (const size_t num_rows) mutable -> CPattern & {
        const auto idx = ctr;
        song->Patterns.Insert(idx, num_rows);
        ++ctr;
        return song->Patterns[idx];
    };
}

std::function<modsample_t &(void)>
mk_alloc_sample(module_renderer &ref) {
    auto song = &ref;
    size_t ctr = 1;
    return [=] () mutable -> modsample_t & {
        const auto idx = ctr;
        ++ctr;
        return song->Samples[idx];
    };
}

std::function<modinstrument_t &(void)>
mk_alloc_instr(module_renderer &ref) {
    auto song = &ref;
    size_t ctr = 1;
    return [=] () mutable -> modinstrument_t & {
        song->Instruments[ctr] = new modinstrument_t;
        auto &ret = *song->Instruments[ctr];
        ret = song->m_defaultInstrument;
        song->SetDefaultInstrumentValues(&ret);
        ++ctr;
        return ret;
    };
}

}
}
