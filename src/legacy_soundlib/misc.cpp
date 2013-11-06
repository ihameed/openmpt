#include "stdafx.h"

#include <cstdlib>

#include "misc.h"

#include "../pervasives/pervasives.h"

using namespace modplug::pervasives;

namespace modplug {
namespace legacy_soundlib {

char *
alloc_sample(size_t size) {
    if (size > 0xFFFFFFD6) return nullptr;
    char * p = static_cast<char *>(malloc((size + 39) & ~7));
    if (p) p += 16;
    return p;
}

void
free_sample(void * p) {
    if (p) free(static_cast<char *>(p) - 16);
}

}
}
