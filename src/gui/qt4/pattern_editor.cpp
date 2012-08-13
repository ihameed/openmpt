#include "stdafx.h"

#include <cassert>

#include "pattern_editor.h"
#include "../../Resource.h"

#include "../../mptrack.h"

#include "../../pervasives/pervasives.h"
#include "../../legacy_soundlib/sndfile.h"
#include "../../tracker/constants.h"

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt4 {

const int column_width = 100;
const int row_height = 15;

const int column_header_height = 20;
const int row_header_width = 60;

inline int rect_bottom(const QRect &rect) {
    return rect.top() + rect.height();
}

inline int rect_right(const QRect &rect) {
    return rect.left() + rect.width();
}

QPixmap load_bmp_resource(LPTSTR resource_name) {
    auto hinstance = (HINSTANCE) GetModuleHandle(nullptr);
    auto hrsrc     = FindResource(hinstance, resource_name, RT_BITMAP);
    auto hglobal   = LoadResource(hinstance, hrsrc);
    auto info      = (LPBITMAPINFO) LockResource(hglobal);

    auto color_table_size = [](BITMAPINFOHEADER &header) -> size_t {
        switch (header.biBitCount) {
        case 1: return 2;
        case 4: return 16;
        case 8: return 256;

        case 16:
        case 24:
        case 32: return header.biClrUsed;

        default:
            DEBUG_FUNC("invalid bpp = %d", header.biBitCount);
            return 0;
        }
    };

    if (!info) {
        DEBUG_FUNC("failed to load resource \"%s\"", resource_name);
    }

    auto &header     = info->bmiHeader;
    auto hdc         = GetDC(nullptr);
    const void *data = &info->bmiColors + color_table_size(header);
    auto hbitmap     = CreateDIBitmap(hdc, &header, CBM_INIT, data, info,
                                      DIB_RGB_COLORS);
    QPixmap pixmap   = QPixmap::fromWinHBITMAP(hbitmap, QPixmap::NoAlpha);
    ReleaseDC(nullptr, hdc);
    DeleteObject(hbitmap);

    DEBUG_FUNC("pixmap colorcount = %d, width = %d, height = %d",
               pixmap.colorCount(), pixmap.width(), pixmap.height());

    return pixmap;
}

struct draw_state {
    const module_renderer &renderer;
    QPainter &painter;
    const QRect &clip;
    const QPixmap &font;
    const pattern_font_metrics_t &font_metrics;
    int pattern_number;
};

struct pattern_column: modplug::pervasives::noncopyable {
    virtual void draw_header(const draw_state &, int) = 0;
    virtual void draw_column(const draw_state &, int) = 0;
};

struct automation_column: pattern_column {
};

struct wave_column: pattern_column {
};

struct note_column: pattern_column {
    virtual void draw_header(const draw_state &state, int column) {
        QRect rect(column_width * column, 0,
                   column_width, column_header_height);
        state.painter.drawRect(rect);
        const char *fmt = state.renderer.m_bChannelMuteTogglePending[column]
            ? "[Channel %1]"
            : "Channel %1";
        state.painter.drawText(rect, Qt::AlignCenter,
                               QString(fmt).arg(column + 1));
    }

    virtual void draw_column(const draw_state &state, int column) {
        auto current_pattern = state.renderer.Patterns[state.pattern_number];
        ROWINDEX nrows = current_pattern ? current_pattern.GetNumRows() : 0;

        const QRect rect(
            column * column_width,
            column_header_height,
            column_width,
            row_height
        );
        QRect cell(rect);

        auto clip_bottom = rect_bottom(state.clip);

        for (size_t i = 0; i < nrows && rect_bottom(cell) <= clip_bottom; ++i) {
            draw_row(state, current_pattern,
                     i, column,
                     cell.left(), cell.top());
            cell.setTop(cell.top() + row_height);
        }
    }

    void draw_row(const draw_state &state, const CPattern &pattern, int row,
        int column, int x, int y)
    {
        auto &evt = pattern.GetModCommand(row, column);
        auto &widths = state.font_metrics.element_widths;
        draw_note(state, x, y, evt);
        x += widths[elem_note];

        draw_instr(state, x, y, evt);
        x += widths[elem_instr];

        draw_vol(state, x, y, evt);
        x += widths[elem_vol];

        draw_cmd(state, x, y, evt);
        x += widths[elem_cmd];
    }

    void draw_note(const draw_state &state, int x, int y,
        const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;
        auto note = evt.note;
        auto srcx = metrics.note_x;
        auto srcy = metrics.note_y;
        auto width = metrics.element_widths[elem_note];

        switch (note) {
        case 0:
            draw_glyph(state, x, y, srcx, srcy, width);
            break;
        case NOTE_NOTECUT:
            draw_glyph(state, x, y, srcx, srcy + 13 * grid_height, width);
            break;
        case NOTE_KEYOFF:
            draw_glyph(state, x, y, srcx, srcy + 14 * grid_height, width);
            break;
        case NOTE_FADE:
            draw_glyph(state, x, y, srcx, srcy + 17 * grid_height, width);
            break;
        case NOTE_PC:
            draw_glyph(state, x, y, srcx, srcy + 15 * grid_height, width);
            break;
        case NOTE_PCS:
            draw_glyph(state, x, y, srcx, srcy + 16 * grid_height, width);
            break;
        default:
            unsigned int octave   = (note - 1) / 12;
            unsigned int notepart = (note - 1) % 12;
            draw_glyph(state, x, y,
                srcx, srcy + (notepart + 1) * grid_height,
                metrics.note_width);
            if (octave <= 9) {
                draw_glyph(state,
                    x + metrics.note_width, y,
                    metrics.num_x, metrics.num_y + octave * grid_height,
                    metrics.octave_width
                );
            } else {
            }
        }
    }

    void draw_instr(const draw_state &state, int x, int y,
        const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;
        auto instr = evt.instr;
        if (instr) {
            auto width = metrics.instr_firstchar_width;
            if (instr < 100) {
                draw_glyph(state, x, y,
                    metrics.num_x + metrics.instr_offset,
                    metrics.num_y + (instr / 10) * grid_height,
                    width
                );
            } else {
                draw_glyph(state, x, y,
                    metrics.num_10_x + metrics.instr_10_offset,
                    metrics.num_10_y + ((instr - 100) / 10) * grid_height,
                    width
                );
            }
            draw_glyph(state, x + width, y,
                metrics.num_x + 1, metrics.num_y + (instr % 10) * grid_height,
                metrics.element_widths[elem_instr] - width
            );

        } else {
            draw_glyph(state, x, y,
                metrics.clear_x + x,
                metrics.clear_y,
                metrics.element_widths[elem_instr]
            );
        }
    }

    void draw_vol(const draw_state &state, int x, int y,
        const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;
        if (evt.volcmd) {
            auto volcmd = evt.volcmd & 0x0f;
            auto vol    = evt.vol & 0x7f;

            draw_glyph(state, x, y,
                metrics.nVolX, metrics.nVolY + volcmd * grid_height,
                metrics.vol_width
            );

            draw_glyph(state, x + metrics.vol_width, y,
                metrics.num_x, metrics.num_y + (vol / 10) * grid_height,
                metrics.vol_firstchar_width
            );

            auto extrawidth = metrics.vol_width + metrics.vol_firstchar_width;
            draw_glyph(state,
                x + extrawidth, y,
                metrics.num_x, metrics.num_y + (vol / 10) * grid_height,
                metrics.element_widths[elem_vol] - extrawidth
            );
        } else {
            draw_glyph(state, x, y,
                metrics.clear_x + x, metrics.clear_y,
                metrics.element_widths[elem_vol]
            );
        }
    }

    void draw_cmd(const draw_state &state, int x, int y,
        const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;
        if (evt.command) {
            auto rawcommand = evt.command & 0x3f;
            char command = state.renderer.m_nType & (MOD_TYPE_MOD | MOD_TYPE_XM)
                ? mod_command_glyphs[rawcommand]
                : s3m_command_glyphs[rawcommand];

            draw_letter(state, x, y,
                metrics.element_widths[elem_cmd], metrics.cmd_offset,
                command);
        } else {
            draw_glyph(state, x, y,
                metrics.clear_x + x,
                metrics.clear_y,
                metrics.element_widths[elem_cmd]);
        }
    }

    void draw_glyph(const draw_state &state, int x, int y,
                   int glyph_x, int glyph_y, int glyph_width)
    {
        state.painter.drawPixmap(
            x, y, state.font,
            glyph_x, glyph_y, glyph_width,
            state.font_metrics.height
        );
    }

    void draw_letter(const draw_state &state, int x, int y,
        int width, int x_offset, char letter)
    {
        auto &metrics = state.font_metrics;
        auto srcx = metrics.space_x;
        auto srcy = metrics.space_y;

        if ((letter >= '0') && (letter <= '9')) {
            srcx = metrics.num_x;
            srcy = metrics.num_y + (letter - '0') * grid_height;
        } else if ((letter >= 'A') && (letter < 'N')){
            srcx = metrics.alpha_am_x;
            srcy = metrics.alpha_am_y + (letter - 'A') * grid_height;
        } else if ((letter >= 'N') && (letter <= 'Z')) {
            srcx = metrics.alpha_nz_x;
            srcy = metrics.alpha_nz_y + (letter - 'N') * grid_height;
        } else switch(letter) {
        case '?':
            srcx = metrics.alpha_nz_x;
            srcy = metrics.alpha_nz_y + 13 * grid_height;
            break;
        case '#':
            srcx = metrics.alpha_am_x;
            srcy = metrics.alpha_am_y + 13 * grid_height;
            break;
        case '\\':
            srcx = metrics.alpha_nz_x;
            srcy = metrics.alpha_nz_y + 14 * grid_height;
            break;
        case ':':
            srcx = metrics.alpha_nz_x;
            srcy = metrics.alpha_nz_y + 15 * grid_height;
            break;
        case ' ':
            srcx = metrics.space_x;
            srcy = metrics.space_y;
            break;
        case '-':
            srcx = metrics.alpha_am_x;
            srcy = metrics.alpha_am_y + 15 * grid_height;
            break;
        case 'b':
            srcx = metrics.alpha_am_x;
            srcy = metrics.alpha_am_y + 14 * grid_height;
            break;
        }

        draw_glyph(state, x, y,
            srcx + x_offset, srcy,
            width);

        state.painter.drawPixmap(x, y, state.font, srcx, srcy, 13, 13);
    }
};

pattern_editor::pattern_editor(module_renderer &renderer) :
    renderer(renderer), font_metrics(small_pattern_font), font_loaded(false)
{ }

void pattern_editor::paintEvent(QPaintEvent *evt) {
    if (!font_loaded) {
        font = load_bmp_resource(MAKEINTRESOURCE(IDB_PATTERNVIEW));
        font_loaded = true;
    }

    QPainter painter(this);
    painter.setRenderHints( QPainter::Antialiasing
                          | QPainter::TextAntialiasing
                          | QPainter::SmoothPixmapTransform
                          | QPainter::HighQualityAntialiasing
                          , false);
    painter.setViewTransformEnabled(false);
    const QRect &clipping_rect = evt->rect();

    CHANNELINDEX channel_count = renderer.GetNumChannels();
    note_column notehomie;

    draw_state state = {
        renderer,
        painter,
        clipping_rect,
        font,
        font_metrics,
        0
    };

    for (CHANNELINDEX idx = 0; idx < channel_count; ++idx) {
        notehomie.draw_header(state, idx);
        notehomie.draw_column(state, idx);
    }
}

}
}
}