#pragma once

#include "slides_platform.h"

#include <cassert>
#include <cstdlib>
#include <span>
#include <vector>

#if CHOWDSP_SLIDES_WINDOWS
#include <Windows.h>
#endif

namespace chowdsp::slides
{
struct Allocator
{
    struct Chunk
    {
        Chunk* next {};
        std::byte* data {};
        size_t bytes_used {};
        size_t size {};
    };

    Chunk* head {};
    Chunk* current {};
    size_t chunk_size {};

    Allocator (size_t alloc_chunk_size = 1 << 18)
        : chunk_size { alloc_chunk_size }
    {
        head = create_chunk (chunk_size);
        current = head;
    }

    ~Allocator()
    {
        for (Chunk* chunk = head; chunk != nullptr;)
        {
            auto* chunk_to_delete = chunk;
            chunk = chunk->next;
#if CHOWDSP_SLIDES_WINDOWS
            _aligned_free (chunk_to_delete);
#else
            std::free (chunk_to_delete);
#endif
        }
    }

    void* allocate_bytes (size_t num_bytes, size_t alignment = 8) noexcept
    {
        if (num_bytes > chunk_size)
        {
            assert (false);
            return nullptr; // I think we're handling this correctly, but I want to know when we hit it!
        }

        current->bytes_used = ((current->bytes_used + alignment - 1) / alignment) * alignment;
        if (current->bytes_used + num_bytes > current->size)
        {
            current->next = create_chunk (std::max (chunk_size, num_bytes + alignment));
            current = current->next;
            return allocate_bytes (num_bytes, alignment);
        }

        auto* ptr = current->data + current->bytes_used;
        current->bytes_used += num_bytes;
        return ptr;
    }

    template <typename T, typename IntType>
    T* allocate (IntType num_Ts, size_t alignment = alignof (T)) noexcept
    {
        return static_cast<T*> (allocate_bytes ((size_t) num_Ts * sizeof (T), alignment));
    }

    template <typename T, typename... Args>
    T* allocate_object (Args&&... args) noexcept
    {
        auto* bytes = allocate_bytes (sizeof (T), alignof (T));
        return new (bytes) T { std::forward<Args> (args)... };
    }

    template <typename T, typename I>
    std::span<T> make_span (I size, size_t alignment = alignof (T))
    {
        return { allocate<T> (size, alignment), static_cast<size_t> (size) };
    }

    static Chunk* create_chunk (size_t size)
    {
        static constexpr size_t chunk_alignment = 64;
        const auto bytes_needed = size + sizeof (Chunk);
        const auto bytes_needed_padded = ((bytes_needed + chunk_alignment - 1) / chunk_alignment) * chunk_alignment;

#if CHOWDSP_SLIDES_WINDOWS
        auto* chunk_data = (std::byte*) _aligned_malloc (bytes_needed_padded, chunk_alignment);
#else
        auto* chunk_data = (std::byte*) std::aligned_alloc (chunk_alignment, bytes_needed_padded);
#endif
        auto* chunk = (Chunk*) chunk_data;
        chunk_data += sizeof (Chunk);

        chunk->next = nullptr;
        chunk->data = chunk_data;
        chunk->bytes_used = 0;
        chunk->size = size;
        return chunk;
    }
};

template <typename T>
struct Lifetime_Allocator : Allocator
{
    // TODO: using a vector here is annoying
    // We don't care about pointer stability, so realloc is okay
    // Or we could do more of a "growable list" kind of thing?
    std::vector<T*> lifetime_list {};

    Lifetime_Allocator()
    {
        lifetime_list.reserve (100);
    }

    ~Lifetime_Allocator()
    {
        for (auto* ptr : lifetime_list)
            ptr->~T();
    }

    template <typename C, typename... Args>
    C* allocate (Args&&... args) noexcept
    {
        auto* ptr = Allocator::allocate_object<C> (std::forward<Args> (args)...);

        if constexpr (std::is_base_of_v<T, C>)
            lifetime_list.emplace_back (ptr);

        return ptr;
    }
};
} // namespace chowdsp::slides
