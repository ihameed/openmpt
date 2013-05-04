#include "stdafx.h"

#include "graph_editor.h"

#include "../pervasives/pervasives.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <new>
#include <set>
#include <utility>
#include <vector>

#include "../mixgraph/core.h"

#include <cstdlib>
#include <cstdint>

#include <cassert>

using namespace modplug::pervasives;
using modplug::mixgraph::id_t;

namespace modplug {
namespace gui {
namespace qt5 {

void arrow_guistate_t::init(vertex_guistate_t *head, vertex_guistate_t *tail, modplug::mixgraph::arrow *arrow) {
    this->head = head;
    this->tail = tail;
    this->arrow = arrow;
    curve_updated = false;

    update_headpoint(head->out_nub_location(arrow->head_channel));
    update_tailpoint(tail->in_nub_location(arrow->tail_channel));
}

graph_editor::graph_editor(modplug::mixgraph::core *graph) : graph(graph)
{
    assert(graph != nullptr);
    lmb_down_ = false;
    hompy_selected = false;

    layout.init();

    selection_state.active = false;

    font_manager.init("Nimbus Sans Novus D Light");
    //font_manager.init("Tahoma");

    //ftype_ = agg::glyph_ren_agg_gray8;


    point_t old_origin = {10, 10};
    auto &vertices = graph->vertices;

    for (auto iter = vertices.begin(); iter != vertices.end(); ++iter) {
        auto item = *iter;
        auto key = item.first;
        vertex_guistate_t *guivertex = new vertex_guistate_t;
        vertex_guistate.insert(std::make_pair(key, guivertex));
        guivertex->init(&layout, item.second);
        guivertex->move(old_origin);

        old_origin.y = guivertex->pos.y + guivertex->pos.height + 20;
    }


    ///*
    auto &arrows = graph->arrows;
    for (auto iter = arrows.begin(); iter != arrows.end(); ++iter) {
        auto item = *iter;
        auto key = item.first;
        auto arrow = item.second;

        auto guiarrow = new arrow_guistate_t;
        arrow_guistate.insert(std::make_pair(item.first, guiarrow));

        auto &head = vertex_guistate[arrow->head->id];
        auto &tail = vertex_guistate[arrow->tail->id];
        guiarrow->init(head, tail, arrow);

        guiarrow->head->out_arrows.push_back(guiarrow);
        guiarrow->tail->in_arrows.push_back(guiarrow);
    }
    //*/
}


void graph_editor::begin_region_selection(point_t mouse_pos) {
    selected_ids.clear();
    selection_state.active = true;
    selection_state.area.left = mouse_pos.x;
    selection_state.area.top  = mouse_pos.y;

    selection_state.area.bottom = selection_state.area.top;
    selection_state.area.right  = selection_state.area.left;
}

void graph_editor::update_region_selection(point_t mouse_pos) {
    if (selection_state.active) {
        selection_state.area.right  = mouse_pos.x;
        selection_state.area.bottom = mouse_pos.y;
        invalidate();
    }
}

void graph_editor::end_region_selection() {
    if (selection_state.active) {
        normalize_rect(selection_state.area);
        calculate_selection();
    }
    selection_state.active = false;
}

void graph_editor::calculate_selection() {
    auto &selection_area = selection_state.area;

    auto select_if_vertex_intersects = [&] (vertex_mapitem_t item) {
        auto id     = item.first;
        auto vertex = item.second;

        rect_t vertex_box = rect_of_relrect(vertex->pos);

        if (rect_intersects_rect(vertex_box, selection_area)) {
            selected_ids.insert(id);
        }
    };

    auto select_if_arrow_intersects = [&] (arrow_mapitem_t item) {
        auto id    = item.first;
        auto arrow = item.second;
        auto &path = arrow->path();

        path.rewind(0);
        unsigned path_command;
        pointd_t last_point;
        pointd_t current_point;
        do {
            path_command = path.vertex(&current_point.x, &current_point.y);
            if (path_command == agg::path_cmd_line_to &&
                rect_intersects_ideal_line(selection_area, last_point, current_point) )
            {
                rect_t projection = rect_of_points(last_point, current_point);
                normalize_rect(projection);
                if (rect_intersects_rect(projection, selection_area)) {
                    selected_ids.insert(id);
                    break;
                }
            }
            last_point = current_point;
        } while (path_command != agg::path_cmd_stop);
    };

    for_each_rev(vertex_guistate, select_if_vertex_intersects);
    for_each_rev(arrow_guistate,  select_if_arrow_intersects);
}

void graph_editor::paint_agg(agg::rendering_buffer &rbuf, rect_t &clipping_rect, rectsize_t &client_size) {
    //ghettotimer timer(__FUNCTION__);
    pixel_format_t pixf(rbuf);
    base_render_t  renb(pixf);


    antialiased_renderer_t antialiased(renb);
    primitive_renderer_t   primitive(renb);

    antialiased_rasterizer_t antialiased_rasterizer;
    primitive_rasterizer_t   primitive_rasterizer(primitive);
    agg::scanline_p8 sl;

    antialiased_rasterizer.clip_box(
        clipping_rect.left,
        clipping_rect.top,
        clipping_rect.right,
        clipping_rect.bottom
    );

    renb.clear(layout.background);

    draw_arrows(antialiased_rasterizer, sl, antialiased);
    draw_vertices(antialiased_rasterizer, sl, antialiased);

    draw_selection_box(primitive_rasterizer, sl, primitive);

    //draw_clipping_debug_box(primitive_rasterizer, sl, primitive, clipping_rect);
}

void graph_editor::invalidate() {
    //this->repaint();
    this->update();
}

void graph_editor::lmb_down(point_t mouse_pos) {
    this->lmb_down_ = true;
    this->hompy_selected = false;
    if (graph) {
        for (auto &iter : vertex_guistate) {
            auto id = iter.first;
            auto vtx = iter.second;
            int x = vtx->pos.x;
            int y = vtx->pos.y;
            int cx = vtx->pos.x + vtx->pos.width;
            int cy = vtx->pos.y + vtx->pos.height;
            //DEBUG_FUNC("mouse (%d, %d) testing against (%d, %d), (%d, %d)", mouse_pos.x, mouse_pos.y, x, y, cx, cy);

            if ((mouse_pos.x >= x && mouse_pos.x < cx) &&
                (mouse_pos.y >= y && mouse_pos.y < cy))
            {
                this->hompy_selected = true;
                this->selected_id = id;
                this->mouse_offset.x = x - mouse_pos.x;
                this->mouse_offset.y = y - mouse_pos.y;
                invalidate();
                break;
            }
        }
    }

    if (!this->hompy_selected) {
        this->begin_region_selection(mouse_pos);
    }
}

void graph_editor::lmb_up() {
    this->end_region_selection();
    this->lmb_down_ = false;
    this->hompy_selected = false;
    invalidate();
}

void graph_editor::mouse_moved(point_t mouse_pos) {
    this->update_region_selection(mouse_pos);
    if (this->hompy_selected) {
        auto &selected_vertex = vertex_guistate[selected_id];
        point_t newpos = {mouse_pos.x + mouse_offset.x, mouse_pos.y + mouse_offset.y};
        selected_vertex->move(newpos);
        invalidate();
    }
}

void graph_editor::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        point_t point = {event->x(), event->y()};
        lmb_down(point);
    }
}

void graph_editor::mouseMoveEvent(QMouseEvent *event) {
    point_t point = {event->x(), event->y()};
    mouse_moved(point);
}

void graph_editor::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        lmb_up();
    }
}

void graph_editor::paintEvent(QPaintEvent *event) {
    ghettotimer timer(__FUNCTION__);
    QPainter painter(this);
    const auto &rect = event->rect();
    rect_t update_rect = {
        rect.x(),
        rect.y(),
        rect.x() + rect.width(),
        rect.y() + rect.bottom()
    };
    rect_t client_rect = update_rect;

    //auto update_size = rect_size(update_rect);
    auto update_size = rect_size(client_rect);
    auto client_size = rect_size(client_rect);

    QImage image(update_size.width, update_size.height, QImage::Format_RGB32);
    agg::rendering_buffer buffer(
        image.bits(),
        image.width(),
        image.height(),
        image.width() * 4
    );
    paint_agg(buffer, update_rect, client_size);

    painter.drawImage(0, 0, image);
}



}
}
}
