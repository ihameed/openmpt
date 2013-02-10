#pragma once

#include <cstdarg>
#include <cstdint>

#include <algorithm>
#include <string>

#include <windows.h>

#include "static_assoc.h"
#include "debug.h"
#include "type.h"

typedef int32_t int24_t;
#define INT24_MAX (2^23 - 1);
#define INT24_MIN (-2^23);

namespace modplug {
namespace pervasives {

class ghettotimer {
public:
    ghettotimer(const char *name) : name(name), start(0), end(0), freq(0) {
        QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
        QueryPerformanceCounter((LARGE_INTEGER *)&start);
    }

    ~ghettotimer() {
        QueryPerformanceCounter((LARGE_INTEGER *)&end);
        double sec = ((double)end - start) / freq;
        double fps = 1 / sec;
        if (freq != 0) {
            debug_log("%s: ghettotimer: took %f ms; fps = %f, freq = %I64d hz",
                name, sec * 1000, fps, freq);
        } else {
            debug_log("%s: ghettotimer: OH MY HEIMER");
        }
    }

    const char *name;
    __int64 start;
    __int64 end;
    __int64 freq;
};

class noncopyable {
protected:
    noncopyable() { };
    ~noncopyable() { };
private:
    noncopyable(const noncopyable &);
    noncopyable & operator = (const noncopyable &);
};

template <typename ty, typename ty1, typename ty2>
ty clamp(ty val, ty1 bad_min, ty2 bad_max) {
    return val < bad_min ? bad_min : (val > bad_max ? bad_max : val);
}

typedef HKEY hkey_t; // :  -  (
int32_t registry_query_value(hkey_t, const char *, uint32_t *,
                             uint32_t *, uint8_t *, uint32_t *);

void assign_without_padding(::std::string &, const char *, const size_t);
void copy_with_padding(char *, const size_t, const ::std::string &);

template <typename array_type, size_t array_size, typename function_type>
function_type for_each(array_type (&container)[array_size], function_type f) {
    std::for_each(container, container + array_size, f);
    return f;
}

template <typename container_type, typename function_type>
function_type for_each(container_type &container, function_type f) {
    std::for_each(container.begin(), container.end(), f);
    return f;
}


template <typename container_type, typename function_type>
function_type for_each_rev(container_type &container, function_type f) {
    std::for_each(container.rbegin(), container.rend(), f);
    return f;
}

}
}
