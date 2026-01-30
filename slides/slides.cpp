#include <slides_runner.h>
#include <chowdsp_slides.h>
using namespace chowdsp::slides;

#if CHOWDSP_SLIDES_WINDOWS
#define DLL_EXPORT extern "C" __declspec(dllexport)
#else
#define DLL_EXPORT extern "C"
#endif

DLL_EXPORT Slideshow* make_slides (Run_Opts&)
{
    std::cout << "dddd44...\n";
    return new Slideshow {
        "ChowDSP Slides",
        {
            new Slide { {
                .background_image = new Image { "background.jpg" },
            } },
            new Slide { {
                .background_color = 0xff00ff00,
            } },
            new Slide { {
                .background_color = 0xff00ffff,
            } },
        }
    };
}
