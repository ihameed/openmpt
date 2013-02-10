#pragma once

#include "pattern_editor.h"
#include "../../Resource.h"

#include "../../pervasives/pervasives.h"

namespace modplug {
namespace gui {
namespace qt5 {

inline int rect_bottom(const QRect &rect) {
    return rect.top() + rect.height();
}

inline int rect_right(const QRect &rect) {
    return rect.left() + rect.width();
}

QImage load_bmp_resource(LPTSTR, HINSTANCE, HDC);

}
}
}