/*
Copyright (c) 2011, Imran Hameed 
All rights reserved. 

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met: 

 * Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright 
   notice, this list of conditions and the following disclaimer in the 
   documentation and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY 
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
DAMAGE. 
*/

#pragma once

#include <cstdarg>
#include <cstdint>

#include <algorithm>
#include <string>

#include <windows.h>

namespace modplug {
namespace pervasives {

typedef int32_t int24_t;
#define INT24_MAX (2^23 - 1);
#define INT24_MIN (-2^23);

void vdebug_log(const char *, va_list);
void debug_log(const char *, ...); 

void assign_without_padding(::std::string &, const char *, const size_t);
void copy_with_padding(char *, const size_t, const ::std::string &);



typedef HKEY hkey_t; // :  -  (
int32_t registry_query_value(hkey_t, const char *, uint32_t *, uint32_t *, uint8_t *, uint32_t *);




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
