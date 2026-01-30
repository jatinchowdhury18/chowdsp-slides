#pragma once

#include "slides_platform.h"

#if CHOWDSP_SLIDES_MACOS || CHOWDSP_SLIDES_LINUX
#include <dlfcn.h>
#elif CHOWDSP_SLIDES_WINDOWS
#endif

namespace chowdsp::slides
{
void* open_dll (std::string_view path)
{
#if CHOWDSP_SLIDES_MACOS || CHOWDSP_SLIDES_LINUX
    return dlopen (path.data(), RTLD_LOCAL | RTLD_NOW);
#elif CHOWDSP_SLIDES_WINDOWS
#else
    return nullptr;
#endif
}

void close_dll (void* dll)
{
    if (dll == nullptr)
        return;
#if CHOWDSP_SLIDES_MACOS || CHOWDSP_SLIDES_LINUX
    dlclose (dll);
#elif CHOWDSP_SLIDES_WINDOWS
#else
    return nullptr;
#endif
}

void* get_dll_function (void* dll, std::string_view function_name)
{
    if (dll == nullptr)
        return nullptr;
#if CHOWDSP_SLIDES_MACOS || CHOWDSP_SLIDES_LINUX
    return dlsym (dll, function_name.data());
#elif CHOWDSP_SLIDES_WINDOWS
#else
    return nullptr;
#endif
}
} // namespace chowdsp::slides
