#pragma once

#include <chowdsp_slides.h>
using namespace chowdsp::slides;

static Slideshow make_slides()
{
    return Slideshow {
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

int run_slides()
{
    visage::ApplicationWindow window;

    window.onDraw() = [&window] (visage::Canvas& canvas)
    {
        canvas.setColor (0xff33393f);
        canvas.fill (0, 0, window.width(), window.height());
    };

    auto slides = make_slides();
    window.addChild (&slides);

    using namespace visage::dimension;
    slides.layout().setDimensions (100_vw, 100_vh);

    window.setTitle (std::string { slides.name });
    window.showMaximized();
    window.runEventLoop();
    return 0;
}
