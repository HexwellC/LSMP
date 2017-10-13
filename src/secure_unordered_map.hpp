#pragma once

#include <unordered_map>
#include "utils/secure_memory.hpp"

namespace lsmp {

    template<typename Key,
             typename T,
             typename Hash = std::hash<Key>,
             typename KeyEqual = std::equal_to<Key>>
    using secure_unordered_map = std::unordered_map<Key, T, Hash, KeyEqual, secure_allocator<std::pair<const Key, T>>>;
} // namespace lsmmp
