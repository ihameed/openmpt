#include "stdafx.h"

#include "debug.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <locale>
#include <sstream>

#include <json/json.h>

#include <windows.h>
#include <strsafe.h>

#ifdef _DEBUG
#define MODPLUG_ENABLE_DEBUG_LOG
#else
#define MODPLUG_ENABLE_DEBUG_LOG
#endif

namespace modplug {
namespace pervasives {


void vdebug_log(const char *fmt, va_list arglist) {
#ifdef MODPLUG_ENABLE_DEBUG_LOG
    static const size_t maxlen = 8192;
    static const size_t buflen = maxlen + 1;
    char buf[buflen];
    char *sprintf_end = nullptr;

    std::fill_n(buf, buflen, 0);

    HRESULT ret = StringCchVPrintfEx(buf, maxlen, &sprintf_end, nullptr, STRSAFE_IGNORE_NULLS, fmt, arglist);
    if (SUCCEEDED(ret)) {
        sprintf_end[0] = '\n';
        sprintf_end[1] = 0;
        OutputDebugString(buf);
    } else {
        OutputDebugString("modplug::pervasives::debug_log(): failure in StringCchVPrintfEx!\n");
    }
#endif
}

void debug_log(const char *fmt, ...) {
#ifdef MODPLUG_ENABLE_DEBUG_LOG
    va_list arglist;
    va_start(arglist, fmt);

    vdebug_log(fmt, arglist);

    va_end(arglist);
#endif
}

std::string debug_json_dump(Json::Value &root) {
#ifdef MODPLUG_ENABLE_DEBUG_LOG
    std::ostringstream buf;
    Json::StyledStreamWriter json("  ");
    json.write(buf, root);
    return buf.str();
#else
    return "";
#endif
}


}
}
