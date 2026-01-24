#pragma once

#include "../slides.h"

int main()
{
    visage::ApplicationWindow window;

    window.onDraw() = [&window] (visage::Canvas& canvas)
    {
        canvas.setColor (0xff33393f);
        canvas.fill (0, 0, window.width(), window.height());
    };

    Slideshow slides { {
        new Slide { {
            .background_image = new MMap_Image { "test/background.jpg" },
        } },
        new Slide { {
            .background_color = 0xff434300,
        } },
    } };
    window.addChild (&slides);

    using namespace visage::dimension;
    slides.layout().setDimensions (100_vw, 100_vh);

    // Slide slide {};
    // window.addChild (&slide);
    // using namespace visage::dimension;
    // slide.layout().setDimensions (100_vw, 100_vh);

    window.setTitle ("ChowDSP Slides");
    window.showMaximized();
    window.runEventLoop();
    return 0;
}
