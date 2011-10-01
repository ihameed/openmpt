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
#include "../mixer/mixutil.h"
#include "./source_vertex.h"

#include <cstdio>

using namespace modplug::pervasives;

namespace modplug {
namespace mixgraph {



template <typename elem_t>
bool __has_room(std::vector<elem_t> *vec) {
    return vec->size() <= vec->capacity();
}

arrow *__swap_and_remove(std::vector<arrow *> *vec, size_t idx) {
    size_t sz = vec->size();
    if (!sz) return nullptr;
    (*vec)[idx] = (*vec)[sz - 1];
    vec->pop_back();
    return (*vec)[idx];
}

bool _vtx_add_link(arrow *link) {
    if (!link) return false;
    auto output_arrows = &link->head->_output_arrows;
    auto input_arrows  = &link->tail->_input_arrows;
    if (__has_room(output_arrows) && __has_room(input_arrows)) {
        output_arrows->push_back(link);
        input_arrows->push_back(link);
        link->_head_idx = output_arrows->size() - 1;
        link->_tail_idx = input_arrows->size() - 1;
        return true;
    }
    return false;
}

bool _vtx_remove_link(arrow *link) {
    if (!link) return false;
    auto output_arrows = &link->head->_output_arrows;
    auto input_arrows  = &link->tail->_input_arrows;
    auto swapped_head  = __swap_and_remove(output_arrows, link->_head_idx);
    auto swapped_tail  = __swap_and_remove(input_arrows, link->_tail_idx);
    swapped_head->_head_idx = link->_head_idx;
    swapped_tail->_tail_idx = link->_tail_idx;
    return true;
}

template <typename Has_Id>
Has_Id *id_map_get(std::map<id_t, Has_Id> &map, const id_t key) {
    auto ret = map.find(key);
    return (ret != map.end()) ? (&ret->second) : nullptr;
}

template <typename Has_Id>
Has_Id *id_map_put(std::map<id_t, Has_Id> &map, const id_t key, const Has_Id &value) {
    auto ret = map.insert(std::pair<id_t, Has_Id>(key, value));
    return &((*ret.first).second);
}

template <typename Has_Id>
bool id_map_del(std::map<id_t, Has_Id> &map, const id_t key) {
    return map.erase(key) == 1;
}




core::core() : _largest_id(1) {
    debug_log("created core");

    _master_sink = new modplug::mixgraph::vertex(1, std::string("audio out"));
    _master_sink->_input_channels = 2;
    id_map_put(_vertices, 1, _master_sink);

    for (size_t idx = 0; idx < modplug::mixgraph::MAX_CHANNELS; ++idx) {
        char my_nuts[256];
        sprintf(my_nuts, "channel %d", idx);

        modplug::mixgraph::vertex *jenkmaster = new source_vertex(new_id(), std::string(my_nuts));
        id_map_put(_vertices, jenkmaster->id, jenkmaster);
        channel_vertices[idx] = jenkmaster;

        link_vertices(jenkmaster->id, 0, _master_sink->id, 0);
        link_vertices(jenkmaster->id, 1, _master_sink->id, 1);
    }
    //link_vertices(id_t(2), 0, id_t(1), 0);
    //link_vertices(id_t(2), 1, id_t(1), 1);
}

core::~core() {
    //the power of sepples
    std::for_each(_vertices.begin(), _vertices.end(), [](std::pair<id_t, modplug::mixgraph::vertex *> item) { delete item.second; });
    for (size_t idx = 0; idx < modplug::mixgraph::MAX_CHANNELS; ++idx) {
        channel_vertices[idx] = nullptr;
    }

    debug_log("destroyed core");
}

id_t core::new_id() {
    _largest_id += 1;
    return _largest_id;
}

vertex *core::vertex(id_t vertex_id) {
    //XXXih:   uuurrr dur rururu
    auto ret = id_map_get(_vertices,vertex_id);
    return (ret) ? (*ret) : (nullptr);
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

    //debug_log("added link (id %d): '%s' (id %d, chn %d) -> '%s' (id %d, chn %d)", link->id, head->name.c_str(), head->id, link->head_channel, tail->name.c_str(), tail->id, link->tail_channel);
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
    size_t len = arrows.size();
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



inline void __mix_buffer_in_place(sample_t *mixtarget, const sample_t *source, const size_t num_samples) {
    for (size_t idx = 0; idx < num_samples; ++idx) {
        mixtarget[idx] += source[idx];
    }
}

void __process(vertex *tail, const size_t num_samples) {
    for (size_t idx = 0, maxsz = tail->_input_arrows.size(); idx < maxsz; ++idx) {
        arrow *link = tail->_input_arrows[idx];
        vertex *head = link->head;
        __process(head, num_samples);

        sample_t *tail_channel = tail->channels[link->tail_channel];
        sample_t *head_channel = head->channels[link->head_channel];
        __mix_buffer_in_place(tail_channel, head_channel, num_samples);
    }
}



void core::process(int *destbuf, const size_t num_samples, const sample_t float_to_int_scale, const sample_t int_to_float_scale) {
    memset(_master_sink->channels[0], 0, sizeof(sample_t) * num_samples);
    memset(_master_sink->channels[1], 0, sizeof(sample_t) * num_samples);

    sample_t *left  = _master_sink->channels[0];
    sample_t *right = _master_sink->channels[1];

    //debug_log("pre process master vs child (left %f, right %f), (left %f, right %f)", left[0], right[0], channel_vertices[0]->channels[0][0], channel_vertices[0]->channels[0][1]);
    __process(_master_sink, num_samples);
    //debug_log("process master vs child (left %f, right %f), (left %f, right %f)", left[0], right[0], channel_vertices[0]->channels[0][0], channel_vertices[0]->channels[0][1]);

    modplug::mixer::float_to_stereo_mix(right, left, destbuf, num_samples, float_to_int_scale);
    //modplug::mixer::float_to_stereo_mix(channel_vertices[0]->channels[0], channel_vertices[0]->channels[1], samples, num_samples, float_to_int_scale);
}

void core::pre_process(const size_t num_samples) {
    for (size_t idx = 0; idx < modplug::mixgraph::MAX_CHANNELS; ++idx) {
        modplug::mixgraph::vertex *channel = channel_vertices[idx];
        modplug::mixer::stereo_fill(channel->ghetto_channels, num_samples, &channel->ghetto_vol_decay_l, &channel->ghetto_vol_decay_r);
        memset(channel->channels[0], 0, sizeof(sample_t) * num_samples);
        memset(channel->channels[1], 0, sizeof(sample_t) * num_samples);
    }
}


}
}
