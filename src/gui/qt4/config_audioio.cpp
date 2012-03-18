#include "stdafx.h"

#include "../../pervasives/pervasives.h"
#include "config_dialog.h"
#include "config_audioio.h"
#include "../../audioio/paudio.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt4 {


config_audioio_main::config_audioio_main(config_context &context, QWidget *parent) : QWidget(parent) {
    auto hoot = new QComboBox(this);
    auto &pa_system = context.pa_system;
    for (auto i = pa_system.devicesBegin(); i != pa_system.devicesEnd(); ++i) {
        debug_log("hostapi '%s' device '%s'", i->hostApi().name(), i->name());
        std::string huo("hostapi ");
        huo += i->hostApi().name() + std::string(" device ") + i->name();
        hoot->addItem(huo.c_str());
    }
};


}
}
}