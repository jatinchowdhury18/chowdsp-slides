#pragma once

#include <cassert>

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
    return LoadLibrary (path.data());
#else
    return nullptr;
#endif
}

void close_dll (void* dll)
{
    if (dll == nullptr)
        return;
#if CHOWDSP_SLIDES_MACOS || CHOWDSP_SLIDES_LINUX
    auto result = dlclose (dll);
    assert (result == 0);
#elif CHOWDSP_SLIDES_WINDOWS
    FreeLibrary ((HMODULE) dll);
#else
    return;
#endif
}

void* get_dll_function (void* dll, std::string_view function_name)
{
    if (dll == nullptr)
        return nullptr;
#if CHOWDSP_SLIDES_MACOS || CHOWDSP_SLIDES_LINUX
    return dlsym (dll, function_name.data());
#elif CHOWDSP_SLIDES_WINDOWS
    return (void*) GetProcAddress ((HMODULE) dll, function_name.data());
#else
    return nullptr;
#endif
}
} // namespace chowdsp::slides
