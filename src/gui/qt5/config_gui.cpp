#include "stdafx.h"

#include "../../legacy_soundlib/sndfile.h"
#include "../../pervasives/pervasives.h"

#include "app_config.h"
#include "config_dialog.h"
#include "config_gui.h"
#include "pattern_editor.h"

namespace modplug {
namespace gui {
namespace qt5 {


using namespace modplug::pervasives;

config_gui_main::config_gui_main(app_config &context) :
    context(context),
    preset_it("Impulse Tracker"),
    preset_xm("FastTracker 2"),
    preset_mpt("Classic Modplug"),
    preset_buzz("Buzz")
{
    demo_dummy = std::unique_ptr<module_renderer>(new module_renderer());
    demo_dummy->m_nChannels = 3;
    demo_dummy->InitChannel(1);
    demo_dummy->InitChannel(2);
    demo_dummy->InitChannel(3);
    demo_dummy->Patterns.Insert(64);

    demo = new pattern_editor(
        *demo_dummy.get(),
        emptymap,
        emptymap,
        emptymap,
        context.colors()
    );
    demo->setMinimumHeight(100);
    demo->setMinimumWidth(100);

    auto box = new QComboBox;
    for_each(color_names, [&] (color_name_assoc_t color) {
        box->addItem(color.value, QVariant(color.key));
    });

    auto wire = [&] (QPushButton &b) { connect(
        &b, &QPushButton::clicked,
        this, &config_gui_main::preset_clicked
    ); };

    wire(preset_it);
    wire(preset_xm);
    wire(preset_mpt);
    wire(preset_buzz);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(box);
    layout->addWidget(demo);
    layout->addWidget(&preset_it);
    layout->addWidget(&preset_xm);
    layout->addWidget(&preset_mpt);
    layout->addWidget(&preset_buzz);

    refresh();
};

void config_gui_main::set_colors(const colors_t &newcolors) {
    colors = newcolors;
    demo->update_colors(newcolors);
}

void config_gui_main::preset_clicked() {
    auto derp = sender();
    if (derp == &preset_it)        set_colors(generate_preset(it_colors));
    else if (derp == &preset_xm)   set_colors(generate_preset(ft2_colors));
    else if (derp == &preset_mpt)  set_colors(generate_preset(old_mpt_colors));
    else if (derp == &preset_buzz) set_colors(generate_preset(buzz_colors));
}

void config_gui_main::refresh() {
    colors = context.colors();
}

void config_gui_main::apply_changes() {
    context.change_colors(colors);
}

}
}
}
