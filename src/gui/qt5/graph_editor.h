#pragma once

#include <QWidget>

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

#include <set>

#include "../../mixgraph/core.h"
#include "../../pervasives/pervasives.h"

namespace modplug {
namespace gui {
namespace qt5 {

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

struct point_t {
    int x;
    int y;
};

struct rect_t {
    int left;
    int top;
    int right;
    int bottom;
};


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

inline bool rect_intersects_ideal_line(rect_t &rect, pointd_t &line_start, pointd_t &line_end) {
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

inline bool rect_intersects_rect(rect_t &r1, rect_t &r2) {
    return (! ( (r1.left   > r2.right)  ||
                (r1.top    > r2.bottom) ||
                (r1.right  < r2.left)   ||
                (r1.bottom < r2.top) ) );
}

inline rect_t rect_of_relrect(const relrect_t &relrect) {
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

inline bool rect_is_empty(rect_t &rect) {
    return !rect.bottom && !rect.left && !rect.right && !rect.top;
}

inline rectsize_t rect_size(rect_t &rect) {
    rectsize_t ret = {rect.right - rect.left, rect.bottom - rect.top};
    return ret;
}





template <typename gfxitem_t>
void hit_test(gfxitem_t item, point_t test_point) {
}

inline void normalize_rect(rect_t &rect) {
    if (rect.left > rect.right) std::swap(rect.left, rect.right);
    if (rect.top > rect.bottom) std::swap(rect.top, rect.bottom);
}

struct mixgraph_selection_state_t {
    rect_t area;
    bool active;
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
        pos.height = bad_max(input_nubs, output_nubs) * (layout->nub_height + layout->vertical_padding) + layout->vertical_padding;
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

        modplug::pervasives::for_each(in_arrows,  update_tail);
        modplug::pervasives::for_each(out_arrows, update_head);
    }
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

    void init(std::string font_face) {
        this->font_face = font_face;

        ftype_ = agg::glyph_ren_outline;
        fdata_ = agg::glyph_data_outline;

        font_engine.height(11);
        font_engine.flip_y(true);
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



class graph_editor : public QWidget {
    Q_OBJECT
public:
    typedef agg::pixfmt_bgra32 pixel_format_t;

    typedef agg::renderer_base<pixel_format_t>              base_render_t;
    typedef agg::renderer_primitives<base_render_t>         primitive_renderer_t;
    typedef agg::renderer_scanline_aa_solid<base_render_t>  antialiased_renderer_t;
    typedef agg::renderer_scanline_bin_solid<base_render_t> aliased_renderer_t;

    typedef agg::rasterizer_scanline_aa<>                 antialiased_rasterizer_t;
    typedef agg::rasterizer_outline<primitive_renderer_t> primitive_rasterizer_t;

    typedef modplug::mixgraph::id_t id_t;

    graph_editor(modplug::mixgraph::core *);
    void begin_region_selection(point_t);
    void update_region_selection(point_t);
    void end_region_selection();
    void calculate_selection();
    void invalidate();
    void lmb_down(point_t);
    void lmb_up();
    void mouse_moved(point_t);
    void paint_agg(agg::rendering_buffer &, rect_t &, rectsize_t &);

    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

    void paintEvent(QPaintEvent *) override;

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

        int height = bad_max(out_nub_y, in_nub_y) - y;

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
        auto color = agg::rgba8(rand() % 255, rand() % 255, rand() % 255, 0x10);
        ren.fill_color(color);
        ren.line_color(color);
        ren.outlined_rectangle(box.left, box.top, box.right, box.bottom);
    }

    agg_fontstate_t font_manager;

    bool lmb_down_;

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
};

}
}
}
