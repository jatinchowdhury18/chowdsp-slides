#pragma once

#include "slides_platform.h"

#include <cassert>
#include <fcntl.h>
#include <sys/stat.h>

#if CHOWDSP_SLIDES_POSIX
#include <sys/mman.h>
#include <unistd.h>
#elif CHOWDSP_SLIDES_WINDOWS
#include <Windows.h>
#include <stdio.h>
#endif

#include "slides_allocator.h"

namespace chowdsp::slides
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
struct File
{
    const unsigned char* data {};
    size_t size {};

    File() = default;

    File (std::string_view path)
    {
#if CHOWDSP_SLIDES_POSIX
        int fd = open (path.data(), O_RDONLY);
        if (fd == -1)
            assert (false);

        struct stat sb;
        if (fstat (fd, &sb) == -1)
            assert (false);

        size = sb.st_size;

        data = static_cast<const unsigned char*> (mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0u));
        if (data == MAP_FAILED)
            assert (false);

        close (fd);
#elif CHOWDSP_SLIDES_WINDOWS
        const auto file = CreateFile (path.data(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

        LARGE_INTEGER lp_file_size;
        GetFileSizeEx (file, &lp_file_size);
        size = static_cast<size_t> (lp_file_size.QuadPart);

        const auto map = CreateFileMapping (file, NULL, PAGE_READONLY, 0, 0, NULL);
        data = static_cast<const unsigned char*> (MapViewOfFile (map, FILE_MAP_READ, 0, 0, 0));

        CloseHandle (map);
        CloseHandle (file);
#endif
    }

    ~File()
    {
        if (data != nullptr)
        {
#if CHOWDSP_SLIDES_POSIX
            munmap (const_cast<unsigned char*> (data), size);
#elif CHOWDSP_SLIDES_WINDOWS
            UnmapViewOfFile (data);
#endif
        }
    }
};
#pragma clang diagnostic pop

using File_Allocator = Lifetime_Allocator<File>;
}
