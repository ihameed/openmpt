#include "stdafx.h"

#include "pervasives.h"

#include <algorithm>
#include <functional>
#include <string>
#include <locale>
#include <sstream>

#include <windows.h>
#include <strsafe.h>

namespace modplug {
namespace pervasives {


int32_t registry_query_value(hkey_t key, const char *value_name, uint32_t *reserved, uint32_t *type, uint8_t *data, uint32_t *data_size) {
    return RegQueryValueEx(key, value_name, reinterpret_cast<DWORD *>(reserved), reinterpret_cast<DWORD *>(type), data, reinterpret_cast<DWORD *>(data_size));
}

std::string &rtrim_whitespace(std::string &str) {
    str.erase(std::find_if(str.rbegin(), str.rend(),
        [] (int c) { return !(isspace(c) || c == 0); }).base()
    );
    return str;
}

void assign_without_padding(std::string &dst, const char *src, const size_t len) {
    rtrim_whitespace(dst.assign(src, len));
}

void copy_with_padding(char *dst, const size_t len, const std::string &src) {
    size_t sz = src.copy(dst, len);
    for (; sz < len; ++sz) {
        dst[sz] = 0;
    }
}

}
}
