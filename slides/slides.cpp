#include <slides_runner.h>
#include <chowdsp_slides.h>
using namespace chowdsp::slides;

DLL_EXPORT Slideshow* make_slides (Run_Opts&)
{
    return new Slideshow {
        "ChowDSP Slides",
        {
            new Slide { {
                .background_image = new Image { "background.jpg" },
            } },
            new Slide { {
                .background_color = 0xff434300,
            } },
            new Slide { {
                .background_color = 0xff00ffff,
            } },
        }
    };
}
