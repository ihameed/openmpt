#include "../../tracker/types.h"
#include "../../tracker/modevent.h"

namespace modplug {
namespace gui {
namespace qt5 {


class pattern_editor;

namespace pattern_editor_actions {
void move_up(pattern_editor &);
void move_down(pattern_editor &);
void move_left(pattern_editor &);
void move_right(pattern_editor &);

void move_first_row(pattern_editor &);
void move_last_row(pattern_editor &);
void move_first_col(pattern_editor &);
void move_last_col(pattern_editor &);

void select_up(pattern_editor &);
void select_down(pattern_editor &);
void select_left(pattern_editor &);
void select_right(pattern_editor &);

void select_first_row(pattern_editor &);
void select_last_row(pattern_editor &);
void select_first_col(pattern_editor &);
void select_last_col(pattern_editor &);

void clear_selected_cells(pattern_editor &);
void delete_row(pattern_editor &);
void insert_row(pattern_editor &);

void insert_note(pattern_editor &, uint8_t, uint8_t);
void insert_instr(pattern_editor &, uint8_t);
void insert_volcmd(pattern_editor &, modplug::tracker::volcmd_t);
void insert_volparam(pattern_editor &, uint8_t);
void insert_cmd(pattern_editor &, modplug::tracker::cmd_t);
void insert_cmdparam(pattern_editor &, uint8_t);
}


}
}
}
