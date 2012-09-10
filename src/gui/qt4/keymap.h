#pragma once

#include <functional>
#include <unordered_map>

#include "../pervasives/pervasives.h"

namespace modplug {
namespace gui {
namespace qt4 {

class pattern_editor;

enum keycontext_t {
    ContextGlobal = 0,
    ContextNoteCol,
    ContextVolCol,
    ContextInstrCol,
    ContextCmdCol,
    ContextParamCol,

    ContextInvalid
};

inline std::string string_of_keycontext(keycontext_t ctx) {
    switch (ctx) {
    case ContextGlobal:   return "ContextGlobal";
    case ContextNoteCol:  return "ContextNoteCol";
    case ContextVolCol:   return "ContextVolCol";
    case ContextInstrCol: return "ContextInstrCol";
    case ContextCmdCol:   return "ContextFxCol";
    case ContextParamCol: return "ContextParamCol";
    default:              return "ContextInvalid";
    }
}

struct key_t {
    const Qt::KeyboardModifiers mask;
    const int key;
    const keycontext_t ctx;

    key_t(Qt::KeyboardModifiers mask, int key) :
        mask(mask), key(key), ctx(ContextGlobal)
    { }

    key_t(Qt::KeyboardModifiers mask, int key, keycontext_t ctx) :
        mask(mask), key(key), ctx(ctx)
    { }

    bool operator == (const key_t &homie) const {
        return homie.mask == mask && homie.key == key && homie.ctx == ctx;
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
        size_t h3 = std::hash<int>()(homie.ctx);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
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
    std::unordered_map<std::string, std::function<void(pattern_editor &)> >
    pattern_actionmap_t;

typedef
    std::unordered_map<key_t, std::string>
    pattern_keymap_t;

extern template global_actionmap_t;
extern template global_keymap_t;
extern template pattern_actionmap_t;
extern template pattern_keymap_t;

extern global_actionmap_t global_actionmap;
extern pattern_actionmap_t pattern_actionmap;
extern pattern_keymap_t pattern_it_fxmap;
extern pattern_keymap_t pattern_xm_fxmap;

void init_action_maps();
global_keymap_t default_global_keymap();
pattern_keymap_t default_pattern_keymap();

template <typename amap_t, typename kmap_t>
inline typename kmap_t::mapped_type
action_of_key(const amap_t &km, const kmap_t &am, const key_t &key) {
    auto descr = km.find(key);
    if (descr == km.end()) return nullptr;

    auto fun = am.find(descr->second);
    if (fun == am.end()) return nullptr;

    return fun->second;
}

}
}
}