#pragma once

#include "slides_allocator.h"

#if CHOWDSP_SLIDES_WINDOWS
#include <windows.h>
#endif

#if CHOWDSP_SLIDES_WEB
#include <emscripten/emscripten.h>
#endif

namespace chowdsp::slides
{
#if CHOWDSP_SLIDES_WEB
EM_JS (void, open_url_js, (const char* u), {
    window.open (UTF8ToString (u), '_blank');
});
#endif

static void launch_url (std::string_view url, Allocator& arena)
{
    // @TODO: arena frame...

#if CHOWDSP_SLIDES_WINDOWS

    ShellExecuteA (nullptr, "open", url.data(), nullptr, nullptr, SW_SHOWNORMAL);

#elif CHOWDSP_SLIDES_MACOS

    auto cmd = arena_format(arena, "open \"{}\"", url);
    system (cmd.data());

#elif CHOWDSP_SLIDES_LINUX

    const auto cmd = arena_format(arena, "xdg-open \"{}\"", url);
    system (cmd.data());

#elif CHOWDSP_SLIDES_WEB

    open_url_js (url.data());

#endif
}
} // namespace chowdsp::slides
