#include "stdafx.h"

#include <cassert>

#include "pattern_editor.h"
#include "../../Resource.h"

#include "../../pervasives/pervasives.h"
#include "../../legacy_soundlib/sndfile.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt4 {

const int column_height = 13;

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

inline const pattern_font_spec_t &pattern_font() {
    return small_pattern_font;
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
    auto hbitmap     = CreateDIBitmap(hdc, &header, CBM_INIT,
                                      data, info, DIB_RGB_COLORS);
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
    int pattern_number;
};

struct pattern_column: modplug::pervasives::noncopyable {
    virtual void draw_header(const draw_state &, int) = 0;
    virtual void draw_column(const draw_state &, int) = 0;
};

struct note_column: pattern_column {
    virtual void draw_header(const draw_state &state, int column) {
        QRect rect(column_width * column, 0, column_width, column_header_height);
        state.painter.drawRect(rect);
        const char *fmt = state.renderer.m_bChannelMuteTogglePending[column]
            ? "[Channel %1]"
            : "Channel %1";
        state.painter.drawText(rect, Qt::AlignCenter, QString(fmt).arg(column + 1));
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
            //state.painter.drawRect(cell);
            //state.painter.drawText(cell, Qt::AlignRight, QString("ya %1").arg(i));
            //state.painter.save();
            cell.setTop(cell.top() + row_height);
        }
    }

    void draw_bitmap_char(const draw_state &state, int x, int y, char letter) {
        auto &font = pattern_font();

        int srcx = font.space_x, srcy = font.space_y;

        if ((letter >= '0') && (letter <= '9')) {
            srcx = font.num_x;
            srcy = font.num_y + (letter - '0') * column_height;
        } else if ((letter >= 'A') && (letter < 'N')) {
            srcx = font.alpha_am_x;
            srcy = font.alpha_am_y + (letter - 'A') * column_height;
        } else if ((letter >= 'N') && (letter <= 'Z')) {
            srcx = font.alpha_nz_x;
            srcy = font.alpha_nz_y + (letter - 'N') * column_height;
        } else switch(letter) {
        case '?':
            srcx = font.alpha_nz_x;
            srcy = font.alpha_nz_y + 13 * column_height;
            break;
        case '#':
            srcx = font.alpha_am_x;
            srcy = font.alpha_am_y + 13 * column_height;
            break;
        case '\\':
            srcx = font.alpha_nz_x;
            srcy = font.alpha_nz_y + 14 * column_height;
            break;
        case ':':
            srcx = font.alpha_nz_x;
            srcy = font.alpha_nz_y + 15 * column_height;
            break;
        case ' ':
            srcx = font.space_x;
            srcy = font.space_y;
            break;
        case '-':
            srcx = font.alpha_am_x;
            srcy = font.alpha_am_y + 15 * column_height;
            break;
        case 'b':
            srcx = font.alpha_am_x;
            srcy = font.alpha_am_y + 14 * column_height;
            break;
        }
    }
};

pattern_editor::pattern_editor(module_renderer &renderer) :
    renderer(renderer), font_loaded(false)
{ }

void pattern_editor::paintEvent(QPaintEvent *evt) {
    if (!font_loaded) {
        font = load_bmp_resource(MAKEINTRESOURCE(IDB_PATTERNVIEW));
        font_loaded = true;
    }

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing
                         | QPainter::TextAntialiasing
                         | QPainter::SmoothPixmapTransform
                         | QPainter::HighQualityAntialiasing
                         , false);
    painter.setViewTransformEnabled(false);
    const QRect &clipping_rect = evt->rect();

    CHANNELINDEX channel_count = renderer.GetNumChannels();
    note_column notehomie;

    draw_state state = { renderer, painter, clipping_rect, 0 };

    for (CHANNELINDEX idx = 0; idx < channel_count; ++idx) {
        notehomie.draw_header(state, idx);
        notehomie.draw_column(state, idx);
    }

    painter.drawPixmap(0, 0, font);

    painter.save();
}

}
}
}