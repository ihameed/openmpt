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

#include <string>
#include <vector>
#include <cstdio>

#include "../pervasives/pervasives.h"
#include "./constants.h"
#include "./constants.h"

namespace modplug {
namespace mixgraph {

class arrow;


class vertex {
public:
    vertex() : id(ID_INVALID) { };
    vertex(id_t, std::string);
    virtual ~vertex();

    const id_t id;

    sample_t channels[MAX_NODE_CHANNELS][modplug::mixgraph::MIX_BUFFER_SIZE + 2];

    int ghetto_channels[modplug::mixgraph::MIX_BUFFER_SIZE * 2 + 2];
    long ghetto_vol_decay_l;
    long ghetto_vol_decay_r;

    std::vector<arrow *> _input_arrows;
    std::vector<arrow *> _output_arrows;

    size_t _input_channels;
    size_t _output_channels;

    std::string name;
};

class arrow {
public:
    arrow() : id(ID_INVALID), head(nullptr), tail(nullptr), head_channel(0), tail_channel(0) { }
    arrow(id_t, vertex *, size_t, vertex *, size_t);

    vertex *const head;
    const size_t head_channel;

    vertex *const tail;
    const size_t tail_channel;

    const id_t id;

    size_t _head_idx;
    size_t _tail_idx;
};


}
}
