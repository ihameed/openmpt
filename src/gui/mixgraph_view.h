#pragma once

#include <windows.h>
#include "../mixgraph/core.h"

namespace modplug {
namespace gui {

struct mixgraph_viewstate_t;

typedef HWND hwnd_t;


void mixgraph_view_register();
hwnd_t mixgraph_view_create(mixgraph_viewstate_t *, hwnd_t parent);


mixgraph_viewstate_t *mixgraph_view_create_state(modplug::mixgraph::core *);
void mixgraph_view_delete_state(mixgraph_viewstate_t *);


void test_view_register();
void show_my_weldus(modplug::mixgraph::core *);




}
}
