#include "stdafx.h"

#include "../../legacy_soundlib/sndfile.h"
#include "../../pervasives/pervasives.h"

#include "app_config.h"
#include "config_dialog.h"
#include "config_gui.h"
#include "pattern_editor.h"

namespace modplug {
namespace gui {
namespace qt4 {


using namespace modplug::pervasives;

config_gui_main::config_gui_main(app_config &context) : context(context)
{
    demo_dummy = std::unique_ptr<module_renderer>(new module_renderer());
    demo_dummy->m_nChannels = 3;
    demo_dummy->InitChannel(1);
    demo_dummy->InitChannel(2);
    demo_dummy->InitChannel(3);
    demo_dummy->Patterns.Insert(64);

    demo = new pattern_editor(*demo_dummy.get());
    demo->setMinimumHeight(100);
    demo->setMinimumWidth(100);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(demo);
    layout->addWidget(new QTextEdit);

    refresh();
};

void config_gui_main::refresh() {
}

void config_gui_main::apply_changes() {
}

}
}
}