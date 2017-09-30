#include "utils/secure_memory.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>

#ifndef MAP_NOCORE
    #define MAP_NOCORE 0
#endif

#if defined(MAP_ANONYMOUS) && defined(HAVE_MMAP) || defined(HAVE_POSIX_MEMALIGN)
    #define HAVE_ALIGNED_MALLOC
#endif

// We can't use mprotect without aligned malloc.
#if defined(PROT_NONE) && defined(PROT_READ) && defined(PROT_WRITE)
    #define HAVE_MPROTECT
#endif

#ifdef _GNUC_
    #define HAVE_WEAK_SYMBOLS
#endif


void lsmp::_detail::lock_memory(void* ptr, std::size_t size_bytes) noexcept {
#ifdef _POSIX_MEMLOCK_RANGE
    mlock(ptr, size_bytes);
#endif
#if defined(HAVE_MPROTECT) && defined(HAVE_ALIGNED_MALLOC)
    mprotect(ptr, size_bytes, PROT_READ | PROT_WRITE);
#endif
}

void lsmp::_detail::unlock_memory(void* ptr, std::size_t size_bytes) noexcept {
#ifdef _POSIX_MEMLOCK_RANGE
    munlock(ptr, size_bytes);
#endif
}

#ifdef HAVE_WEAK_SYMBOLS
__attribute__((weak))
void __lsmp_dummy_symbol_to_prevent_memset_lto(void* const ptr, std::size_t size) {
    (void)ptr; (void)size;
}
#endif

void lsmp::_detail::rewrite_memory(void* ptr, std::size_t size_bytes, std::uint8_t byte) noexcept {
#if defined(HAVE_MEMSET_S)
    if (ptr > 0U && memset_s(ptr, (rsize_t)size_bytes, byte, (rsize_t)size_bytes) != 0) {
        std::abort();
    }
#elif defined(HAVE_WEAK_SYMBOLS)
    memset(ptr, byte, size);
    __lsmp_dummy_symbol_to_prevent_memset_lto(ptr, size);
#else
    volatile std::uint8_t *volatile vptr = (volatile std::uint8_t *volatile) ptr;
    for (std::size_t i = 0; i < size_bytes; ++i) {
        vptr[i] = byte;
    }
#endif
}

void lsmp::_detail::create_canary(void* ptr, std::size_t user_memory_size, std::size_t canary_size, std::uint8_t byte) noexcept {
    rewrite_memory(ptr, canary_size, byte);
#if defined(HAVE_MPROTECT) && defined(HAVE_ALIGNED_MALLOC)
    mprotect(ptr, canary_size, PROT_NONE);
#endif

    rewrite_memory((std::uint8_t*)ptr + canary_size + user_memory_size, canary_size, byte);
#if defined(HAVE_MPROTECT) && defined(HAVE_ALIGNED_MALLOC)
    mprotect((std::uint8_t*)ptr + user_memory_size, canary_size, PROT_NONE);
#endif
}

void lsmp::_detail::check_canary(void* vptr, std::size_t user_memory_size, std::size_t canary_size, std::uint8_t byte) noexcept {
    std::uint8_t* end_ptr = (std::uint8_t*)vptr + canary_size;
#if defined(HAVE_MPROTECT) && defined(HAVE_ALIGNED_MALLOC)
    // Unlock canary memory before checking.
    mprotect(vptr, canary_size, PROT_READ);
#endif
    for (std::uint8_t* ptr = (std::uint8_t*)vptr; ptr != end_ptr; ++ptr) {
        if (*ptr != byte) {
            std::cerr << "** Memory canary corrupted. **\n";
            std::abort();
        }
    }
#if defined(HAVE_MPROTECT) && defined(HAVE_ALIGNED_MALLOC)
    // Lock it back...
    mprotect(vptr, canary_size, PROT_NONE);
    // Same for trailing canary...
    mprotect((std::uint8_t*)vptr + user_memory_size, canary_size, PROT_READ);
#endif
    end_ptr = (std::uint8_t*)vptr + user_memory_size + canary_size * 2;
    for (std::uint8_t* ptr = (std::uint8_t*)vptr + user_memory_size + canary_size; ptr != end_ptr; ++ptr) {
        if (*ptr != byte) {
            std::cerr << "** Memory canary corrupted. **\n";


            std::abort();
        }
    }
#if defined(HAVE_MPROTECT) && defined(HAVE_ALIGNED_MALLOC)
    mprotect((std::uint8_t*)vptr + user_memory_size, canary_size, PROT_NONE);
#endif
}

void* lsmp::_detail::aligned_malloc(std::size_t size) noexcept {
    static const std::size_t page_size = get_page_size();

    void* ptr = nullptr;

#if defined(MAP_ANONYMOUS) && defined(HAVE_MMAP)
    ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_NOCORE | MAP_LOCKED, -1, 0);
    if (ptr == MAP_FAILED || ptr == nullptr) {
        auto errno_save = errno;
        std::cout << strerror(errno_save) << " (" << errno_save << ")\n";
        ptr = nullptr;
    }
#elif defined(HAVE_POSIX_MEMALIGN)
    if (posix_memalign(&ptr, page_size, size) != 0) {
        ptr = nullptr;
    }
#else // !HAVE_ALIGNED_MALLOC
    ptr = std::malloc(size);
#endif
    return ptr;
}

void lsmp::_detail::aligned_free(void* ptr, std::size_t size) noexcept {
# if defined(MAP_ANON) && defined(HAVE_MMAP)
    munmap(ptr, size);
# elif defined(HAVE_POSIX_MEMALIGN)
    free(ptr);
#endif
}

std::size_t lsmp::_detail::get_page_size() noexcept {
    return sysconf(_SC_PAGESIZE);
}

std::size_t lsmp::_detail::padding_size(std::size_t unpadded_size) noexcept {
#ifdef HAVE_ALIGNED_MALLOC
    const std::size_t page_size = get_page_size();
    if (unpadded_size % page_size == 0) return 0;

    return page_size - unpadded_size % page_size;
#else
    // Padding is useless if we can't use aligned malloc.
    return 0;
#endif
}

void* lsmp::_detail::secure_malloc(std::size_t size) noexcept {
    std::size_t allocation_size = size + memory_canary_size * 2 + padding_size(size);
    std::uint8_t* allocation_ptr = (uint8_t*)aligned_malloc(allocation_size);
    if (allocation_ptr == nullptr) return nullptr;

    std::uint8_t* user_ptr = allocation_ptr + memory_canary_size;
    std::size_t user_size = size;

    lock_memory(allocation_ptr, allocation_size);
    rewrite_memory(allocation_ptr, allocation_size, memory_garbage_byte);
    create_canary(allocation_ptr, size + padding_size(size), memory_canary_size, memory_canary_byte);

    return user_ptr;
}

void lsmp::_detail::secure_free(void* ptr, std::size_t size) noexcept {
    std::size_t allocation_size = size + memory_canary_size * 2 + padding_size(size);
    std::uint8_t* allocation_ptr = (std::uint8_t*)ptr - memory_canary_size;

    // Check canary, rewrite protected memory with memory_garbage_byte and then unlock it.
    check_canary(allocation_ptr, size + padding_size(size), memory_canary_size, memory_canary_byte);

#if defined(HAVE_MPROTECT) && defined(HAVE_ALIGNED_MALLOC)
    // Allow write to canary's memory (it's marked read-only by create_canary).
    mprotect(allocation_ptr, allocation_size, PROT_READ | PROT_WRITE);
#endif

    rewrite_memory(allocation_ptr, allocation_size, memory_garbage_byte);
    unlock_memory(allocation_ptr, allocation_size);

    aligned_free(allocation_ptr, allocation_size);
}
