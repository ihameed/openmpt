#include "stdafx.h"

#include "song_overview.h"

namespace modplug {
namespace gui {
namespace qt5 {

song_overview::song_overview(module_renderer &renderer) : renderer(renderer) {
    auto layout = new QHBoxLayout();
    setLayout(layout);

    layout->addWidget(&samples);
    layout->addWidget(&instruments);
}


}
}
}