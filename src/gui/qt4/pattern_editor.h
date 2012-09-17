#pragma once

#include <QtGui>
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
namespace qt4 {

class pattern_editor;

class pattern_editor_row_header : public QWidget {
    Q_OBJECT
public:
    pattern_editor_row_header(pattern_editor &);
    virtual QSize sizeHint() const override;
protected:
    virtual void paintEvent(QPaintEvent *event) override;
private:
    pattern_editor &parent;
};

class pattern_editor_column_header : public QWidget {
    Q_OBJECT
public:
    pattern_editor_column_header(pattern_editor &);
    virtual QSize sizeHint() const override;
protected:
    virtual void paintEvent(QPaintEvent *) override;
private:
    pattern_editor &parent;
};

class pattern_editor_draw : public QGLWidget {
    Q_OBJECT
public:
    friend class pattern_editor;
    friend class pattern_editor_row_header;
    friend class pattern_editor_column_header;

    pattern_editor_draw(
        module_renderer &renderer,
        const colors_t &colors,
        pattern_editor &parent
    );

    bool position_from_point(const QPoint &, editor_position_t &);

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int, int) override;

    virtual void mousePressEvent(QMouseEvent *) override;
    virtual void mouseMoveEvent(QMouseEvent *) override;
    virtual void mouseReleaseEvent(QMouseEvent *) override;

private:
    selection_t selection;
    normalized_selection_t corners;

    player_position_t playback_pos;
    player_position_t active_pos;

    module_renderer &renderer;

    const pattern_font_metrics_t &font_metrics;
    QImage font_bitmap;
    colors_t colors;

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
        const pattern_keymap_t &keymap,
        const pattern_keymap_t &it_keymap,
        const pattern_keymap_t &xm_keymap,
        const colors_t &colors
    );

    void update_colors(const colors_t &colors);
    void update_playback_position(const player_position_t &);

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
    bool invoke_key(const pattern_keymap_t &, key_t);

    const CPattern *active_pattern() const;
    CPattern *active_pattern();

    modplug::tracker::modevent_t *active_event();

    QSize pattern_size();

protected:
    virtual void paintEvent(QPaintEvent *) override;
    virtual void resizeEvent(QResizeEvent *) override;

    virtual void keyPressEvent(QKeyEvent *) override;

    virtual void scrollContentsBy(int, int) override;

private:
    pattern_editor_draw draw;
    bool is_dragging;

    const pattern_keymap_t &keymap;
    const pattern_keymap_t &it_keymap;
    const pattern_keymap_t &xm_keymap;

    uint8_t _base_octave;
    bool follow_playback;
};


}
}
}