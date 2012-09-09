#pragma once

#include <functional>
#include <unordered_map>

#include "../pervasives/pervasives.h"

namespace modplug {
namespace gui {
namespace qt4 {

class pattern_editor;

struct key_t {
    const Qt::KeyboardModifiers mask;
    const int key;

    key_t(Qt::KeyboardModifiers mask, int key) : mask(mask), key(key) { }

    bool operator == (const key_t &homie) const {
        return homie.mask == mask && homie.key == key;
    }
};

}
}
}


namespace std {

template <>
class hash<modplug::gui::qt4::key_t> {
public:
    size_t operator () (const modplug::gui::qt4::key_t &homie) const {
        size_t h1 = std::hash<int>()(homie.mask);
        size_t h2 = std::hash<int>()(homie.key);
        return h1 ^ (h2 << 1);
    }
};

}

namespace modplug {
namespace gui {
namespace qt4 {

typedef void (*global_action_t)(void);

typedef void (*pattern_action_t)(pattern_editor &);

typedef
    std::unordered_map<std::string, global_action_t>
    global_actionmap_t;

typedef
    std::unordered_map<key_t, std::string>
    global_keymap_t;



typedef
    std::unordered_map<std::string, pattern_action_t>
    pattern_actionmap_t;

typedef
    std::unordered_map<key_t, std::string>
    pattern_keymap_t;


extern global_actionmap_t global_actionmap;
extern pattern_actionmap_t pattern_actionmap;

void init_action_maps();
global_keymap_t default_global_keymap();
pattern_keymap_t default_pattern_keymap();

inline void invoke(const global_keymap_t &km, const key_t &key) {
    auto descr = km.find(key);
    if (descr == km.end()) return;

    auto fun = global_actionmap.find(descr->second);
    if (fun == global_actionmap.end()) return;

    fun->second();
}

inline void invoke(const pattern_keymap_t &km, const key_t &key,
                   pattern_editor &editor)
{
    auto descr = km.find(key);
    if (descr == km.end()) return;

    auto fun = pattern_actionmap.find(descr->second);
    if (fun == pattern_actionmap.end()) return;

    fun->second(editor);
}

}
}
}