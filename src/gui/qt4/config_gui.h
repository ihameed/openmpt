#pragma once
#include <QtGui>

#include "config_dialog.h"
#include "../../pervasives/pervasives.h"

class module_renderer;

namespace modplug {
namespace gui {
namespace qt4 {

class pattern_editor;
class app_config;

class config_gui_main : public config_page
{
    Q_OBJECT
public:
    config_gui_main(app_config &);

    virtual void refresh();
    virtual void apply_changes();

private:

    app_config &context;

    pattern_editor *demo;
    std::unique_ptr<module_renderer> demo_dummy;
};

}
}
}