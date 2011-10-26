#include "stdafx.h"

#include "mixgraph_view.h"
#include "../pervasives/pervasives.h"

#include <algorithm>
#include <new>
#include <set>
#include <utility>
#include <vector>

#include "../mixgraph/core.h"

#include <cstdlib>
#include <cstdint>
#include "widget.h"

#include <agg_rendering_buffer.h>

#include <agg_pixfmt_rgba.h>
#include <agg_pixfmt_rgb.h>

#include <agg_rasterizer_scanline_aa.h>
#include <agg_rasterizer_outline.h>

#include <agg_renderer_scanline.h>
#include <agg_renderer_primitives.h>

#include <agg_scanline_p.h>
#include <agg_curves.h>
#include <agg_conv_stroke.h>

#include <agg_font_win32_tt.h>

#include <cassert>

using namespace modplug::pervasives;
using modplug::mixgraph::id_t;



namespace modplug {
namespace gui {

const static char mixgraph_class[] = "mixgraph_view";

struct line {
    double x1, y1, x2, y2;
    int f;

    line(double x1_, double y1_, double x2_, double y2_) : 
        x1(x1_), y1(y1_), x2(x2_), y2(y2_), f(0) {}

    void rewind(unsigned) { f = 0; }
    unsigned vertex(double* x, double* y) {
        if(f == 0) { ++f; *x = x1; *y = y1; return agg::path_cmd_move_to; }
        if(f == 1) { ++f; *x = x2; *y = y2; return agg::path_cmd_line_to; }
        return agg::path_cmd_stop;
    }
};

struct bezier_curve_t {
    agg::curve4_div c;

    void set_endpoints(point_t &headpos, point_t &tailpos) {
        //c.approximation_scale(0.025);
        c.init(headpos.x, headpos.y, 
               headpos.x + 100, headpos.y,
               tailpos.x - 100, tailpos.y,
               tailpos.x, tailpos.y);
    }

    void rewind(unsigned path_id) { c.rewind(path_id); }
    unsigned vertex(double* x, double* y) { return c.vertex(x, y); }
};

struct relrect_t {
    int x;
    int y;
    int width;
    int height;
};

struct rectsize_t {
    int width;
    int height;
};

struct pointd_t {
    double x;
    double y;
};

enum sign_t {
    SIGN_ZERO,
    SIGN_POSITIVE,
    SIGN_NEGATIVE
};

bool rect_intersects_ideal_line(rect_t &rect, pointd_t &line_start, pointd_t &line_end) {
    auto line_test = [&line_start, &line_end](int test_x, int test_y) -> sign_t {
        auto val = (line_end.y - line_start.y) * test_x +
                   (line_start.x - line_end.x) * test_y +
                   (line_end.x * line_start.y - line_start.x * line_end.y);
        return (val > 0) ?
               (SIGN_POSITIVE) :
               ( (val < 0) ?
                 (SIGN_NEGATIVE) :
                 (SIGN_ZERO) );
    };

    sign_t sign;

    const auto old_sign = line_test(rect.left, rect.top);

    if ( old_sign != (sign = line_test(rect.right, rect.top)) )
        return true;

    if ( old_sign != (sign = line_test(rect.right, rect.bottom)) )
        return true;

    if ( old_sign != (sign = line_test(rect.left, rect.bottom)) )
        return true;

    return old_sign == SIGN_ZERO;
}

bool rect_intersects_rect(rect_t &r1, rect_t &r2) {
    return (! ( (r1.left   > r2.right)  ||
                (r1.top    > r2.bottom) ||
                (r1.right  < r2.left)   ||
                (r1.bottom < r2.top) ) );
}

rect_t rect_of_relrect(const relrect_t &relrect) {
    rect_t ret = {relrect.x,
                  relrect.y,
                  relrect.x + relrect.width,
                  relrect.y + relrect.height};
    return ret;
}

template <typename point_type>
rect_t rect_of_points(point_type p1, point_type p2) {
    rect_t ret = {
        static_cast<int32_t>(p1.x), static_cast<int32_t>(p1.y),
        static_cast<int32_t>(p2.x), static_cast<int32_t>(p2.y)
    };
    return ret;
}

struct graphicsbuffer_t {
    graphicsbuffer_t(void *_buffer, int _width, int _height) :
        buffer(reinterpret_cast<agg::int8u *>(_buffer)),
        width(_width),
        height(_height)
    { };
    agg::int8u *buffer;
    const int width;
    const int height;
};

void buffer_attach(agg::rendering_buffer &rbuf, graphicsbuffer_t &gfxbuf, int y_factor) {
    rbuf.attach(gfxbuf.buffer, gfxbuf.width, gfxbuf.height, y_factor * gfxbuf.width * 4);
}

bool rect_is_empty(rect_t &rect) {
    return !rect.bottom && !rect.left && !rect.right && !rect.top;
}

rectsize_t rect_size(rect_t &rect) {
    rectsize_t ret = {rect.right - rect.left, rect.bottom - rect.top};
    return ret;
}



struct mixgraph_layout_t {
    int vertical_padding;
    int nub_height;
    int half_nub_height;
    int nub_width;
    int main_width;

    agg::rgba8 background;
    agg::rgba8 inactive_foreground;
    agg::rgba8 active_foreground;
    agg::rgba8 text_color;
    agg::rgba8 line_color;
    agg::rgba8 selection_color;
    agg::rgba8 selection_color_outline;

    void init() {
        vertical_padding = 3;
        nub_height       = 5;
        half_nub_height  = nub_height / 2;
        nub_width        = 5;
        main_width       = 100;

        background          = agg::rgba8(0x1f, 0x1f, 0x1f, 0xff);
        inactive_foreground = agg::rgba8(0x40, 0x40, 0x40, 0xff);
        active_foreground   = agg::rgba8(0xff, 0xa6, 0x50, 0xff);
        text_color          = agg::rgba8(0xcc, 0xcc, 0xcc, 0xff);
        line_color          = agg::rgba8(0x60, 0x60, 0x60, 0xff);

        selection_color         = agg::rgba8(0xff, 0xa6, 0x50, 0x20);
        selection_color_outline = agg::rgba8(0xff, 0xa6, 0x50, 0xff);
    }
};

struct vertex_guistate_t;

struct arrow_guistate_t {
    point_t headpos;
    point_t tailpos;

    vertex_guistate_t *head;
    vertex_guistate_t *tail;

    modplug::mixgraph::arrow *arrow;

    bezier_curve_t curve;
    bool curve_updated;

    void init(vertex_guistate_t *, vertex_guistate_t *, modplug::mixgraph::arrow *);

    void update_headpoint(point_t headpos) {
        curve_updated = false;
        this->headpos.x = headpos.x;
        this->headpos.y = headpos.y;
    }

    void update_tailpoint(point_t tailpos) {
        curve_updated = false;
        this->tailpos.x = tailpos.x;
        this->tailpos.y = tailpos.y;
    }

    bezier_curve_t &path() {
        if (!curve_updated) {
            curve.set_endpoints(headpos, tailpos);
            curve_updated = true;
        }
        return curve;
    }
};

struct vertex_guistate_t {
    relrect_t pos;
    modplug::mixgraph::vertex *vertex;
    std::vector<arrow_guistate_t *> in_arrows;
    std::vector<arrow_guistate_t *> out_arrows;
    mixgraph_layout_t *layout;

    void init(mixgraph_layout_t *layout, modplug::mixgraph::vertex *vertex) {
        this->layout = layout;
        this->vertex = vertex;

        calc_bounding_box();
    }

    void calc_bounding_box() {
        size_t input_nubs  = vertex->_input_channels;
        size_t output_nubs = vertex->_output_channels;

        pos.width  = (2 * layout->nub_width) + layout->main_width;
        pos.height = max(input_nubs, output_nubs) * (layout->nub_height + layout->vertical_padding) + layout->vertical_padding;
    }

    point_t _nub_location(int channel, bool input) {
        point_t ret;

        const int half_height = layout->half_nub_height;

        ret.x = pos.x + (input ? (0) : (layout->main_width + 2 * layout->nub_width));
        ret.y = pos.y + (layout->vertical_padding + layout->nub_height) * (channel + 1) - half_height;

        return ret;
    }

    point_t in_nub_location(int channel) {
        return _nub_location(channel, true);
    }

    point_t out_nub_location(int channel) {
        return _nub_location(channel, false);
    }

    void move(point_t newpos) {
        pos.x = newpos.x;
        pos.y = newpos.y;
        calc_arrow_endpoints();
    }

    void calc_arrow_endpoints() {
        auto update_tail = [&] (arrow_guistate_t *item) {
            item->update_tailpoint(in_nub_location(item->arrow->tail_channel));
        };
        auto update_head = [&] (arrow_guistate_t *item) {
            item->update_headpoint(out_nub_location(item->arrow->head_channel));
        };

        for_each(in_arrows,  update_tail);
        for_each(out_arrows, update_head);
    }
};

template <typename gfxitem_t>
void hit_test(gfxitem_t item, point_t test_point) {
}

void normalize_rect(rect_t &rect) {
    if (rect.left > rect.right) std::swap(rect.left, rect.right);
    if (rect.top > rect.bottom) std::swap(rect.top, rect.bottom);
}

void arrow_guistate_t::init(vertex_guistate_t *head, vertex_guistate_t *tail, modplug::mixgraph::arrow *arrow) {
    this->head = head;
    this->tail = tail;
    this->arrow = arrow;
    curve_updated = false;

    update_headpoint(head->out_nub_location(arrow->head_channel));
    update_tailpoint(tail->in_nub_location(arrow->tail_channel));
}

struct mixgraph_selection_state_t {
    rect_t area;
    bool active;
};

struct agg_fontstate_t {
    typedef agg::font_engine_win32_tt_int16 font_engine_t;
    typedef agg::font_cache_manager<font_engine_t> font_cache_t;
    typedef agg::conv_curve<font_cache_t::path_adaptor_type> conv_font_curve_type;

    HDC dummy_dc;
    font_engine_t font_engine;
    font_cache_t font_cache;
    agg::glyph_rendering ftype_;
    agg::glyph_data_type fdata_;
    std::string font_face;

    agg_fontstate_t() :
        dummy_dc(GetDC(0)),
        font_engine(dummy_dc),
        font_cache(font_engine)
    { }

    ~agg_fontstate_t() {
        ReleaseDC(0, dummy_dc);
    }

    void init(std::string font_face, bool flip_y) {
        this->font_face = font_face;

        ftype_ = agg::glyph_ren_outline;
        fdata_ = agg::glyph_data_outline;

        font_engine.height(11);
        font_engine.flip_y(flip_y);
        font_engine.hinting(false);

        font_engine.create_font(this->font_face.c_str(), ftype_);
    }

    template <typename rasterizer_t, typename scanline_t, typename renderer_t>
    void draw_text(rasterizer_t &ras, scanline_t &sl, renderer_t &ren, int x, int y, const char *text, const agg::rgba8 color) {
        conv_font_curve_type fcurves(font_cache.path_adaptor());

        double tx = x;
        double ty = y;
        ren.color(color);
        while (*text) {
            const auto glyph = font_cache.glyph(*text);
            if (glyph) {
                font_cache.add_kerning(&tx, &ty);
                font_cache.init_embedded_adaptors(glyph, tx, ty);

                if (glyph->data_type == fdata_) {
                    ras.reset();
                    ras.add_path(fcurves);
                    agg::render_scanlines(ras, sl, ren);
                }

                tx += glyph->advance_x;
                ty += glyph->advance_y;
            }
            ++text;
        }
    }
};

struct mixgraph_viewstate_t {
    typedef agg::pixfmt_bgra32 pixel_format_t;

    typedef agg::renderer_base<pixel_format_t>              base_render_t;
    typedef agg::renderer_primitives<base_render_t>         primitive_renderer_t;
    typedef agg::renderer_scanline_aa_solid<base_render_t>  antialiased_renderer_t;
    typedef agg::renderer_scanline_bin_solid<base_render_t> aliased_renderer_t;

    typedef agg::rasterizer_scanline_aa<>                 antialiased_rasterizer_t;
    typedef agg::rasterizer_outline<primitive_renderer_t> primitive_rasterizer_t;

    hwnd_t hwnd;

    agg_fontstate_t font_manager;

    bool lmb_down_;
    bool flip_y;
    int y_factor;

    mixgraph_layout_t layout;

    modplug::mixgraph::core *graph;
    std::map<id_t, vertex_guistate_t *> vertex_guistate;
    std::map<id_t, arrow_guistate_t *> arrow_guistate;
    typedef std::pair<id_t, vertex_guistate_t *> vertex_mapitem_t;
    typedef std::pair<id_t, arrow_guistate_t *> arrow_mapitem_t;

    std::set<id_t> selected_ids;
    bool vertex_selected;
    modplug::mixgraph::id_t selected_id;

    point_t mouse_offset;
    bool hompy_selected;
    size_t hompy_idx;

    mixgraph_selection_state_t selection_state;

    mixgraph_viewstate_t(modplug::mixgraph::core *mixgraph) :
        flip_y(true),
        graph(mixgraph)
    {
        assert(mixgraph != nullptr);
    }

    bool init(hwnd_t ctrl_hwnd) {
        hwnd = ctrl_hwnd;

        y_factor = flip_y ? -1 : 1;
        lmb_down_ = false;
        hompy_selected = false;

        layout.init();

        selection_state.active = false;

        font_manager.init("Helvetica LT Std", flip_y);
        //font_manager.init("Tahoma", flip_y);

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


        auto &arrows = graph->arrows;
        for (auto iter = arrows.begin(); iter != arrows.end(); ++iter) {
            auto item = *iter;
            auto key = item.first;
            auto arrow = item.second;

            arrow_guistate_t *guiarrow = new arrow_guistate_t;
            arrow_guistate.insert(std::make_pair(item.first, guiarrow));

            auto &head = vertex_guistate[arrow->head->id];
            auto &tail = vertex_guistate[arrow->tail->id];
            guiarrow->init(head, tail, arrow);

            guiarrow->head->out_arrows.push_back(guiarrow);
            guiarrow->tail->in_arrows.push_back(guiarrow);
        }

        return true;
    }

    bool destroy() {
        return true;
    }

    void begin_region_selection(points_t mouse_pos) {
        selected_ids.clear();

        selection_state.active = true;
        selection_state.area.left = mouse_pos.x;
        selection_state.area.top  = mouse_pos.y;

        selection_state.area.bottom = selection_state.area.top;
        selection_state.area.right  = selection_state.area.left;
    }

    void update_region_selection(points_t mouse_pos) {
        if (selection_state.active) {
            selection_state.area.right  = mouse_pos.x;
            selection_state.area.bottom = mouse_pos.y;
            invalidate();
        }
    }

    void end_region_selection() {
        if (selection_state.active) {
            normalize_rect(selection_state.area);
            calculate_selection();
        }
        selection_state.active = false;
    }

    void calculate_selection() {
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

    int resized(int width, int height) {
        return 0;
    }

    template <typename rasterizer_t>
    int draw_nub(rasterizer_t &ras, int x, int y) {
        int width   = layout.nub_width;
        int height  = layout.nub_height;
        int padding = layout.vertical_padding;
    
        ras.move_to_d(x, y);
        ras.line_to_d(x + width, y);
        ras.line_to_d(x + width, y + height);
        ras.line_to_d(x, y + height);
        ras.line_to_d(x, y);
    
        return y + height + padding;
    }

    template <typename rasterizer_t, typename scanline_t>
    void draw_mainbox(rasterizer_t &ras, scanline_t &sl, int x, int y, int height, int width) {
        ras.move_to_d(x, y);
        ras.line_to_d(x + width, y);
        ras.line_to_d(x + width, y + height);
        ras.line_to_d(x, y + height);
        ras.line_to_d(x, y);
    }

    template <typename rasterizer_t, typename scanline_t, typename renderer_t>
    void draw_arrow(rasterizer_t &ras, scanline_t &sl, renderer_t &ren, arrow_guistate_t *guiarrow) {
        bezier_curve_t &curveheimer = guiarrow->path();
        agg::conv_stroke<bezier_curve_t> stroke(curveheimer);

        stroke.width(0.5);
        ras.add_path(stroke);

        auto id = guiarrow->arrow->id;
        auto &color = (selected_ids.find(id) != selected_ids.end()) ? (layout.active_foreground) : (layout.line_color);
        ren.color(color);
        agg::render_scanlines(ras, sl, ren);
    }

    template <typename rasterizer_t, typename scanline_t, typename renderer_t>
    void draw_arrows(rasterizer_t &ras, scanline_t &sl, renderer_t &ren) {
        auto f = [&] (arrow_mapitem_t item) { draw_arrow(ras, sl, ren, item.second); };
        for_each_rev(arrow_guistate, f);
    }

    
    template <typename rasterizer_t, typename scanline_t, typename renderer_t>
    void draw_vertex(rasterizer_t &ras, scanline_t &sl, renderer_t &ren, vertex_guistate_t *guivtx) {
        const char *label = guivtx->vertex->name.c_str();
        int x = guivtx->pos.x;
        int y = guivtx->pos.y;

        auto out_nubs = guivtx->vertex->_output_channels;
        auto in_nubs  = guivtx->vertex->_input_channels;

        int width    = layout.main_width;
        int outnub_x = x + width + layout.nub_width;
        int padding  = layout.vertical_padding;
    
        int out_nub_y = y + padding;
        for (size_t idx = 0; idx < out_nubs; ++idx) {
            out_nub_y = draw_nub(ras, outnub_x, out_nub_y);
        }
    
        int in_nub_y = y + padding;
        for (size_t idx = 0; idx < in_nubs; ++idx) {
            in_nub_y = draw_nub(ras, x, in_nub_y);
        }
    
        int height = max(out_nub_y, in_nub_y) - y;
    
        draw_mainbox(ras, sl, x + layout.nub_width, y, height, width);

        auto id = guivtx->vertex->id;
        auto &color = (selected_ids.find(id) != selected_ids.end()) ? (layout.active_foreground) : (layout.inactive_foreground);
        ren.color(color);

        agg::render_scanlines(ras, sl, ren);
    
        font_manager.draw_text(ras, sl, ren, x, y, label, layout.text_color);
    }

    template <typename rasterizer_t, typename scanline_t, typename renderer_t>
    void draw_vertices(rasterizer_t &ras, scanline_t &sl, renderer_t &ren) {
        auto f = [&] (vertex_mapitem_t item) { draw_vertex(ras, sl, ren, item.second); };
        for_each_rev(vertex_guistate, f);
    }

    template <typename scanline_t>
    void draw_selection_box(primitive_rasterizer_t &ras, scanline_t &sl, primitive_renderer_t &ren) {
        if (selection_state.active) {
            rect_t r = selection_state.area;
            normalize_rect(r);
            ren.line_color(layout.selection_color_outline);
            ren.fill_color(layout.selection_color);
            ren.outlined_rectangle(r.left, r.top, r.right, r.bottom);
        }
    }

    template <typename rasterizer_t, typename scanline_t, typename renderer_t>
    void draw_clipping_debug_box(rasterizer_t &ras, scanline_t &sl, renderer_t &ren, rect_t &box) {
        ren.color(agg::rgba8(rand() % 255, rand() % 255, rand() % 255, 0x10));
        ras.move_to_d(box.left, box.top);
        ras.line_to_d(box.right, box.top);
        ras.line_to_d(box.right, box.bottom);
        ras.line_to_d(box.left, box.bottom);
        agg::render_scanlines(ras, sl, ren);
    }
    
    void paint_agg(graphicsbuffer_t &gfxbuf, rect_t &clipping_rect, rectsize_t &client_size) {
        agg::rendering_buffer rbuf;
        buffer_attach(rbuf, gfxbuf, y_factor);
    
        pixel_format_t pixf(rbuf);
        base_render_t  renb(pixf);
    

        antialiased_renderer_t antialiased(renb);
        primitive_renderer_t   primitive(renb);

        antialiased_rasterizer_t antialiased_rasterizer;
        primitive_rasterizer_t   primitive_rasterizer(primitive);
        agg::scanline_p8 sl;

        antialiased_rasterizer.clip_box(clipping_rect.left, clipping_rect.top, clipping_rect.right, clipping_rect.bottom);

        renb.clear(layout.background);

        draw_arrows(antialiased_rasterizer, sl, antialiased);
        draw_vertices(antialiased_rasterizer, sl, antialiased);

        draw_selection_box(primitive_rasterizer, sl, primitive);

        //draw_clipping_debug_box(antialiased_rasterizer, sl, antialiased, clipping_rect);
    }

    bool hit_test(arrow_guistate_t *arrow) {
    }

    int lmb_down(points_t mouse_pos) {
        SetCapture(this->hwnd);
        this->lmb_down_ = true;
        this->hompy_selected = false;
        if (graph) {
            for (auto iter = vertex_guistate.begin(); iter != vertex_guistate.end(); ++iter) {
                auto id = iter->first;
                auto vtx = iter->second;
                int x = vtx->pos.x;
                int y = vtx->pos.y;
                int cx = vtx->pos.x + vtx->pos.width;
                int cy = vtx->pos.y + vtx->pos.height;
                //debug_log("mouse (%d, %d) testing against (%d, %d), (%d, %d)", mouse_pos.x, mouse_pos.y, x, y, cx, cy);
    
                if ((mouse_pos.x >= x && mouse_pos.x < cx) && (mouse_pos.y >= y && mouse_pos.y < cy)) {
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
        return 0;
    }

    int lmb_double_click(points_t mouse_pos) {
        // activate plugin menu
        return 0;
    }

    int lmb_up() {
        this->end_region_selection();
        this->lmb_down_ = false;
        this->hompy_selected = false;
        ReleaseCapture();
        invalidate();
        return 0;
    }

    int mouse_moved(points_t mouse_pos) {
        this->update_region_selection(mouse_pos);
        if (this->hompy_selected) {
            auto &selected_vertex = vertex_guistate[selected_id];
            point_t newpos = {mouse_pos.x + mouse_offset.x, mouse_pos.y + mouse_offset.y};
            selected_vertex->move(newpos);
            invalidate();
        }
        return 0;
    }

    void invalidate() {
        InvalidateRect(this->hwnd, nullptr, false);
        UpdateWindow(this->hwnd);
        //force_paint();
        //RedrawWindow(hwnd, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE | RDW_UPDATENOW);
    }

    void paint_common(hdc_t hdc, rect_t &update_rect, rect_t &client_rect) {
        auto update_size = rect_size(update_rect);
        auto client_size = rect_size(client_rect);
    
        BITMAPINFO bmp_info;
        bmp_info.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
        bmp_info.bmiHeader.biWidth         = client_size.width;
        bmp_info.bmiHeader.biHeight        = client_size.height;
        bmp_info.bmiHeader.biPlanes        = 1;
        bmp_info.bmiHeader.biBitCount      = 32;
        bmp_info.bmiHeader.biCompression   = BI_RGB;
        bmp_info.bmiHeader.biSizeImage     = 0;
        bmp_info.bmiHeader.biXPelsPerMeter = 0;
        bmp_info.bmiHeader.biYPelsPerMeter = 0;
        bmp_info.bmiHeader.biClrUsed       = 0;
        bmp_info.bmiHeader.biClrImportant  = 0;
    
        hdc_t mem_dc = CreateCompatibleDC(hdc);
        void *buf = nullptr;
    
        HBITMAP bmp  = CreateDIBSection(mem_dc, &bmp_info, DIB_RGB_COLORS, &buf, 0, 0);
        HBITMAP temp = (HBITMAP) SelectObject(mem_dc, bmp);

        graphicsbuffer_t gfxbuf(buf, client_size.width, client_size.height);
    
        paint_agg(gfxbuf, update_rect, client_size);
    

        BitBlt(hdc,
               update_rect.left, update_rect.top,
               update_size.width, update_size.height,
               mem_dc,
               update_rect.left, update_rect.top,
               SRCCOPY);
    
        SelectObject(mem_dc, temp);
        DeleteObject(bmp);
        DeleteObject(mem_dc);
    }

    void force_paint() {
        rect_t update_rect;
        GetClientRect(hwnd, &update_rect);

        hdc_t hdc = GetDC(hwnd);
        paint_common(hdc, update_rect, update_rect);
        ReleaseDC(hwnd, hdc);
    }
    
    LRESULT paint() {
        PAINTSTRUCT ps;

        rect_t update_rect;

        if (GetUpdateRect(hwnd, &update_rect, FALSE)) {
            rect_t client_rect;
            GetClientRect(hwnd, &client_rect);

            hdc_t hdc = BeginPaint(hwnd, &ps);
            paint_common(hdc, update_rect, client_rect);
            EndPaint(hwnd, &ps);
        }
        return 0;
    }
};



DEFINE_WNDPROC(mixgraph_viewstate_t);

void mixgraph_view_register() {
    WNDCLASSEX wc;

    wc.cbSize        = sizeof(wc);
    wc.lpszClassName = mixgraph_class;
    wc.hInstance     = GetModuleHandle(0);
    wc.lpfnWndProc   = &wndproc_mixgraph_viewstate_t;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = 0;
    wc.lpszMenuName  = nullptr;
    wc.hbrBackground = (HBRUSH) GetSysColorBrush(COLOR_BTNFACE);
    wc.style         = CS_DBLCLKS;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(mixgraph_viewstate_t *);
    wc.hIconSm       = 0;

    RegisterClassEx(&wc);

    test_view_register();
}

hwnd_t mixgraph_view_create(mixgraph_viewstate_t *state, hwnd_t parent) {
    assert(state != nullptr);

    hwnd_t hwnd;
    hwnd = CreateWindowEx(WS_EX_LEFT, mixgraph_class, "ya",
                          WS_CHILD | WS_VISIBLE, 0, 0, 2000, 2000, parent,
                          nullptr, nullptr, state);
    return hwnd;
}

mixgraph_viewstate_t *mixgraph_view_create_state(modplug::mixgraph::core *graph) {
    auto state = new (std::nothrow) mixgraph_viewstate_t(graph);
    return state;
}

void mixgraph_view_delete_state(mixgraph_viewstate_t *state) {
    state->destroy();
    delete state;
}












struct hooperstate_t {
    hwnd_t graphchild;
    hwnd_t hwnd;

    hooperstate_t() {
        graphchild = 0;
    }

    void init(hwnd_t hwnd) {
        this->hwnd = hwnd;
    }

    void resized(int width, int height) {
        if (graphchild) {
            SetWindowPos(graphchild, HWND_TOP, 0, 0, width, height, SWP_NOZORDER);
        }
    }
};

DEFINE_WNDPROC(hooperstate_t);

void test_view_register() {
    WNDCLASSEX wc;

    wc.cbSize        = sizeof(wc);
    wc.lpszClassName = "testwindow";
    wc.hInstance     = GetModuleHandle(0);
    wc.lpfnWndProc   = wndproc_hooperstate_t;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = 0;
    wc.lpszMenuName  = nullptr;
    wc.hbrBackground = (HBRUSH) GetSysColorBrush(COLOR_BTNFACE);
    wc.style         = 0;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(hooperstate_t *);
    wc.hIconSm       = 0;

    RegisterClassEx(&wc);
}


void show_my_weldus(modplug::mixgraph::core *graph) {
    auto hooper = new (std::nothrow) hooperstate_t;
    auto hwnd   = CreateWindowEx(0, "testwindow", "my hompy", WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 1000, 1000,
                                 nullptr, nullptr, nullptr, hooper);
    debug_log("show_my_weldus CreateWindow lasterror = %d", GetLastError());
    auto state  = mixgraph_view_create_state(graph);
    auto child  = mixgraph_view_create(state, hwnd);
    debug_log("show_my_weldus mixgraph_view_create lasterror = %d", GetLastError());
    hooper->graphchild = child;

    debug_log("lasterror = %d", GetLastError());
    debug_log("show_my_weldus hwnd = %d    state = %p    child = %d", hwnd, state, child);

    ShowWindow(hwnd, SW_SHOW);
}




}
}
