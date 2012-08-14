#pragma once

#define DEBUG_FUNC(fmt, ...) do { \
    modplug::pervasives::debug_log(\
        "%s(): " fmt, __FUNCTION__, __VA_ARGS__); \
} while(0)

#define DEBUG_FILE(fmt, ...) do { \
    modplug::pervasives::debug_log(\
        "%s:%s %s(): " fmt, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); \
} while(0)

namespace Json { class Value; }

namespace modplug {
namespace pervasives {

void vdebug_log(const char *, va_list);
void debug_log(const char *, ...);
std::string debug_json_dump(Json::Value &);

}
}