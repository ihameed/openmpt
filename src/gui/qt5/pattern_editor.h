#pragma once

#include <QtGui>
#include <QtWidgets/QtWidgets>
#include <QGLWidget>
#include <cstdint>
#include "colors.h"
#include "pattern_bitmap_fonts.h"
#include "pattern_editor_types.h"

#include "keymap.h"
#include "../../legacy_soundlib/Snd_defs.h"
#include "../../tracker/types.h"
#include "../../tracker/modevent.h"
#include "../../legacy_soundlib/patternContainer.h"

class module_renderer;

namespace modplug {
namespace gui {
namespace qt5 {

class pattern_editor;

class pattern_editor_row_header : public QWidget {
    Q_OBJECT
public:
    pattern_editor_row_header(pattern_editor &);
    QSize sizeHint() const override;

    void paintEvent(QPaintEvent *event) override;
    pattern_editor &parent;
};

class pattern_editor_column_header : public QWidget {
    Q_OBJECT
public:
    pattern_editor_column_header(pattern_editor &);
    QSize sizeHint() const override;

    void paintEvent(QPaintEvent *) override;
    pattern_editor &parent;
};

class pattern_editor_draw : public QGLWidget {
    Q_OBJECT
public:

    pattern_editor_draw(
        module_renderer &renderer,
        const colors_t &colors,
        pattern_editor &parent
    );

    bool position_from_point(const QPoint &, editor_position_t &);

    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int, int) override;

    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

    selection_t selection;
    normalized_selection_t corners;

    player_position_t playback_pos;
    player_position_t active_pos;

    module_renderer &renderer;

    const pattern_font_metrics_t &font_metrics;
    QImage font_bitmap;
    colors_t colors;

    uint32_t first_column;
    int width;
    int height;

    GLuint font_texture;
    QRect clipping_rect;

    bool is_dragging;
    pattern_editor &parent;
};

class pattern_editor : public QAbstractScrollArea {
    Q_OBJECT
public:
    friend class pattern_editor_row_header;
    friend class pattern_editor_column_header;

    pattern_editor(
        module_renderer &renderer,
        const keymap_t &keymap,
        const keymap_t &it_keymap,
        const keymap_t &xm_keymap,
        const colors_t &colors
    );

    void update_colors(const colors_t &colors);
    void update_playback_position(const player_position_t &);
    void update_scrollbars(QSize, QSize);

    void set_base_octave(uint8_t octave);
    uint8_t base_octave();

    bool position_from_point(const QPoint &, editor_position_t &);

    void ensure_selection_end_visible();
    void collapse_selection();
    void set_selection_start(const QPoint &);
    void set_selection_start(const editor_position_t &);
    void set_selection_end(const QPoint &);
    void set_selection_end(const editor_position_t &);
    bool set_pos_from_point(const QPoint &, editor_position_t &);
    void move_to(const editor_position_t &target);

    const normalized_selection_t &selection_corners();
    void recalc_corners();

    editor_position_t pos_move_by_row(const editor_position_t &, int) const;
    editor_position_t pos_move_by_subcol(const editor_position_t &, int) const;

    const editor_position_t &pos() const;

    keycontext_t keycontext() const;
    bool invoke_key(const keymap_t &, key_t);

    const CPattern *active_pattern() const;
    CPattern *active_pattern();

    modplug::tracker::modevent_t *active_event();

    QSize pattern_size();

    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void scrollContentsBy(int, int) override;

public slots:
    void set_active_order(modplug::tracker::orderindex_t);

private:
    pattern_editor_draw draw;
    pattern_editor_row_header row_header;
    pattern_editor_column_header column_header;
    bool is_dragging;

    const keymap_t &keymap;
    const keymap_t &it_keymap;
    const keymap_t &xm_keymap;

    uint8_t _base_octave;
    bool follow_playback;

    QMenu context_menu;

    QAction select_column;
    QAction select_pattern;
    QAction cut;
    QAction copy;
    QAction paste;
    
    QMenu paste_special;

    QAction undo;
    QAction redo;

    QAction clear_selection;

    QAction interpolate_note;
    QAction interpolate_volume;
    QAction interpolate_effect;

    QAction transpose_semiup;
    QAction transpose_semidown;
    QAction transpose_octup;
    QAction transpose_octdown;

    QAction amplify;

    QAction change_instrument;

    QAction grow_selection;
    QAction shrink_selection;

    QAction insert_row;
    QAction delete_row;
};


}
}
}
