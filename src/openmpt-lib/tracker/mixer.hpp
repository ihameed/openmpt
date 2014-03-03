#pragma once

#include "../mixgraph/constants.hpp"

namespace modplug {
namespace tracker {

struct voice_ty;

size_t
mix_and_advance(modplug::mixgraph::sample_t *, modplug::mixgraph::sample_t *, size_t, voice_ty *);

}
}
