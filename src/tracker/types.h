#pragma once

#include <cstdint>

namespace modplug {
namespace tracker {

typedef uint32_t rowindex_t;
const rowindex_t RowIndexMax     = UINT32_MAX;
const rowindex_t RowIndexInvalid = RowIndexMax;

typedef uint16_t chnindex_t;
const chnindex_t ChannelIndexMax     = UINT16_MAX;
const chnindex_t ChannelIndexInvalid = ChannelIndexMax;

}
}