#include "slides_runner.h"
#include <slides.cpp>

#if CHOWDSP_SLIDES_POSIX
int main (int argc, char* argv[])
{
    chowdsp::slides::Run_Opts run_opts {
        .slides_maker = &make_slides,
    };

    for (int i = 0; i < argc; ++i)
    {
        const auto arg = std::string { argv[i] };
        if (arg == "--reload")
            run_opts.hot_reload = true;
    }

#elif CHOWDSP_SLIDES_WINDOWS
#include <windows.h>
int WINAPI WinMain (_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
#endif

    slides_runner (run_opts);
    return 0;
}
