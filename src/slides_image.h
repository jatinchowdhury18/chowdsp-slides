#pragma once

#include "slides_platform.h"

#include <cassert>
#include <fcntl.h>
#include <sys/stat.h>

#if CHOWDSP_SLIDES_POSIX
#include <sys/mman.h>
#include <unistd.h>
#define USE_MMAP 1
#elif CHOWDSP_SLIDES_WINDOWS
#include <Windows.h>
#include <stdio.h>
#define USE_MMAP 1 // @TODO no MMAP is not implemented!
#endif

namespace chowdsp::slides
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
struct Image
{
    const unsigned char* data {};
    size_t size {};

    Image() = default;

    Image (std::string path)
    {
#if CHOWDSP_SLIDES_POSIX
        int fd = open (path.c_str(), O_RDONLY);
        if (fd == -1)
            assert (false);

        struct stat sb;
        if (fstat (fd, &sb) == -1)
            assert (false);

        size = sb.st_size;

#if USE_MMAP
        data = static_cast<const unsigned char*> (mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0u));
        if (data == MAP_FAILED)
            assert (false);
#else
        auto* fdata = malloc (size);
        const auto bytes_read = read (fd, fdata, size);
        assert (bytes_read == size);
        data = static_cast<const unsigned char*> (fdata);
#endif

        close (fd);
#elif CHOWDSP_SLIDES_WINDOWS
        const auto file = CreateFile (path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

        LARGE_INTEGER lp_file_size;
        GetFileSizeEx (file, &lp_file_size);
        size = static_cast<size_t> (lp_file_size.QuadPart);

        const auto map = CreateFileMapping (file, NULL, PAGE_READONLY, 0, 0, NULL);
        data = static_cast<const unsigned char*> (MapViewOfFile (map, FILE_MAP_READ, 0, 0, 0));

        CloseHandle (map);
        CloseHandle (file);
#endif
    }

    ~Image()
    {
        if (data != nullptr)
        {
#if CHOWDSP_SLIDES_POSIX
#if USE_MMAP
            munmap (const_cast<unsigned char*> (data), size);
#else
            free (const_cast<unsigned char*> (data));
#endif
#elif CHOWDSP_SLIDES_WINDOWS
            UnmapViewOfFile (data);
#endif
        }
    }
};
#pragma clang diagnostic pop
}
