#include "stdafx.h"
#include "../legacy_soundlib/Sndfile.h"
#include "../pervasives/pervasives.h"
#include "../audioio/paudio.h"

using namespace modplug::audioio;

void module_renderer::change_audio_settings(const paudio_settings_t &settings) {
    auto set_deprecated_globals = [&settings] () {
        module_renderer::deprecated_global_bits_per_sample = 16;
        module_renderer::deprecated_global_channels = 2;
        module_renderer::deprecated_global_mixing_freq = static_cast<uint32_t>(settings.sample_rate);
        module_renderer::deprecated_global_sound_setup_bitmask &= ~(SNDMIX_SOFTPANNING | SNDMIX_REVERSESTEREO);
    };

    DEBUG_FUNC("thread id = %x", GetCurrentThreadId());
    bool reset = !settings_equal(render_settings, settings);
    render_settings = settings;

    set_deprecated_globals();

    InitPlayer(reset);
}
