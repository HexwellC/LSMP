#ifndef LSMP_UTILS_SECURE_MEMORY_HPP
#define LSMP_UTILS_SECURE_MEMORY_HPP

#include <cstddef>      // std::size_t, std::ptrdiff_t
#include <cstdint>      // std::uint8_t, std::uint64_t
#include <new>          // placement new
#include <type_traits>  // std::true_type, std::false_type
#include <utility>      // std::forward

namespace lsmp {
    constexpr std::uint8_t memory_garbage_byte = 0x00;
    constexpr std::uint8_t memory_canary_byte = 0xFF;

    namespace _detail {
        /**
         * Prevent specified memory region from being swapped out to disk.
         *
         * \note On some systems size of memory that can "locked" is limited.
         *       This function silently ignores errors.
         *
         * \param ptr        Pointer to first byte of region. Must be aligned to page boundary.
         * \param size_bytes Region size.
         *
         * \sa \ref unlock_memory
         */
        void lock_memory(void* ptr, std::size_t size_bytes) noexcept;

        /**
         * Reverse of \ref lock_memory.
         */
        void unlock_memory(void* ptr, std::size_t size_bytes) noexcept;

        /**
         * Fill specified memory region with some value. Unlike memset it can't
         * optimized out by compiler.
         *
         * \param ptr       Pointer to first byte of region.
         * \param size_byte Region size.
         * \param byte      Value to write, defaults to 0x00.
         */
        void rewrite_memory(void* ptr, std::size_t size_bytes, std::uint8_t byte = 0x00) noexcept;

        /**
         * Create canary page.
         *
         * \param ptr               Pointer to first byte, where canary should be written.
         * \param user_memory_size  Size of memory between leading and trailing canary pages.
         * \param canary_size       Size of canary (memory page size usually).
         * \param byte              Canary page should be filled with this byte.
         *
         * \sa \ref check_canary
         */
        void create_canary(void* ptr, std::size_t user_memory_size, std::size_t canary_size, std::uint8_t byte) noexcept;

        /**
         * Check canary. Instantly terminates the program if canary page corrupted.
         *
         * \param ptr               Pointer to first byte of canary.
         * \param user_memory_size  Size of memory between leading and trailing canary pages.
         * \param canary_size       Size of canary (memory page size usually).
         * \param byte              Canary page should be filled with this byte.
         *
         * \sa \ref create_canary
         */
        void check_canary(void* ptr, std::size_t user_memory_size, std::size_t canary_size, std::uint8_t byte) noexcept;

        /// Align allocated memory to page boundary (warning: pretty slow and creates a lot of mapped regions).
        void* aligned_malloc(std::size_t size) noexcept;

        /// Free memory allocated with \ref aligned_malloc.
        void aligned_free(void* ptr, std::size_t size) noexcept;

        /// Get size of virtual memory page size.
        std::size_t get_page_size() noexcept;

        /// Return padding size required to align unpadded_size area to memory page boundary (if HAVE_ALIGNED_MALLOC defined).
        std::size_t padding_size(std::size_t unpadded_size) noexcept;

        /**
         * malloc-like function that:
         * * Locks heap in RAM (avoid swapping to disk).
         * * Omits memory pages from core dumps.
         * * Puts canary page before each allocation.
         * * Puts canary page after each allocation.
         * * Rewrites memory with zeros before freeing (done by \ref secure_free).
         *
         * Drawbacks:
         * * Significantly slower than standard functions.
         * * Each allocation requires 3 or 4 additional pages
         * * The returned address will not be aligned if the allocation size is not
         *   a multiple of the required alignment.
         *
         * As a side effect, allocated area will be filled with zeros (or
         * other value of memory_garbage_byte), resuling in
         * zero-initialization of POD types.
         *
         * Memory layout will be as following:
         * +-------------+-----------------+---------+-------------+
         * | canary page |   user data     | padding | canary page |
         * +-------------+-----------------+---------+-------------+
         * ^ - VM page - ^ - VM page - ^ - VM page - ^ - VM page - ^
         */
        void* secure_malloc(std::size_t size) noexcept;

        /**
         * Free memory allocated with \ref secure_malloc.
         * Terminates program if canary page changed.
         */
        void secure_free(void* ptr, std::size_t size = 1) noexcept;
    } // namespace _detail

    // Must be multiply of page size, otherwise mprotect will not work.
    const std::size_t memory_canary_size = _detail::get_page_size();

    /**
     * STL allocator that uses \ref _detail::secure_malloc and \ref _detail::secure_free.
     *
     * Satisfies requirements of Allocator concept. For low-level memory
     * management see \ref secure_new and \ref secure_delete.
     */
    template<typename T>
    class secure_allocator {
    public:
        using pointer             = T*;
        using const_pointer       = const T*;
        using void_pointer        = void*;
        using const_void_pointer  = const void*;
        using value_type          = T;
        using size_type           = std::size_t;
        using difference_type     = std::ptrdiff_t;

        using propagate_on_container_copy_assignment = std::false_type;
        using propagate_on_container_move_assignment = std::false_type;
        using propagate_on_container_swap            = std::false_type;
        using always_equal                           = std::true_type;

        template <typename U>
        struct rebind {
            typedef secure_allocator<U> other;
        };

        secure_allocator() noexcept = default;
        secure_allocator(const secure_allocator<T>&) noexcept = default;
        secure_allocator(secure_allocator<T>&&) noexcept = default;

        secure_allocator<T>& operator=(const secure_allocator<T>&) noexcept = default;
        secure_allocator<T>& operator=(secure_allocator<T>&&) noexcept = default;

        pointer allocate(size_type size) const {
            return (pointer)_detail::secure_malloc(size * sizeof(value_type));
        }

        inline pointer allocate(size_type size, const_void_pointer cvptr) const {
            return allocate(size);
        }

        void deallocate(pointer ptr, size_type size) const noexcept {
            _detail::secure_free(ptr, size * sizeof(value_type));
        }

        inline constexpr size_t max_size() const noexcept {
            return std::size_t(-1);
        }

        template<typename... Args>
        inline void construct(pointer ptr, Args&&... args) const {
            new (ptr) T(std::forward<Args>(args)...);
        }

        inline void destroy(pointer ptr) const noexcept {
            ptr->~T();
        }

        inline secure_allocator<T> select_on_container_copy_construction() const noexcept {
            return secure_allocator<T>();
        }
    }; // class secure_allocator

    template<typename T1, typename T2>
    inline constexpr bool operator==(const secure_allocator<T1>&, const secure_allocator<T2>&) noexcept {
        return true;
    }

    template<typename T1, typename T2>
    inline constexpr bool operator!=(const secure_allocator<T1>&, const secure_allocator<T2>&) noexcept {
        return false;
    }

    /**
     * Allocate and construct T in heap using \ref _detial::secure_malloc.
     */
    template<typename T, typename... Args, typename = typename std::enable_if<!std::is_array<T>::value, void>::type>
    inline T* secure_new(Args&&... args) {
        uint8_t* ptr = (uint8_t*)_detail::secure_malloc(sizeof(T));
        if (ptr == nullptr) return nullptr;
        return new (ptr) T(std::forward<Args>(args)...);
    }

    /**
     * Allocate and construct T in heap using \ref _detial::secure_malloc.
     *
     * Overload for C-arrays. Requires size as first argument.
     */
    template<typename T, typename... Args, typename = typename std::enable_if<std::is_array<T>::value, void>::type>
    inline typename std::remove_all_extents<T>::type* secure_new(std::size_t size, Args&&... args) {
        return (typename std::remove_all_extents<T>::type*)_detail::secure_malloc(sizeof(typename std::remove_all_extents<T>::type) * size);
    }

    /**
     * Deconstruct and free T in heap using \ref _detail::secure_free.
     */
    template<typename T,  typename = typename std::enable_if<!std::is_array<T>::value, void>::type>
    inline void secure_delete(T* ptr) noexcept {
        ptr->~T();
        _detail::secure_free(ptr, sizeof(T));
    }

    /**
     * Deconstruct and free T in heap using \ref _detail::secure_free.
     *
     * Overload for C-arrays. Requires size as first argument.
     */
    template<typename T,  typename = typename std::enable_if<std::is_array<T>::value, void>::type>
    inline void secure_delete(typename std::remove_all_extents<T>::type* ptr, std::size_t size) noexcept {
        _detail::secure_free(ptr, sizeof(typename std::remove_all_extents<T>::type) * size);
    }

    template<typename T>
    inline void check_canary(const T* ptr, std::size_t size) noexcept {
        _detail::check_canary((void*)(ptr - memory_canary_size), size + _detail::padding_size(size), memory_canary_size, memory_garbage_byte);
    }
} // namespace lsmp

#endif // LSMP_UTILS_SECURE_MEMORY_HPP
