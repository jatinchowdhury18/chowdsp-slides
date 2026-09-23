#pragma once

#include "chowdsp_slides.h"
#include "slides_dll.h"
#include <filesystem>

// If the slides directory contains custom_slides.h, pull it in automatically.
// This is the standard place to define and register custom Content_Frame types.
#if __has_include("custom_slides.h")
#include "custom_slides.h"
#endif

namespace chowdsp::slides
{
static Slideshow* make_slides (visage::Window* window, bool instant_animations = false)
{
    try
    {
        return new Slideshow { GonObject::Load ("slides.gon"), window, instant_animations };
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

struct Render_Request
{
    size_t slide_idx {};
    size_t steps {}; // SIZE_MAX renders the slide fully revealed
    std::string_view path {};
};

struct Run_Opts
{
    // --render slide:step:out.png (repeatable) renders windowlessly, then exits
    std::span<Render_Request> renders {};
    int render_width = 1600;
    int render_height = 900;

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

static void render_slides (const Run_Opts& run_opts)
{
    auto* slides = make_slides (nullptr, true);
    if (slides == nullptr)
        return;

    visage::ApplicationEditor editor;
    editor.addChild (slides);
    editor.setWindowless (run_opts.render_width, run_opts.render_height);
    slides->setBounds (0.0f, 0.0f, editor.width(), editor.height());
    editor.computeLayout (slides);

    for (const auto& request : run_opts.renders)
    {
        if (request.slide_idx >= slides->slides.size())
        {
            std::cout << "ERROR: slide " << request.slide_idx << " is out of range\n";
            continue;
        }

        // Rewind the current slide so that it's in a clean state if we come back to it later.
        auto* slide = slides->slides[slides->active_slide];
        if (slides->active_slide != request.slide_idx || slides->animation_step > request.steps)
        {
            while (slide->previous_step())
                slides->animation_step--;
            slides->set_state (request.slide_idx, 0);
            slide = slides->slides[request.slide_idx];
        }

        while (slides->animation_step < request.steps && slide->next_step())
            slides->animation_step++;
        slides->update_slide_metadata();

        // Animations are instant, but a frame still needs one draw to update and another to show the result.
        for (int i = 0; i < 3; ++i)
            editor.drawWindow();
        editor.takeScreenshot().save (std::string { request.path });
        std::cout << "Rendered slide " << request.slide_idx << " step " << slides->animation_step
                  << " to " << request.path << '\n';
    }

    editor.removeChild (slides);
    delete slides;
}

void slides_runner (Run_Opts run_opts)
{
    if (! run_opts.renders.empty())
    {
        render_slides (run_opts);
        return;
    }

    visage::ApplicationWindow window;
    window.onDraw() = [] (visage::Canvas& canvas)
    {
        // canvas.setColor (0xff33393f);
        canvas.setColor (0xff000000);
        canvas.fill (0, 0, canvas.width(), canvas.height());
    };
    window.showMaximized();

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

        slides = make_slides (window.get_window());
        if (slides == nullptr)
            return;

        slides->set_state (run_opts.hot_reload_state.slide_idx,
                           run_opts.hot_reload_state.animation_step);
        window.addChild (slides);

        window.onResize() = [&window, slides]
        {
            if (slides != nullptr)
            {
                if (slides->params.aspect_ratio[0] > 0.0f)
                {
                    const auto bounds = fit_and_center (window.width(),
                                                        window.height(),
                                                        slides->params.aspect_ratio[0],
                                                        slides->params.aspect_ratio[1]);
                    slides->setBounds (bounds[0], bounds[1], bounds[2], bounds[3]);
                }
                else
                {
                    slides->setBounds (0.0f, 0.0f, window.width(), window.height());
                }
            }
        };

        using namespace visage::dimension;
        window.onResize().callback();
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
    // window.showMaximized();
    window.runEventLoop();
}
} // namespace chowdsp::slides
