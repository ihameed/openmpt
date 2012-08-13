#pragma once

#include <QtGui>
#include <cstdint>

class module_renderer;


struct pattern_font_spec_t {
    int32_t  width,      height;     // Column Width & Height, including 4-pixels border
    int32_t  clear_x,    clear_y;    // Clear (empty note) location
    int32_t  space_x,    space_y;    // White location (must be big enough)
    uint32_t element_widths[5];      // Elements Sizes
    int32_t  num_x,      num_y;      // Vertically-oriented numbers 0x00-0x0F
    int32_t  num_10_x,   num_10_y;   // Numbers 10-19
    int32_t  alpha_am_x, alpha_am_y; // Letters A-M +#
    int32_t  alpha_nz_x, alpha_nz_y; // Letters N-Z +?
    int32_t  note_x,     note_y;     // Notes ..., C-, C#, ...
    int32_t  note_width;             // NoteWidth
    int32_t  octave_width;           // Octave Width
    int32_t  nVolX,      nVolY;      // Volume Column Effects
    int32_t  vol_width;              // Width of volume effect
    int32_t  vol_firstchar_width;    // Width of first character in volume parameter
    int32_t  cmd_offset;             // XOffset (-xxx) around the command letter
    int32_t  cmd_firstchar_width;
    int32_t  instr_offset;
    int32_t  instr_10_offset;
    int32_t  instr_firstchar_width;
};

static const pattern_font_spec_t medium_pattern_font = {
    92, 13,              // Column Width & Height
    0,   0,              // Clear location
    130, 8,              // Space Location.
    {20, 20, 24, 9, 15}, // Element Widths
    20, 13,              // Numbers 0-F (hex)
    30, 13,              // Numbers 10-19 (dec)
    64, 26,              // A-M#
    78, 26,              // N-Z?
    0,   0,
    12,  8,              // Note & Octave Width
    42, 13,              // Volume Column Effects
    8,   8,
    -1,
    8,    	             // 8+7 = 15
    -3, -1, 12,
};

static const pattern_font_spec_t small_pattern_font = {
    70,  11,             // Column Width & Height
    92,   0,             // Clear location
    130,  8,             // Space Location.
    {16, 14, 18, 7, 11}, // Element Widths
    108, 13,             // Numbers 0-F (hex)
    120, 13,             // Numbers 10-19 (dec)
    142, 26,             // A-M#
    150, 26,             // N-Z?
    92,   0,             // Notes
    10,   6,             // Note & Octave Width
    132, 13,             // Volume Column Effects
    6,    5,
    -1,
    6,                   // 8+7 = 15
    -3, 1, 9,            // InstrOfs + nInstrHiWidth
};

namespace modplug {
namespace gui {
namespace qt4 {

enum sub_column_t {
    NOTE,
    INSTRUMENT,
    VOLUME,
    EFFECT,
    EFFECT_PARAMETER
};

struct pattern_selection_t {
    uint32_t row;
    uint32_t column;
    sub_column_t sub_column;
};

class pattern_editor : public QWidget {
    Q_OBJECT
public:
    pattern_editor(module_renderer &renderer);

protected:
    virtual void paintEvent(QPaintEvent *) override;

private:
    pattern_selection_t selection_start;
    pattern_selection_t selection_end;

    module_renderer &renderer;

    bool font_loaded;
    QPixmap font;
};

}
}
}