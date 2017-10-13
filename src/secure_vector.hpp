#pragma once

#include <vector>
#include "utils/secure_memory.hpp"

namespace lsmp {
    template<typename T>
    using secure_vector = std::vector<T, secure_allocator<T>>;
} // namespace lsmp
