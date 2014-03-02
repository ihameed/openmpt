#include "../openmpt-lib/stdafx.h"
#include "../openmpt-lib/main.h"

#include <QtWidgets>

struct reference_static_resources {
    reference_static_resources() { Q_INIT_RESOURCE(resources); }
};

reference_static_resources _;

CTrackApp theApp;
