#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include "json.hpp"
#include "utils/secure_memory.hpp"
#include "secure_string.hpp"

namespace lsmp {
    using secure_json = nlohmann::basic_json</* object_t  */ std::map,
                                             /* array_t   */ std::vector,
                                             /* string_t  */ secure_string,
                                             /* boolean_t */ bool,
                                             /* number_integer_t  */ std::int64_t,
                                             /* number_unsigned_t */ std::uint64_t,
                                             /* number_float_t    */ float,
                                             /* allocator         */ secure_allocator>;
} // namespace lsmp

