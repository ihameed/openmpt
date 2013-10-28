#pragma once

#include <functional>
#include <memory>
#include <stack>
#include <string>

namespace modplug {
namespace gui {
namespace qt5 {

struct undo_item {
    const std::string descr;
    const std::function<void (void)> undo;
    const std::function<void (void)> redo;
};

const undo_item do_nothing = {
    "the dawn of time",
    [] {},
    [] {},
};

struct undo_stack {
    std::stack<undo_item> undo_stack;
    std::stack<undo_item> redo_stack;
};

inline undo_stack mk_undo_root() {
    undo_stack ret;
    return ret;
}

inline void empty_redo_stack(undo_stack state) {
    std::stack<undo_item> empty;
    std::swap(state.redo_stack, empty);
}

inline void add_undo_node(undo_stack state, undo_item item) {
    state.undo_stack.push(item);
    empty_redo_stack(state);
}

inline void apply_undo(undo_stack state) {
    if (!state.undo_stack.empty()) {
        auto item = state.undo_stack.top();
        item.undo();
        state.redo_stack.push(item);
    }
}

inline void apply_redo(undo_stack state) {
    if (!state.redo_stack.empty()) {
        auto item = state.redo_stack.top();
        item.redo();
        state.undo_stack.push(item);
    }
}


}
}
}
