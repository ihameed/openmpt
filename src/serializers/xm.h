#pragma once

#include "binaryparse.h"
#include "../tracker/orderlist.h"

namespace modplug {
namespace serializers {
namespace xm {

struct file_header_ty {
    uint16_t version;
    uint32_t size;
    uint16_t orders;
    uint16_t restartpos;
    uint16_t channels;
    uint16_t patterns;
    uint16_t instruments;
    uint16_t flags;
    uint16_t speed;
    uint16_t tempo;
};

struct instrument_header_ty {
    uint32_t instrument_header_size; // size of instrument_header + sample_header
    uint8_t name[22];
    uint8_t type;
    uint16_t samples;
};

struct sample_header_ty {
    uint32_t sample_header_size;
    uint8_t snum[96];
    uint16_t venv[24];
    uint16_t penv[24];
    uint8_t vnum, pnum;
    uint8_t vsustain, vloopstart, vloopend;
    uint8_t psustain, ploopstart, ploopend;
    uint8_t vtype, ptype;
    uint8_t vibtype, vibsweep, vibdepth, vibrate;
    uint16_t volfade;
    uint8_t midienabled;
    uint8_t midichannel;
    uint16_t midiprogram;
    uint16_t pitchwheelrange;
    uint8_t mutecomputer;
    uint8_t reserved1[15];
};

struct sample_info_ty {
    uint32_t length;
    uint32_t loopstart;
    uint32_t looplen;
    uint8_t vol;
    int8_t finetune;
    uint8_t type;
    uint8_t pan;
    int8_t relnote;
    uint8_t res;
    char name[22];
};

struct pattern_header_ty {
    uint32_t length;
    uint8_t packing_type;
    uint16_t row_length;
    uint16_t data_length;
};

struct pattern_ty {
    pattern_header_ty header;
    std::vector<uint8_t> data;
};

struct invalid_data_exception : public std::exception {
    const char* what() const throw () override {
        return "invalid data, broski!";
    };
};

modplug::tracker::orderlist_t
read(modplug::serializers::binaryparse::context &ctx);

}
}
}
