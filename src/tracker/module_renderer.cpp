#include "stdafx.h"
#include "../legacy_soundlib/Sndfile.h"
#include "../pervasives/pervasives.h"
#include "../audioio/paudio.h"

using namespace modplug::audioio;

void module_renderer::change_audio_settings(const paudio_settings &settings) {
    DEBUG_FUNC("thread id = %x", GetCurrentThreadId());
    bool reset = !settings_equal(render_settings, settings);
    render_settings = settings;

    InitPlayer(reset);
}