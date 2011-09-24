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

#include <algorithm>
#include <utility>

#include "core.h"
#include "../mptrack.h"
#include "../pervasives/pervasives.h"
#include "../graph/source_vertex.h"

#include <cstdio>

using namespace modplug::pervasives;

namespace modplug {
namespace graph {



size_t find_idx(const std::vector<arrow *> ar, const arrow *val) {
    size_t i = 0, len = ar.capacity();
    for (; i < len && ar[i] != val; ++i);
    return i;
}

bool _vtx_add_link(arrow *link) {
    if (!link) return false;
    auto &output_arrows = link->head->_output_arrows;
    auto &input_arrows  = link->tail->_input_arrows;
    size_t head_endpoint = find_idx(output_arrows, nullptr);
    size_t tail_endpoint = find_idx(input_arrows,  nullptr);
    if (head_endpoint != MAX_CHANNEL_ENDPOINTS && tail_endpoint != MAX_CHANNEL_ENDPOINTS) {
        output_arrows[head_endpoint] = link;
        output_arrows[tail_endpoint] = link;
        return true;
    }
    return false;
}

bool _vtx_remove_link(arrow *link) {
    if (!link) return false;
    auto &output_arrows = link->head->_output_arrows;
    auto &input_arrows  = link->tail->_input_arrows;
    size_t head_endpoint = find_idx(output_arrows, link);
    size_t tail_endpoint = find_idx(input_arrows,  link);
    if (head_endpoint != MAX_CHANNEL_ENDPOINTS && tail_endpoint != MAX_CHANNEL_ENDPOINTS) {
        output_arrows[head_endpoint] = nullptr;
        output_arrows[tail_endpoint] = nullptr;
        return true;
    }
    return false;
}

template <class Has_Id>
Has_Id *id_map_get(std::map<id_t, Has_Id> &map, const id_t key) {
    auto ret = map.find(key);
    return (ret != map.end()) ? (&ret->second) : nullptr;
}

template <class Has_Id>
Has_Id *id_map_put(std::map<id_t, Has_Id> &map, const id_t key, const Has_Id &value) {
    auto ret = map.insert(std::pair<id_t, Has_Id>(key, value));
    return &((*ret.first).second);
}

template <class Has_Id>
bool id_map_del(std::map<id_t, Has_Id> &map, const id_t key) {
    return map.erase(key) == 1;
}




core::core() : _largest_id(1) {
    debug_log("created core\n");
    for (size_t idx = 0; idx < modplug::graph::MAX_CHANNELS; ++idx) {
        char my_nuts[256];
        sprintf(my_nuts, "channel %d", idx);

        modplug::graph::vertex *jenkmaster = new source_vertex(new_id(), std::string(my_nuts));
        id_map_put(_vertices, jenkmaster->id, jenkmaster);
        channel_vertices[idx] = jenkmaster;
    }
    _master_sink = new modplug::graph::vertex(1, std::string("audio out"));
    _master_sink->_input_channels = 2;
}

core::~core() {
    //the power of sepples
    std::for_each(_vertices.begin(), _vertices.end(), [](std::pair<id_t, modplug::graph::vertex *> item) { delete item.second; });
    for (size_t idx = 0; idx < modplug::graph::MAX_CHANNELS; ++idx) {
        channel_vertices[idx] = nullptr;
    }
    delete _master_sink;

    debug_log("destroyed core\n");
}

id_t core::new_id() {
    _largest_id += 1;
    return _largest_id;
}

vertex *core::vertex(id_t vertex_id) {
    //XXXih:   uuurrr dur rururu
    return *id_map_get(_vertices, vertex_id);
}

id_t core::link_vertices(id_t head_id, size_t head_channel, id_t tail_id, size_t tail_channel) {
    auto *head = vertex(head_id);
    auto *tail = vertex(tail_id);
    if (!head || !tail) return ID_INVALID;
    id_t id = new_id();
    arrow *link = id_map_put(_arrows, id, arrow(id, head, head_channel, tail, tail_channel));
    if (!_vtx_add_link(link)) {
        id_map_del(_arrows, id);
        return ID_INVALID;
    }
    return link->id;
}

bool core::unlink_vertices(id_t arrow_id) {
    return _vtx_remove_link(id_map_get(_arrows, arrow_id));
}

id_t add_channel() {
    return ID_INVALID;
}

id_t add_vst() {
    return ID_INVALID;
}

void _remove_links(std::vector<arrow *> &arrows) {
    size_t len = arrows.capacity();
    for (size_t i = 0; i < len; ++i) {
        if (arrows[i]) _vtx_remove_link(arrows[i]);
    }
}

bool core::remove_vertex(id_t vertex_id) {
    auto *node = vertex(vertex_id);
    _remove_links(node->_input_arrows);
    _remove_links(node->_output_arrows);
    return id_map_del(_vertices, vertex_id);
}

void core::process(int *samples, const size_t num_samples, const size_t sample_width) {
}

void _process(int *samples, const size_t num_samples, const size_t sample_width) {
}


}
}
