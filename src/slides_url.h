#pragma once

#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace chowdsp::slides
{
#if defined(__EMSCRIPTEN__)
EM_JS (void, open_url_js, (const char* u), {
    window.open (UTF8ToString (u), '_blank');
});
#endif

static void launch_url (const std::string& url)
{
#if defined(_WIN32)

    ShellExecuteA (nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

#elif defined(__APPLE__)

    std::string cmd = "open \"" + url + "\"";
    system (cmd.c_str());

#elif defined(__linux__)

    std::string cmd = "xdg-open \"" + url + "\"";
    system (cmd.c_str());

#elif defined(__EMSCRIPTEN__)

    open_url_js (url.c_str());

#endif
}
} // namespace chowdsp::slides
