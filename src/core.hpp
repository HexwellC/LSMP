#ifndef _LSMP_CORE_HPP
#define _LSMP_CORE_HPP
#include <sodium.h>
namespace lsmp {
/**
 * \brief Initializes LSMP library
 *
 * Initializes core library and other libraries like libsodium
 *
 * \returns true on success, false if something gone wrong. Output can be ignored,
 * but safety isn't guaranteed.
 */
bool initialize();
}
#endif // _LSMP_CORE_HPP
