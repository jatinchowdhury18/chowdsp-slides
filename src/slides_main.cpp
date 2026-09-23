#include "slides_runner.h"
#include <charconv>

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

template <typename T>
static bool parse_number (std::string_view str, T& value)
{
    const auto [end, error] = std::from_chars (str.data(), str.data() + str.size(), value);
    return error == std::errc {} && end == str.data() + str.size();
}

static bool parse_render_request (std::string_view spec,
                                  chowdsp::slides::Render_Request& request,
                                  chowdsp::slides::Allocator& allocator)
{
    const auto first = spec.find (':');
    if (first == std::string_view::npos)
        return false;
    const auto second = spec.find (':', first + 1);
    if (second == std::string_view::npos || second + 1 == spec.size())
        return false;

    if (! parse_number (spec.substr (0, first), request.slide_idx))
        return false;

    const auto steps = spec.substr (first + 1, second - first - 1);
    if (steps == "end")
        request.steps = SIZE_MAX;
    else if (! parse_number (steps, request.steps))
        return false;

    request.path = allocator.copy_string (spec.substr (second + 1));
    return true;
}

static bool parse_render_size (std::string_view size, chowdsp::slides::Run_Opts& run_opts)
{
    const auto x = size.find ('x');
    if (x == std::string_view::npos)
        return false;
    return parse_number (size.substr (0, x), run_opts.render_width)
           && parse_number (size.substr (x + 1), run_opts.render_height)
           && run_opts.render_width > 0
           && run_opts.render_height > 0;
}

#if CHOWDSP_SLIDES_POSIX
int main (int argc, char* argv[])
#elif CHOWDSP_SLIDES_WINDOWS
int WINAPI WinMain (_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
#endif
{
    chowdsp::slides::Run_Opts run_opts {};
    chowdsp::slides::Allocator arg_allocator { 1 << 12 };

#if CHOWDSP_SLIDES_WINDOWS
    int argc;
    auto* argv = CommandLineToArgvW (GetCommandLineW(), &argc);
#endif

    size_t render_count = 0;
    for (int i = 0; i < argc; ++i)
        render_count += to_string (argv[i]) == "--render";
    run_opts.renders = arg_allocator.make_span<chowdsp::slides::Render_Request> (render_count);
    render_count = 0;
    for (int i = 0; i < argc; ++i)
    {
        const auto arg = to_string (argv[i]);
        if (arg == "--reload")
            run_opts.hot_reload = true;

        // --render <slide>:<steps|end>:<out.png>, where <slide> is the footer number
        if (arg == "--render" && i + 1 < argc)
        {
            const auto spec = to_string (argv[++i]);
            if (! parse_render_request (spec, run_opts.renders[render_count++], arg_allocator))
            {
                std::cout << "ERROR: bad --render spec \"" << spec << "\", expected <slide>:<steps|end>:<out.png>\n";
                return 1;
            }
        }

        // --render-size <width>x<height>
        if (arg == "--render-size" && i + 1 < argc)
        {
            const auto size = to_string (argv[++i]);
            if (! parse_render_size (size, run_opts))
            {
                std::cout << "ERROR: bad --render-size \"" << size << "\", expected <width>x<height>\n";
                return 1;
            }
        }
    }

    slides_runner (run_opts);
    return 0;
}
