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

#include "stdafx.h"

#include "pervasives.h"

#include <algorithm>
#include <cstdarg>

#include <Windows.h>
#include <strsafe.h>

namespace modplug {
namespace pervasives {


void debug_log(const char *fmt, ...) {
#ifdef _DEBUG
    static const size_t maxlen = 2048;
    static const size_t buflen = maxlen + 1;
    char buf[buflen];
    char *sprintf_end = nullptr;

    va_list arglist;
    va_start(arglist, fmt);

    std::fill_n(buf, buflen, 0);

    HRESULT ret = StringCchVPrintfEx(buf, maxlen, &sprintf_end, nullptr, STRSAFE_IGNORE_NULLS, fmt, arglist);
    if (SUCCEEDED(ret)) {
        sprintf_end[0] = '\n';
        sprintf_end[1] = 0;
        OutputDebugString(buf);
    } else {
        OutputDebugString("modplug::pervasives::debug_log(): failure in StringCchVPrintfEx!\n");
    }

    va_end(arglist);
#endif
}


}
}