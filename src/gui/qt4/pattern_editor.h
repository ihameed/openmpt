#pragma once

#include <QtGui>
#include <QGLWidget>
#include <cstdint>
#include "colors.h"
#include "pattern_bitmap_fonts.h"

#include "keymap.h"
#include "../../legacy_soundlib/Snd_defs.h"
#include "../../tracker/types.h"
#include "../../tracker/modevent.h"

class module_renderer;

namespace modplug {
namespace gui {
namespace qt4 {

struct player_position_t {
    modplug::tracker::orderindex_t   order;
    modplug::tracker::patternindex_t pattern;
    modplug::tracker::rowindex_t     row;

    player_position_t() : order(0), pattern(0),
                          row(modplug::tracker::RowIndexInvalid) { }

    player_position_t(modplug::tracker::orderindex_t order,
                      modplug::tracker::patternindex_t pattern,
                      modplug::tracker::rowindex_t row)
                      : order(order), pattern(pattern), row(row) { }
};

struct editor_position_t {
    uint32_t row;
    uint32_t column;
    elem_t   subcolumn;

    editor_position_t() : row(0), column(0), subcolumn(ElemNote) { };

    editor_position_t prev_row() const {
        auto newpos = *this;
        if (newpos.row > 0) {
            newpos.row--;
        }
        return newpos;
    }

    editor_position_t next_row() const {
        auto newpos = *this;
        newpos.row++;
        return newpos;
    }

    editor_position_t prev_subcol() const {
        auto newpos = *this;
        if (newpos.subcolumn == ElemNote) {
            --newpos.column;
            newpos.subcolumn = ElemMax;
        }
        --newpos.subcolumn;
        return newpos;
    }

    editor_position_t next_subcol() const {
        auto newpos = *this;
        ++newpos.subcolumn;
        if (newpos.subcolumn >= ElemMax) {
            ++newpos.column;
            newpos.subcolumn = ElemNote;
        }
        return newpos;
    }
};

inline bool in_selection(const editor_position_t &start,
                         const editor_position_t &end,
                         const editor_position_t &pos)
{
    auto minrow = min(start.row, end.row);
    auto maxrow = max(start.row, end.row);

    auto mincol = min(start.column, end.column);
    auto maxcol = max(start.column, end.column);

    elem_t minsub = start.column == mincol ? start.subcolumn : end.subcolumn;
    elem_t maxsub = start.column == mincol ? end.subcolumn : start.subcolumn;

    if (minrow <= pos.row && pos.row <= maxrow) {
        if (mincol < pos.column && pos.column < maxcol) {
            return true;
        }
        if (mincol == pos.column && maxcol == pos.column) {
            return minsub <= pos.subcolumn && pos.subcolumn <= maxsub;
        }
        if (mincol == pos.column) {
            return minsub <= pos.subcolumn;
        }
        if (maxcol == pos.column) {
            return pos.subcolumn <= maxsub;
        }
    }

    return false;
}

class pattern_editor : public QGLWidget {
    Q_OBJECT
public:
    pattern_editor(
        module_renderer &renderer,
        const pattern_keymap_t &keymap,
        const pattern_keymap_t &it_keymap,
        const pattern_keymap_t &xm_keymap,
        const colors_t &colors
    );

    void update_colors(const colors_t &colors);
    void update_playback_position(const player_position_t &);

    bool position_from_point(const QPoint &, editor_position_t &);

    void set_selection_start(const QPoint &);
    void set_selection_end(const QPoint &);
    void set_selection(const QPoint &, editor_position_t &);

    void move_to(const editor_position_t &target);
    const editor_position_t &pos() const;

    bool invoke_key(const pattern_keymap_t &, key_t);
    keycontext_t keycontext() const;


    void set_base_octave(uint8_t octave);

    void collapse_selection();
    modplug::tracker::modevent_t *active_event();

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int, int) override;

    virtual void mousePressEvent(QMouseEvent *) override;
    virtual void mouseMoveEvent(QMouseEvent *) override;
    virtual void mouseReleaseEvent(QMouseEvent *) override;

    virtual void keyPressEvent(QKeyEvent *) override;

private:
    bool selection_active;
    bool is_dragging;
    editor_position_t selection_start;
    editor_position_t selection_end;

    player_position_t playback_pos;
    player_position_t active_pos;
    bool follow_playback;

    const pattern_keymap_t &keymap;
    const pattern_keymap_t &it_keymap;
    const pattern_keymap_t &xm_keymap;

    uint8_t base_octave;

    module_renderer &renderer;

    const pattern_font_metrics_t &font_metrics;
    QImage font_bitmap;
    colors_t colors;

    int width;
    int height;

    GLuint font_texture;
    QRect clipping_rect;

public:
    static void move_up(pattern_editor &);
    static void move_down(pattern_editor &);
    static void move_left(pattern_editor &);
    static void move_right(pattern_editor &);

    static void select_up(pattern_editor &);
    static void select_down(pattern_editor &);
    static void select_left(pattern_editor &);
    static void select_right(pattern_editor &);

    static void insert_note(pattern_editor &, uint8_t, uint8_t);

    static void insert_instr(pattern_editor &, uint8_t);

    static void insert_volcmd(pattern_editor &, modplug::tracker::volcmd_t);
    static void insert_volparam(pattern_editor &, uint8_t);

    static void insert_cmd(pattern_editor &, modplug::tracker::cmd_t);
    static void insert_cmdparam(pattern_editor &, uint8_t);
};


}
}
}