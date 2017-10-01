#include "core.hpp"
namespace lsmp {
bool initialize() {
    bool initialized_successfully = true;
    if (sodium_init() != 0) {
        initialized_successfully = false;
    }
    return initialized_successfully;
}
}
