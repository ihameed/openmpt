#include "../../tracker/types.h"
#include "../../tracker/modevent.h"

namespace modplug {
namespace gui {
namespace qt4 {


class pattern_editor;

struct pattern_editor_actions {
    static void move_up(pattern_editor &);
    static void move_down(pattern_editor &);
    static void move_left(pattern_editor &);
    static void move_right(pattern_editor &);

    static void move_first_row(pattern_editor &);
    static void move_last_row(pattern_editor &);
    static void move_first_col(pattern_editor &);
    static void move_last_col(pattern_editor &);

    static void select_up(pattern_editor &);
    static void select_down(pattern_editor &);
    static void select_left(pattern_editor &);
    static void select_right(pattern_editor &);

    static void select_first_row(pattern_editor &);
    static void select_last_row(pattern_editor &);
    static void select_first_col(pattern_editor &);
    static void select_last_col(pattern_editor &);

    static void clear_selected_cells(pattern_editor &);
    static void delete_row(pattern_editor &);
    static void insert_row(pattern_editor &);

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