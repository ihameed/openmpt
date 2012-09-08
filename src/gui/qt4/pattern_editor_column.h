#pragma once

#include "pattern_editor.h"
#include "../../Resource.h"

#include "../../mptrack.h"

#include "../../pervasives/pervasives.h"
#include "../../legacy_soundlib/sndfile.h"
#include "../../tracker/constants.h"

#include "colors.h"
#include "util.h"
#include "../legacy_soundlib/Snd_defs.h"

#include <GL/gl.h>

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt4 {

static const int column_header_height = 20;

struct draw_state {
    const module_renderer &renderer;

    const patternindex_t num_patterns;

    //QPainter &painter;
    const QRect &clip;

    const int standard_width;
    const int row_height;

    //QImage &font;
    GLuint *font_textures;
    const pattern_font_metrics_t &font_metrics;
    colors_t::colortype_t background;

    const editor_position_t &playback_pos;
    const editor_position_t &active_pos;

    const colors_t &colors;
};

struct note_column {
    draw_state &state;
    colors_t::colortype_t foreground;

    note_column(draw_state &state) : state(state) { }

    void draw_header(draw_state &state, int column) {
        QRect rect(state.standard_width * column, 0,
                   state.standard_width, column_header_height);
        //state.painter.drawRect(rect);
        const char *fmt = state.renderer.m_bChannelMuteTogglePending[column]
            ? "[Channel %1]"
            : "Channel %1";
            /*
        state.painter.drawText(rect, Qt::AlignCenter,
                               QString(fmt).arg(column + 1));
                               */
    }

    void draw_column(draw_state &state, int column) {
        if (state.active_pos.pattern > state.num_patterns) {
            return;
        }
        auto pattern = state.renderer.Patterns[state.active_pos.pattern];
        rowindex_t nrows = pattern ? pattern.GetNumRows() : 0;

        auto clip_bottom = rect_bottom(state.clip);

        const int left = column * state.standard_width;
        const int height = state.font_metrics.height;

        int top = column_header_height + 1;

        for (size_t row = 0;
             row < nrows && top + height <= clip_bottom;
             ++row)
        {
            draw_background(
                state, left, top,
                background_color(state, pattern, row)
            );
            draw_row(state, pattern,
                     row, column,
                     left, top);
            top += height;
        }
    }

    colors_t::colortype_t background_color(
        draw_state &state,
        const CPattern &pattern,
        size_t row)
    {
        if (row == state.playback_pos.row &&
            state.active_pos.pattern == state.playback_pos.pattern)
        {
            return colors_t::PlayCursor;
        }

        auto measure_length  = state.renderer.m_nDefaultRowsPerMeasure;
        auto beat_length     = state.renderer.m_nDefaultRowsPerBeat;

        if (pattern.GetOverrideSignature()) {
            measure_length = pattern.GetRowsPerMeasure();
            beat_length    = pattern.GetRowsPerBeat();
        }

        if (row % measure_length == 0) {
            return colors_t::HighlightPrimary;
        } else if (row % beat_length == 0) {
            return colors_t::HighlightPrimary;
        } else {
            return colors_t::Normal;
        }
    }

    void draw_background(draw_state &state, int x, int y,
        colors_t::colortype_t color)
    {
        const float left = x;
        const float top = y;
        const float right = x + state.font_metrics.width;
        const float bottom = y + state.font_metrics.height;

        GLint vertices[] = {
            left,  top,
            right, top,
            right, bottom,
            left,  bottom
        };

        const auto &qcolor = state.colors[color].background;

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_TEXTURE_COORD_ARRAY);
        glColor3f(qcolor.redF(), qcolor.greenF(), qcolor.blueF());
        glVertexPointer(2, GL_INT, 0, vertices);
        glDrawArrays(GL_QUADS, 0, 4);
    }

    void draw_row(draw_state &state, const CPattern &pattern,
                  int row, int column, int x, int y)
    {
        auto &evt = pattern.GetModCommand(row, column);
        auto &widths = state.font_metrics.element_widths;

        draw_note(state, x, y, evt);
        x += widths[ElemNote];

        draw_instr(state, x, y, evt);
        x += widths[ElemInstr];

        draw_vol(state, x, y, evt);
        x += widths[ElemVol];

        draw_cmd(state, x, y, evt);
        x += widths[ElemCmd];

        draw_param(state, x, y, evt);
        x += widths[ElemParam];
    }

    void draw_note(draw_state &state, int x, int y,
                   const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;
        auto note = evt.note;
        auto srcx = metrics.note_x;
        auto srcy = metrics.note_y;
        auto width = metrics.element_widths[ElemNote];

        foreground = colors_t::Note;

        switch (note) {
        case 0:
            draw_spacer(state, x, y, ElemNote);
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
                metrics.note_width
            );
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

    void draw_instr(draw_state &state, int x, int y,
                    const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;
        auto instr = evt.instr;

        if (instr) {
            auto width = metrics.instr_firstchar_width;

            foreground = colors_t::Instrument;

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
                metrics.element_widths[ElemInstr] - width
            );

        } else {
            draw_spacer(state, x, y, ElemInstr);
        }
    }

    void draw_vol(draw_state &state, int x, int y,
                  const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;

        if (evt.volcmd) {
            auto volcmd = evt.volcmd & 0x0f;
            auto vol    = evt.vol & 0x7f;

            foreground = colors_t::Volume;

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
                metrics.element_widths[ElemVol] - extrawidth
            );
        } else {
            draw_spacer(state, x, y, ElemVol);
        }
    }

    void draw_cmd(draw_state &state, int x, int y,
                  const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;

        if (evt.command) {
            foreground = colors_t::Pitch;
            auto rawcommand = evt.command & 0x3f;
            char command = state.renderer.m_nType & (MOD_TYPE_MOD | MOD_TYPE_XM)
                ? mod_command_glyphs[rawcommand]
                : s3m_command_glyphs[rawcommand];

            draw_letter(state, x, y,
                metrics.element_widths[ElemCmd], metrics.cmd_offset,
                command);
        } else {
            draw_spacer(state, x, y, ElemCmd);
        }
    }

    void draw_param(draw_state &state, int x, int y,
                    const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;

        if (evt.command) {
            foreground = colors_t::Panning;
            draw_glyph(state, x, y,
                metrics.num_x,
                metrics.num_y + (evt.param >> 4) * grid_height,
                metrics.cmd_firstchar_width
            );

            draw_glyph(state, x + metrics.cmd_firstchar_width, y,
                metrics.num_x + 1,
                metrics.num_y + (evt.param & 0x0f) * grid_height,
                metrics.element_widths[ElemParam] - metrics.cmd_firstchar_width
            );
        } else {
            draw_spacer(state, x, y, ElemParam);
        }
    }

    void draw_letter(draw_state &state, int x, int y,
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
    }

    void draw_spacer(draw_state &state, int x, int y, elem_t elem_type) {
        int offset = 0;
        for (size_t i = 0; i < (size_t) elem_type; ++i) {
            offset += state.font_metrics.element_widths[i];
        }
        foreground = colors_t::Normal;


        draw_glyph(state, x, y,
            state.font_metrics.clear_x + offset,
            state.font_metrics.clear_y,
            state.font_metrics.element_widths[elem_type]);
    }

    void tex_coords(GLfloat *arr, int left, int top, int width, int height) {
        const float bitmap_width = 176.0f;
        const float bitmap_height = 274.0f;

        const float texleft = left / bitmap_width;
        const float textop = 1 - top / bitmap_height;

        const float texright = (left + width) / bitmap_width;
        const float texbottom = textop - height / bitmap_height;

        arr[0] = texleft;  arr[1] = textop;
        arr[2] = texright; arr[3] = textop;
        arr[4] = texright; arr[5] = texbottom;
        arr[6] = texleft;  arr[7] = texbottom;

    }

    void draw_glyph(draw_state &state, int x, int y,
                    int glyph_x, int glyph_y, int glyph_width)
    {
        GLfloat font_vertices[8];
        tex_coords(font_vertices, glyph_x, glyph_y,
            glyph_width, state.font_metrics.height);

        const float left = x;
        const float top = y;
        const float right = x + glyph_width;
        const float bottom = y + state.font_metrics.height;

        GLint vertices[] = {
            left,  top,
            right, top,
            right, bottom,
            left,  bottom
        };

        const auto &color = state.colors[foreground].foreground;

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_COORD_ARRAY);
        glColor3f(color.redF(), color.greenF(), color.blueF());

        glVertexPointer(2, GL_INT, 0, vertices);
        glTexCoordPointer(2, GL_FLOAT, 0, font_vertices);
        glDrawArrays(GL_QUADS, 0, 4);
    }
};

}
}
}