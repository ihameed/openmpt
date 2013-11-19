#pragma once

#include <exception>

namespace modplug {
namespace modformat {

struct invalid_data_exception : public std::exception {
    const char* what() const throw () override {
        return "invalid data, broski!";
    };
};

}
}
