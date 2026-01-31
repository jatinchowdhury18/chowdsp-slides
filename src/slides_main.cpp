#include "slides_runner.h"
#include <slides.cpp>

#if CHOWDSP_SLIDES_WINDOWS
#include <windows.h>
#include <locale>
#include <codecvt>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
std::string to_string (LPCWSTR wide_string)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes (reinterpret_cast<const wchar_t*> (wide_string));
}
#pragma clang diagnostic pop
#else
std::string to_string (char* string)
{
    return std::string { string };
}
#endif

#if CHOWDSP_SLIDES_POSIX
int main (int argc, char* argv[])
#elif CHOWDSP_SLIDES_WINDOWS
int WINAPI WinMain (_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
#endif
{
    chowdsp::slides::Run_Opts run_opts {
        .slides_maker = &make_slides,
    };

#if CHOWDSP_SLIDES_WINDOWS
    int argc;
    auto* argv = CommandLineToArgvW (GetCommandLineW(), &argc);
#endif
    for (int i = 0; i < argc; ++i)
    {
        const auto arg = to_string (argv[i]);
        if (arg == "--reload")
            run_opts.hot_reload = true;
    }

    slides_runner (run_opts);
    return 0;
}
