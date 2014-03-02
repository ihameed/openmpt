#pragma once

#include <cstdint>
#include "../pervasives/pervasives.hpp"

namespace modplug {
namespace tracker {

typedef int32_t sampleoffset_t;
/*
NEWTYPE(sampleoffset_t, int32_t);
ORDERED_TYPE(sampleoffset_t);
ARITHMETIC_TYPE(sampleoffset_t);
INCR_DECR_TYPE(sampleoffset_t);
*/

typedef int32_t fixedpt_t;

typedef uint32_t rowindex_t;
const rowindex_t RowIndexMax     = UINT32_MAX;
const rowindex_t RowIndexInvalid = RowIndexMax;

typedef uint16_t chnindex_t;
const chnindex_t ChannelIndexMax     = UINT16_MAX;
const chnindex_t ChannelIndexInvalid = ChannelIndexMax;

/*
NEWTYPE(orderindex_t, uint16_t);
ORDERED_TYPE(orderindex_t);
ARITHMETIC_TYPE(orderindex_t);
INCR_DECR_TYPE(orderindex_t);
*/
typedef uint16_t orderindex_t;
const orderindex_t OrderIndexMax     = orderindex_t(UINT16_MAX);
const orderindex_t OrderIndexInvalid = orderindex_t(OrderIndexMax);

typedef uint16_t patternindex_t;
const patternindex_t PatternIndexMax     = UINT16_MAX;
const patternindex_t PatternIndexInvalid = PatternIndexMax;

typedef uint16_t sampleindex_t;
const sampleindex_t SampleIndexMax     = UINT16_MAX;
const sampleindex_t SampleIndexInvalid = SampleIndexMax;

typedef uint16_t instrumentindex_t;
const instrumentindex_t InstrumentIndexMax     = UINT16_MAX;
const instrumentindex_t InstrumentIndexInvalid = InstrumentIndexMax;

typedef uint8_t sequenceindex_t;
const sequenceindex_t SequenceIndexMax     = UINT8_MAX;
const sequenceindex_t SequenceIndexInvalid = SequenceIndexMax;

}

namespace pervasives {
modplug::tracker::sampleoffset_t inline
unwrap(const modplug::tracker::sampleoffset_t &wrapped) { return wrapped; }

modplug::tracker::orderindex_t inline
unwrap(const modplug::tracker::orderindex_t &wrapped) { return wrapped; }
}

}
