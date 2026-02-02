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
                    .justification = visage::Font::kLeft,
                },
                .content = {
                    new Bullet_List { {
                                          .frame_params = {
                                              .dims = { 2_vw, 10_vh, 47_vw, 88_vh },
                                              .animate = true,
                                          },
                                          .animate = false,
                                      },
                                      {
                                          Bullet_Params {
                                              .text = "Heading",
                                              .font = 40.0f,
                                              .justification = visage::Font::kCenter,
                                              .has_bullet = false,
                                          },
                                          {
                                              .text = "Bullet point #1",
                                          },
                                          {
                                              .text = "Bullet point #2",
                                          },
                                          {
                                              .text = "sub-bullet point #1",
                                              .indent = 1,
                                          },
                                          {
                                              .text = "sub-bullet point #2",
                                              .indent = 1,
                                          },
                                      } },
                    new Audio_Player { {
                        .frame_params = {
                            .dims = { 51_vw, 10_vh, 47_vw, 25_vh },
                        },
                        .file_path = "assets/test.wav",
                    } },
                },
            } },
            new Slide { {
                // .background_color = 0xff00ffff,
            } },
        }
    };
}
