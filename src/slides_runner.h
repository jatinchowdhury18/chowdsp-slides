#pragma once

#include "chowdsp_slides.h"
#include "slides_dll.h"

namespace chowdsp::slides
{
struct Run_Opts;
using Slides_Maker = Slideshow* (*) (Run_Opts&);

struct Run_Opts
{
    // hot-reloading stuff
    bool hot_reload {};
    size_t hot_reload_slide_state { 0 };
    void* hot_reload_dll {};

    Slides_Maker slides_maker {};
};

static Slideshow* make_slides_reload (Run_Opts& run_opts)
{
    close_dll (run_opts.hot_reload_dll);
    std::system ("cmake --build ../build --parallel --target slides_reload");

    run_opts.hot_reload_dll = open_dll ("../build/Debug/libslides_reload.dylib");

    auto slide_maker = reinterpret_cast<Slides_Maker> (get_dll_function (run_opts.hot_reload_dll, "make_slides"));
    if (slide_maker == nullptr)
    {
        // @TODO: log error
        assert (false);
        return nullptr;
    }

    return slide_maker (run_opts);
}

void slides_runner (Run_Opts run_opts)
{
    visage::ApplicationWindow window;

    Slideshow* slides {};
    auto load_slides = [&window, &run_opts, slides]() mutable
    {
        if (slides != nullptr)
            run_opts.hot_reload_slide_state = slides->active_slide;
        delete slides;

        slides = run_opts.slides_maker (run_opts);
        if (slides == nullptr)
            return;

        slides->active_slide = run_opts.hot_reload_slide_state;
        window.addChild (slides);
        using namespace visage::dimension;
        slides->layout().setDimensions (100_vw, 100_vh);
        window.setTitle (std::string { slides->name });
    };

    // slides stuff
    if (run_opts.hot_reload)
    {
        run_opts.slides_maker = &make_slides_reload;
        // @TODO: file watcher...
    }
    load_slides();

    // set up window stuff...
    window.onDraw() = [&window] (visage::Canvas& canvas)
    {
        canvas.setColor (0xff33393f);
        canvas.fill (0, 0, window.width(), window.height());
    };
    window.showMaximized();
    window.runEventLoop();

    delete slides;
}
} // namespace chowdsp::slides
