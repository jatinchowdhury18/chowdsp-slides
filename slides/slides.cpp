#include <chowdsp_slides.h>
#include <slides_runner.h>
using namespace chowdsp::slides;

#if CHOWDSP_SLIDES_WINDOWS
#define DLL_EXPORT extern "C" __declspec (dllexport)
#else
#define DLL_EXPORT extern "C"
#endif

DLL_EXPORT Slideshow* make_slides (Run_Opts&)
{
    return new Slideshow {
        "ChowDSP Slides",
        new Default_Params {
            .font = new File { "assets/Lato-Regular.ttf" },
        },
        {
            new Slide { {
                .background_image = new File { "assets/background.jpg" },
                .background_color = 0x33000000,
                .style = Cover,
                .title = {
                    .text = "ChowDSP Slides",
                    .size = 85,
                    .justification = visage::Font::kCenter,
                },
                .text = {
                    {
                        .text = "Jatin Chowdhury",
                        .size = 55,
                        .justification = visage::Font::kCenter,
                        .dims = { 0_vw, 50_vh, 100_vw, 50_vh },
                    },
                },
            } },
            new Slide { {
                .title = {
                    .text = "Slide #1",
                    .size = 55,
                },
                .text = {
                    {
                        .text = R"(
Heading 1
- Bullet point 1
    - Sub-point #1)",
                        .size = 35,
                        .dims = { 2_vw, 10_vh, 96_vw, 90_vh },
                    },
                },
            } },
            new Slide { {
                // .background_color = 0xff00ffff,
            } },
        }
    };
}
