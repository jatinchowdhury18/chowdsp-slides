#pragma once

#include "chowdsp_slides.h"
#include "slides_dll.h"
#include <filesystem>

namespace chowdsp::slides
{
static Slideshow* make_slides()
{
    try
    {
        return new Slideshow { GonObject::Load ("slides.gon") };
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what() << '\n';
    }
    return {};
}

struct Reload_State
{
    size_t slide_idx { 0 };
    size_t animation_step { 0 };
};

struct Run_Opts
{
    // hot-reloading stuff
    bool hot_reload {};
    Reload_State hot_reload_state {};
    visage::EventTimer reload_timer {};
    long long last_update_time = visage::time::milliseconds();

    // @TODO: store full slides JSON for diffing/partial updates?
};

#if ALLOW_HOT_RELOAD
static bool needs_reload (Run_Opts& run_opts)
{
    namespace fs = std::filesystem;
    fs::path json_source_path { "slides.gon" };
    const auto last_write_time = std::chrono::duration_cast<std::chrono::milliseconds> (
                                     fs::last_write_time (json_source_path).time_since_epoch())
                                     .count();

    if (last_write_time <= run_opts.last_update_time)
        return false;

    run_opts.last_update_time = last_write_time;
    return true;
}
#else
static bool needs_reload (Run_Opts&) { return false; }
#endif

void slides_runner (Run_Opts run_opts)
{
    visage::ApplicationWindow window;
    window.onDraw() = [] (visage::Canvas& canvas)
    {
        // canvas.setColor (0xff33393f);
        canvas.setColor (0xff000000);
        canvas.fill (0, 0, canvas.width(), canvas.height());
    };

    Slideshow* slides {};
    auto load_slides = [&window, &run_opts, &slides]()
    {
        if (slides != nullptr)
        {
            run_opts.hot_reload_state = { slides->active_slide, slides->animation_step };
            window.removeChild (slides);
            delete slides;
            slides = nullptr;
        }

        slides = make_slides();
        if (slides == nullptr)
            return;

        slides->set_state (run_opts.hot_reload_state.slide_idx,
                           run_opts.hot_reload_state.animation_step);
        window.addChild (slides);

        // @TODO: fixed aspect ratio here?
        window.onResize() = [&window, slides]
        {
            if (slides != nullptr)
            {
                if (slides->params->aspect_ratio[0] > 0.0f)
                {
                    const auto bounds = fit_and_center (window.width(), window.height(), 16.0f, 9.0f);
                    slides->setBounds (bounds[0], bounds[1], bounds[2], bounds[3]);
                }
                else
                {
                    slides->setBounds (0.0f, 0.0f, window.width(), window.height());
                }
            }
        };
        slides->layout().setDimensions (100_vw, 100_vh);
        window.setTitle (std::string { slides->slide_metadata.slideshow_title });
        window.computeLayout (slides);

        std::cout << "Finished loading slides!\n";
    };

    // slides stuff
    if (run_opts.hot_reload)
    {
        run_opts.reload_timer.onTimerCallback() = [&run_opts, load_slides]()
        {
            if (needs_reload (run_opts))
                load_slides();
        };
        run_opts.reload_timer.startTimer (100);
    }
    load_slides();

    window.onCloseRequested() = [&]
    {
        delete slides;
        return true;
    };
    window.showMaximized();
    window.runEventLoop();
}
} // namespace chowdsp::slides
