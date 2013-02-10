#include <cstdint>
#include <algorithm>

#include "pattern_bitmap_fonts.h"
#include "../../tracker/types.h"
#include "../../tracker/modevent.h"

namespace modplug {
namespace gui {
namespace qt5 {


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
    editor_position_t(uint32_t row, uint32_t column, elem_t subcolumn)
        : row(row), column(column), subcolumn(subcolumn)
    { };
};

struct selection_t {
    editor_position_t start;
    editor_position_t end;
};

struct normalized_selection_t {
    editor_position_t topleft;
    editor_position_t bottomright;
};

inline normalized_selection_t
normalize_selection(const selection_t selection) {
    auto &start = selection.start;
    auto &end   = selection.end;

    auto minrow = std::min(start.row, end.row);
    auto maxrow = std::max(start.row, end.row);

    auto mincol = std::min(start.column, end.column);
    auto maxcol = std::max(start.column, end.column);

    elem_t minsub = start.column == mincol ? start.subcolumn : end.subcolumn;
    elem_t maxsub = start.column == mincol ? end.subcolumn : start.subcolumn;

    normalized_selection_t ret = { editor_position_t(minrow, mincol, minsub),
                                   editor_position_t(maxrow, maxcol, maxsub) };
    return ret;
}

inline bool pos_in_rect(const normalized_selection_t &corners,
                        const editor_position_t &pos)
{
    if (corners.topleft.row <= pos.row && pos.row <= corners.bottomright.row) {
        bool in_left = (pos.column == corners.topleft.column &&
                        pos.subcolumn >= corners.topleft.subcolumn
                       ) || pos.column > corners.topleft.column;

        bool in_right = (pos.column == corners.bottomright.column &&
                         pos.subcolumn <= corners.bottomright.subcolumn
                        ) || pos.column < corners.bottomright.column;

        return in_left && in_right;
    }

    return false;
}


}
}
}