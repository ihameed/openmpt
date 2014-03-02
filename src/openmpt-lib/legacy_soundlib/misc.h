#pragma once

#include <cstdint>
#include <tuple>

namespace modplug {
namespace legacy_soundlib {

char * alloc_sample(size_t);
void free_sample(void *);

}
}
