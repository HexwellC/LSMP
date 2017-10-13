#pragma once

#include <string>
#include "utils/secure_memory.hpp"

namespace lsmp {
    template<typename CharT, typename Traits = std::char_traits<CharT>>
    using secure_basic_string = std::basic_string<CharT, Traits, secure_allocator<CharT>>;

    using secure_string = secure_basic_string<char>;
} // namespace lsmp
