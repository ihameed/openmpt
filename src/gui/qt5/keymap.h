#pragma once

#include <functional>
#include <unordered_map>

#include "../pervasives/pervasives.h"

namespace modplug {
namespace gui {
namespace qt5 {

class pattern_editor;

enum struct keycontext_t {
    Global = 0,
    NoteCol,
    VolCol,
    InstrCol,
    CmdCol,
    ParamCol
};

inline std::string string_of_keycontext(keycontext_t ctx) {
    switch (ctx) {
    case keycontext_t::Global:   return "ContextGlobal";
    case keycontext_t::NoteCol:  return "ContextNoteCol";
    case keycontext_t::VolCol:   return "ContextVolCol";
    case keycontext_t::InstrCol: return "ContextInstrCol";
    case keycontext_t::CmdCol:   return "ContextFxCol";
    case keycontext_t::ParamCol: return "ContextParamCol";
    }
}

struct key_t {
    const Qt::KeyboardModifiers mask;
    const int key;
    const keycontext_t ctx;

    key_t(Qt::KeyboardModifiers mask, int key) :
        mask(mask), key(key), ctx(keycontext_t::Global)
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
class hash<modplug::gui::qt5::key_t> {
public:
    size_t operator () (const modplug::gui::qt5::key_t &homie) const {
        size_t h1 = std::hash<int>()(homie.mask);
        size_t h2 = std::hash<int>()(homie.key);
        size_t h3 = std::hash<int>()(static_cast<int>(homie.ctx));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

}

namespace modplug {
namespace gui {
namespace qt5 {

struct keymap_t {
    std::unordered_map<key_t, std::string> key_to_descr;
    std::unordered_map<std::string, std::vector<key_t>> descr_to_keys;

    inline void insert(const key_t &key, const std::string &descr) {
        key_to_descr[key] = descr;
        descr_to_keys[descr].push_back(key);
    }

    inline const std::string * descr_of(const key_t &key) const {
        auto descr = key_to_descr.find(key);
        return descr == key_to_descr.end() ? nullptr : &descr->second;
    }
};

typedef void (*global_action_t)(void);

typedef void (*pattern_action_t)(pattern_editor &);



typedef
    std::unordered_map<std::string, global_action_t>
    global_actionmap_t;


typedef
    std::unordered_map<std::string, std::function<void(pattern_editor &)> >
    pattern_actionmap_t;


extern template global_actionmap_t;
extern template pattern_actionmap_t;

extern global_actionmap_t global_actionmap;
extern pattern_actionmap_t pattern_actionmap;
extern keymap_t pattern_it_fxmap;
extern keymap_t pattern_xm_fxmap;

void init_action_maps();
keymap_t default_global_keymap();
keymap_t default_pattern_keymap();

template <typename amap_ty>
inline typename amap_ty::mapped_type
action_of_key(const keymap_t &km, const amap_ty &am, const key_t &key) {
    auto descr = km.descr_of(key);
    if (descr == nullptr) return nullptr;

    auto fun = am.find(*descr);
    if (fun == am.end()) return nullptr;

    return fun->second;
}







}
}
}
