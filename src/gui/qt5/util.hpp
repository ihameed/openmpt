#pragma once

#include "pattern_editor.hpp"
#include "../../Resource.h"

#include "../../pervasives/pervasives.hpp"

namespace modplug {
namespace gui {
namespace qt5 {

inline int rect_bottom(const QRect &rect) {
    return rect.top() + rect.height();
}

inline int rect_right(const QRect &rect) {
    return rect.left() + rect.width();
}

}
}
}
