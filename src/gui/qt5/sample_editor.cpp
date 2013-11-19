#include "stdafx.h"

#include "sample_editor.hpp"

namespace modplug {
namespace gui {
namespace qt5 {

sample_editor::sample_editor()
    : current_sample(0)
{
    auto vert_section = new QVBoxLayout(this);
    auto top_section = new QHBoxLayout();
    auto bottom_section = new QHBoxLayout();

    top_section->addWidget(&this->current_sample);
    top_section->addWidget(&this->zoom);
    top_section->addWidget(&this->name);
    top_section->addWidget(&this->filename);

    bottom_section->addWidget(&this->default_volume);
    bottom_section->addWidget(&this->global_volume);
    bottom_section->addWidget(&this->default_pan);
    bottom_section->addWidget(&this->c5freq);
    bottom_section->addWidget(&this->transpose);
    bottom_section->addWidget(&this->loop);
    bottom_section->addWidget(&this->susloop);
    bottom_section->addWidget(&this->vibratobox);

    vert_section->addLayout(top_section);
    vert_section->addLayout(bottom_section);

}

void
sample_editor::paintEvent(QPaintEvent *event) {
}

void
sample_editor::resizeEvent(QResizeEvent *event) {
}


sample_loop_editor::sample_loop_editor()
{}

sample_osc_viz::sample_osc_viz()
{}

void
sample_osc_viz::paintEvent(QPaintEvent *event) {
}

}
}
}
