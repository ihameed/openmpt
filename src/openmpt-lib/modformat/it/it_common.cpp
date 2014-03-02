#include "stdafx.h"

#include "it.hpp"

#include <string>
#include "../../pervasives/pervasives.hpp"

namespace std {

using namespace modplug::modformat::it;

#define MODPLUG_EXAPND_AS_VOL_MATCH_STRING(tycon) \
    case ( vol_ty::tycon ) : return #tycon;

#define MODPLUG_EXAPND_AS_CMD_MATCH_STRING(tycon) \
    case ( cmd_ty::tycon ) : return #tycon;

std::string
to_string(const vol_ty val) {
    switch (val) {
    MODPLUG_IT_VOL_TAGS(MODPLUG_EXAPND_AS_VOL_MATCH_STRING);
    default: return "???";
    }
}

std::string
to_string(const cmd_ty val) {
    switch (val) {
    MODPLUG_IT_CMD_TAGS(MODPLUG_EXAPND_AS_CMD_MATCH_STRING);
    default: return "???";
    }
}

std::string
to_string(const pattern_entry_ty &val) {
    std::string ret("(");
    ret.append(to_string(val.note))
    .append(", ")
    .append(to_string(val.instr))
    .append(", ")
    .append(to_string(val.volcmd))
    .append(" ")
    .append(to_string(val.volcmd_parameter))
    .append(", ")
    .append(to_string(val.effect_type))
    .append(" ")
    .append(to_string(val.effect_parameter))
    .append(")");
    return ret;
}

std::string
to_string(const pattern_ty &pattern) {
    std::string ret;
    for (const auto &row : pattern) {
        for (const auto &item : row) {
            ret.append(to_string(item));
            ret.append(";");
        }
        ret.append("\n");
    }
    return ret;
}

}
