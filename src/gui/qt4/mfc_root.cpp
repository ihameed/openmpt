#include "stdafx.h"

#include "../../MainFrm.h"
#include "app_config.h"
#include "mfc_root.h"
#include "../../pervasives/pervasives.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt4 {

mfc_root::mfc_root(app_config &settings, CMainFrame &mfc_parent) :
    QWinWidget(&mfc_parent), settings(settings), mainwnd(mfc_parent)
{
    QObject::connect(
        &settings, SIGNAL(audio_settings_changed(void)),
        this, SLOT(update_audio_settings())
    );
}

void mfc_root::update_audio_settings() {
    DEBUG_FUNC("thread id = %x", GetCurrentThreadId());
    if (mainwnd.m_pSndFile) {
        mainwnd.m_pSndFile->deprecated_SetResamplingMode(mainwnd.m_nSrcMode);
        mainwnd.m_pSndFile->change_audio_settings(settings.audio_settings());
    }
}

}
}
}