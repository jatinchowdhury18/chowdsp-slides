#pragma once

#include "chowdsp_slides.h"
#include "slides_dll.h"
#include <filesystem>

namespace chowdsp::slides
{
struct Run_Opts;
using Slides_Maker = Slideshow* (*) (Run_Opts&);

struct Run_Opts
{
    // hot-reloading stuff
    bool hot_reload {};
    size_t hot_reload_slide_state { 0 };
    visage::EventTimer reload_timer {};
    long long last_update_time = visage::time::milliseconds();

    Slides_Maker slides_maker {};
};

#if ALLOW_HOT_RELOAD
static Slideshow* make_slides_reload (Run_Opts& run_opts)
{
    namespace fs = std::filesystem;
    std::cout << "Reloading...\n";

    const auto build_cmd = std::string { "cmake --build " } + BUILD_DIR + " --parallel --config Debug --target " + RELOAD_TARGET;
    auto error_code = std::system (build_cmd.c_str());
    if (error_code != 0)
    {
        // @TODO: log error
        return nullptr;
    }

    const auto original_dll_path = fs::path { DLL_PATH };
    const auto new_dll_filename = original_dll_path.stem().string() + "_" + std::to_string (run_opts.last_update_time);
    const auto new_dll_path = original_dll_path.parent_path().string() + "/" + new_dll_filename + original_dll_path.extension().string();
    const auto copy_cmd = std::string { "mv " } + original_dll_path.string() + " " + new_dll_path;
    error_code = std::system (copy_cmd.c_str());
    assert (error_code == 0);

    auto* hot_reload_dll = open_dll (new_dll_path);
    auto slide_maker = reinterpret_cast<Slides_Maker> (get_dll_function (hot_reload_dll, "make_slides"));
    if (slide_maker == nullptr)
    {
        // @TODO: log error
        assert (false);
        return nullptr;
    }

    auto* slides = slide_maker (run_opts);

    close_dll (hot_reload_dll);
    const auto rm_cmd = std::string { "rm " } + new_dll_path;
    error_code = std::system (rm_cmd.c_str());
    assert (error_code == 0);

    return slides;
}

static bool needs_reload (Run_Opts& run_opts)
{
    namespace fs = std::filesystem;
    fs::path dll_source_path { DLL_SOURCE };
    const auto last_write_time = std::chrono::duration_cast<std::chrono::milliseconds> (
                                     fs::last_write_time (dll_source_path).time_since_epoch())
                                     .count();

    if (last_write_time <= run_opts.last_update_time)
        return false;

    run_opts.last_update_time = last_write_time;
    return true;
}
#else
static Slideshow* make_slides_reload (Run_Opts&) { return nullptr; }
static bool needs_reload (Run_Opts&) { return false; }
#endif

void slides_runner (Run_Opts run_opts)
{
    visage::ApplicationWindow window;
    window.onDraw() = [] (visage::Canvas& canvas)
    {
        canvas.setColor (0xff33393f);
        canvas.fill (0, 0, canvas.width(), canvas.height());
    };

    Slideshow* slides {};
    auto load_slides = [&window, &run_opts, &slides]()
    {
        if (slides != nullptr)
        {
            run_opts.hot_reload_slide_state = slides->active_slide;
            window.removeChild (slides);
            delete slides;
            slides = nullptr;
        }

        slides = run_opts.slides_maker (run_opts);
        if (slides == nullptr)
            return;

        slides->active_slide = run_opts.hot_reload_slide_state;
        window.addChild (slides);

        slides->layout().setDimensions (100_vw, 100_vh);
        window.setTitle (std::string { slides->name });
        window.computeLayout (slides);

        std::cout << "Finished loading slides!\n";
    };

    // slides stuff
    if (run_opts.hot_reload)
    {
        run_opts.slides_maker = &make_slides_reload;
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
