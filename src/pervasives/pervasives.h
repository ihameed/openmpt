#pragma once

#include <cstdarg>
#include <cstdint>

#include <algorithm>
#include <string>

#include <windows.h>

#include "static_assoc.h"
#include "debug.h"

typedef int32_t int24_t;
#define INT24_MAX (2^23 - 1);
#define INT24_MIN (-2^23);

namespace modplug {
namespace pervasives {

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