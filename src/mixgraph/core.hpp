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

#include <array>
#include <vector>
#include <map>
#include <utility>

#include "constants.hpp"
#include "vertex.hpp"

namespace modplug {
namespace mixgraph {


typedef std::pair<id_t, vertex *> vertex_item_t;
typedef std::map<id_t, vertex *> vertex_map_t;

typedef std::pair<id_t, arrow *> arrow_item_t;
typedef std::map<id_t, arrow *> arrow_map_t;

class core {
public:
    core();
    ~core();

    void pre_process(size_t);
    void process(int *, size_t, const sample_t, const sample_t);

    id_t add_channel();
    id_t add_vst();

    id_t load_vertex();

    bool remove_vertex(id_t);
    void rename_vertex();


    vertex *vertex_with_id(id_t vertex);

    id_t link_vertices(id_t, size_t, id_t, size_t);
    bool unlink_vertices(id_t);

    id_t new_id();
    id_t _largest_id;
    
    std::array<vertex *, MAX_PHYSICAL_CHANNELS> channel_vertices;
    vertex *channel_bypass;
    vertex *master_sink;

    vertex_map_t vertices;
    arrow_map_t arrows;
};


}
}
