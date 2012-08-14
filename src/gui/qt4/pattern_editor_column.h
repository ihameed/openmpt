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

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt4 {

static const int column_header_height = 20;

struct draw_state {
    const module_renderer &renderer;
    QPainter &painter;
    const QRect &clip;
    const int standard_width;
    const int row_height;
    QImage &font;
    const pattern_font_metrics_t &font_metrics;
    const int pattern_number;
    const modplug::tracker::rowindex_t playback_row;
    const colors_t &colors;

    void set_foreground(const colors_t::colortype_t colortype) {
        font.setColor(1, colors[colortype].foreground.rgba());
    }

    void set_background(const colors_t::colortype_t colortype) {
        font.setColor(0, colors[colortype].background.rgba());
    }
};

struct pattern_column: modplug::pervasives::noncopyable {
    virtual void draw_header(draw_state &, int) = 0;
    virtual void draw_column(draw_state &, int) = 0;
};

struct automation_column: pattern_column {
};

struct wave_column: pattern_column {
};

struct note_column: pattern_column {
    virtual void draw_header(draw_state &state, int column) {
        QRect rect(state.standard_width * column, 0,
                   state.standard_width, column_header_height);
        state.painter.drawRect(rect);
        const char *fmt = state.renderer.m_bChannelMuteTogglePending[column]
            ? "[Channel %1]"
            : "Channel %1";
        state.painter.drawText(rect, Qt::AlignCenter,
                               QString(fmt).arg(column + 1));
    }

    virtual void draw_column(draw_state &state, int column) {
        auto current_pattern = state.renderer.Patterns[state.pattern_number];
        modplug::tracker::rowindex_t nrows = current_pattern ? current_pattern.GetNumRows() : 0;

        const QRect rect(
            column * state.standard_width,
            column_header_height + 1,
            state.standard_width,
            state.font_metrics.height
        );
        QRect cell(rect);

        auto clip_bottom = rect_bottom(state.clip);

        for (size_t row = 0;
             row < nrows && rect_bottom(cell) <= clip_bottom;
             ++row)
        {
            set_background(state, row);
            draw_row(state, current_pattern,
                     row, column,
                     cell.left(), cell.top());
            cell.setTop(cell.top() + state.font_metrics.height);
        }
    }

    void set_background(draw_state &state, size_t row) {
        if (row == state.playback_row) {
            state.set_background(colors_t::PlayCursor);
            return;
        }

        auto current_pattern = state.renderer.Patterns[state.pattern_number];
        auto measure_length  = state.renderer.m_nDefaultRowsPerMeasure;
        auto beat_length     = state.renderer.m_nDefaultRowsPerBeat;

        if (current_pattern.GetOverrideSignature()) {
            measure_length = current_pattern.GetRowsPerMeasure();
            beat_length = current_pattern.GetRowsPerBeat();
        }

        if (row % measure_length == 0) {
            state.set_background(colors_t::HighlightPrimary);
        } else if (row % beat_length == 0) {
            state.set_background(colors_t::HighlightSecondary);
        } else {
            state.set_background(colors_t::Normal);
        }
    }

    void draw_row(draw_state &state, const CPattern &pattern,
                  int row, int column, int x, int y)
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

        draw_param(state, x, y, evt);
        x += widths[elem_param];
    }

    void draw_note(draw_state &state, int x, int y,
                   const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;
        auto note = evt.note;
        auto srcx = metrics.note_x;
        auto srcy = metrics.note_y;
        auto width = metrics.element_widths[elem_note];

        state.set_foreground(colors_t::Note);

        switch (note) {
        case 0:
            draw_spacer(state, x, y, elem_note);
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
        draw_debug(state, x, y, elem_note);
    }

    void draw_instr(draw_state &state, int x, int y,
                    const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;
        auto instr = evt.instr;

        if (instr) {
            auto width = metrics.instr_firstchar_width;

            state.set_foreground(colors_t::Instrument);

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
            draw_spacer(state, x, y, elem_instr);
        }
        draw_debug(state, x, y, elem_instr);
    }

    void draw_vol(draw_state &state, int x, int y,
                  const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;

        if (evt.volcmd) {
            auto volcmd = evt.volcmd & 0x0f;
            auto vol    = evt.vol & 0x7f;

            state.set_foreground(colors_t::Volume);

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
            draw_spacer(state, x, y, elem_vol);
        }
        draw_debug(state, x, y, elem_vol);
    }

    void draw_cmd(draw_state &state, int x, int y,
                  const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;

        if (evt.command) {
            state.set_foreground(colors_t::Pitch);
            auto rawcommand = evt.command & 0x3f;
            char command = state.renderer.m_nType & (MOD_TYPE_MOD | MOD_TYPE_XM)
                ? mod_command_glyphs[rawcommand]
                : s3m_command_glyphs[rawcommand];

            draw_letter(state, x, y,
                metrics.element_widths[elem_cmd], metrics.cmd_offset,
                command);
        } else {
            draw_spacer(state, x, y, elem_cmd);
        }
        draw_debug(state, x, y, elem_cmd);
    }

    void draw_param(draw_state &state, int x, int y,
                    const modevent_t &evt)
    {
        auto &metrics = state.font_metrics;

        if (evt.command) {
            state.set_foreground(colors_t::Panning);
            draw_glyph(state, x, y,
                metrics.num_x,
                metrics.num_y + (evt.param >> 4) * grid_height,
                metrics.cmd_firstchar_width
            );

            draw_glyph(state, x + metrics.cmd_firstchar_width, y,
                metrics.num_x + 1,
                metrics.num_y + (evt.param & 0x0f) * grid_height,
                metrics.element_widths[elem_param] - metrics.cmd_firstchar_width
            );
        } else {
            draw_spacer(state, x, y, elem_param);
        }
        draw_debug(state, x, y, elem_param);
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
        for (elem_t i = 0; i < elem_type; ++i) {
            offset += state.font_metrics.element_widths[i];
        }
        state.set_foreground(colors_t::Normal);
        draw_glyph(state, x, y,
            state.font_metrics.clear_x + offset,
            state.font_metrics.clear_y,
            state.font_metrics.element_widths[elem_type]);
    }

    void draw_debug(draw_state &state, int x, int y, elem_t elem_type) {
        if (false) {
            state.set_foreground(colors_t::Normal);
            state.painter.drawRect(x, y,
                state.font_metrics.element_widths[elem_type],
                state.font_metrics.height);
        }
    }

    void draw_glyph(draw_state &state, int x, int y,
                    int glyph_x, int glyph_y, int glyph_width)
    {
        state.painter.drawImage(
            x, y, state.font,
            glyph_x, glyph_y, glyph_width,
            state.font_metrics.height
        );
    }
};

}
}
}
