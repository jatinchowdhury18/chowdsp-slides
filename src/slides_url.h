#pragma once

#include <cstdlib>
#include <string>

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

static void launch_url (const std::string& url)
{
#if CHOWDSP_SLIDES_WINDOWS

    ShellExecuteA (nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

#elif CHOWDSP_SLIDES_MACOS

    std::string cmd = "open \"" + url + "\"";
    system (cmd.c_str());

#elif CHOWDSP_SLIDES_LINUX

    std::string cmd = "xdg-open \"" + url + "\"";
    system (cmd.c_str());

#elif CHOWDSP_SLIDES_WEB

    open_url_js (url.c_str());

#endif
}
} // namespace chowdsp::slides
