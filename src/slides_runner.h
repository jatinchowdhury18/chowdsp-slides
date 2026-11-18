#pragma once

#include "chowdsp_slides.h"
#include "slides_dll.h"
#include "slides_pdf.h"
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
    global_diagnostics().clear();
    try
    {
        return new Slideshow { GonObject::Load ("slides.gon"), window, instant_animations };
    }
    catch (const std::exception& e)
    {
        global_diagnostics().warn (std::string { "Failed to load slides: " } + e.what());
    }
    catch (const std::string& e)
    {
        global_diagnostics().warn ("Failed to load slides: " + e);
    }
    catch (...)
    {
        global_diagnostics().warn ("Failed to load slides: unknown error");
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

    // --export-pdf out.pdf renders every slide fully revealed into one PDF, then exits
    std::string_view pdf_path {};

    // Desktop builds open full-screen unless --windowed is passed.
    bool windowed {};
    visage::EventTimer full_screen_timer {};

    // hot-reloading stuff
    bool hot_reload {};
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

static void enter_full_screen ([[maybe_unused]] visage::Window* window)
{
    if (window == nullptr)
        return;

#if CHOWDSP_SLIDES_MACOS
    using namespace choc::objc;
    auto* ns_window = (CHOC_OBJC_CAST_BRIDGED id) window->windowHandle();
    if (ns_window == nullptr)
        return;

    // NSWindowStyleMaskFullScreen: already there (e.g. macOS restored the previous session full-screen).
    constexpr unsigned long style_mask_full_screen = 1ul << 14;
    if ((call<unsigned long> (ns_window, "styleMask") & style_mask_full_screen) != 0)
        return;

    // NSWindowCollectionBehaviorFullScreenPrimary, so the window gets its own full-screen space.
    constexpr unsigned long full_screen_primary = 1ul << 7;
    call<void> (ns_window, "setCollectionBehavior:", call<unsigned long> (ns_window, "collectionBehavior") | full_screen_primary);
    call<void> (ns_window, "toggleFullScreen:", (id) nullptr);
#elif CHOWDSP_SLIDES_WINDOWS
    auto* hwnd = static_cast<HWND> (window->windowHandle());
    if (hwnd == nullptr)
        return;

    // Borderless window covering the whole monitor that the window is currently on.
    MONITORINFO monitor_info { sizeof (MONITORINFO) };
    if (! GetMonitorInfo (MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST), &monitor_info))
        return;

    const auto style = GetWindowLongPtr (hwnd, GWL_STYLE);
    SetWindowLongPtr (hwnd, GWL_STYLE, (style & ~(LONG_PTR) (WS_CAPTION | WS_THICKFRAME)) | WS_POPUP);
    const auto& rect = monitor_info.rcMonitor;
    SetWindowPos (hwnd,
                  HWND_TOP,
                  rect.left,
                  rect.top,
                  rect.right - rect.left,
                  rect.bottom - rect.top,
                  SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
#endif
}

// Moves the deck to the given slide and step, then draws it so that a screenshot shows the result.
static void render_slide_state (visage::ApplicationEditor& editor, Slideshow& slides, size_t slide_idx, size_t steps)
{
    // Rewind the current slide so that it's in a clean state if we come back to it later.
    auto* slide = slides.slides[slides.active_slide];
    if (slides.active_slide != slide_idx || slides.animation_step > steps)
    {
        while (slide->previous_step())
            slides.animation_step--;
        slides.set_state (slide_idx, 0);
        slide = slides.slides[slide_idx];
    }

    while (slides.animation_step < steps && slide->next_step())
        slides.animation_step++;
    slides.update_slide_metadata();

    // Give content that lives outside the canvas (like web views) a chance to capture itself.
    for (auto* frame : slide->params.content)
        if (frame->isVisible())
            frame->prepare_for_render();

    // Animations are instant, but a frame still needs one draw to update and another to show the result.
    for (int i = 0; i < 3; ++i)
        editor.drawWindow();
}

static void render_slides (const Run_Opts& run_opts)
{
    auto* slides = make_slides (nullptr, true);
    if (slides == nullptr)
        return;

    // When exporting a PDF, match the page shape to the deck's aspect ratio (if it has one)
    // so the pages don't get letterboxed.
    auto render_height = run_opts.render_height;
    if (! run_opts.pdf_path.empty() && slides->params.aspect_ratio[0] > 0.0f)
        render_height = (int) std::round ((float) run_opts.render_width * slides->params.aspect_ratio[1] / slides->params.aspect_ratio[0]);

    visage::ApplicationEditor editor;
    editor.addChild (slides);
    editor.setWindowless (run_opts.render_width, render_height);
    slides->setBounds (0.0f, 0.0f, editor.width(), editor.height());
    editor.computeLayout (slides);

    for (const auto& request : run_opts.renders)
    {
        if (request.slide_idx >= slides->slides.size())
        {
            std::cout << "ERROR: slide " << request.slide_idx << " is out of range\n";
            continue;
        }

        render_slide_state (editor, *slides, request.slide_idx, request.steps);
        editor.takeScreenshot().save (std::string { request.path });
        std::cout << "Rendered slide " << request.slide_idx << " step " << slides->animation_step
                  << " to " << request.path << '\n';
    }

    if (! run_opts.pdf_path.empty())
    {
        Pdf_Writer pdf { run_opts.pdf_path, slides->slides.size(), editor.nativeWidth(), editor.nativeHeight() };
        if (! pdf.is_open())
        {
            std::cout << "ERROR: could not open " << run_opts.pdf_path << " for writing\n";
        }
        else
        {
            // Each page shows its slide with every animation step revealed.
            for (size_t slide_idx = 0; slide_idx < slides->slides.size(); ++slide_idx)
            {
                render_slide_state (editor, *slides, slide_idx, SIZE_MAX);
                const auto& screenshot = editor.takeScreenshot();
                if (! pdf.add_page (screenshot.data(), screenshot.width(), screenshot.height()))
                {
                    std::cout << "ERROR: failed to add slide " << slide_idx << " to the PDF\n";
                    break;
                }
            }

            if (pdf.finish())
                std::cout << "Exported " << slides->slides.size() << " slides to " << run_opts.pdf_path << '\n';
            else
                std::cout << "ERROR: failed while writing " << run_opts.pdf_path << '\n';
        }
    }

    editor.removeChild (slides);
    delete slides;
}

void slides_runner (Run_Opts run_opts)
{
    if (! run_opts.renders.empty() || ! run_opts.pdf_path.empty())
    {
        render_slides (run_opts);
        return;
    }

    visage::ApplicationWindow window;
    Slideshow* slides {};

    window.onDraw() = [&slides] (visage::Canvas& canvas)
    {
        // canvas.setColor (0xff33393f);
        canvas.setColor (0xff000000);
        canvas.fill (0, 0, canvas.width(), canvas.height());

        auto& diagnostics = global_diagnostics();
        if (diagnostics.messages.empty())
            return;

        const auto pad = 10.0f;
        const auto banner_width = std::min (canvas.width() * 0.6f, 700.0f);
        const auto banner_height = 70.0f;
        const auto x = pad;
        const auto y = canvas.height() - banner_height - pad;

        canvas.setColor (visage::Color { 0xffcc2222 });
        canvas.roundedRectangle (x, y, banner_width, banner_height, 6.0f);

        if (slides != nullptr && slides->params.font != nullptr)
        {
            canvas.setColor (visage::Color { 0xffffffff });
            canvas.text (diagnostics.messages.back(),
                         font (slides->params, 14.0f),
                         visage::Font::kTopLeft,
                         x + pad,
                         y + pad,
                         banner_width - 2.0f * pad,
                         banner_height - 2.0f * pad);
        }
    };
    window.showMaximized();

    auto load_slides = [&window, &slides]()
    {
        const Reload_State state = slides != nullptr
                                       ? Reload_State { slides->active_slide, slides->animation_step }
                                       : Reload_State {};

        auto* new_slides = make_slides (window.get_window());
        if (new_slides == nullptr)
        {
            // Keep the previous (working) deck on screen; the diagnostics
            // banner in window.onDraw() surfaces why the reload failed.
            return;
        }

        if (slides != nullptr)
        {
            window.removeChild (slides);
            delete slides;
        }
        slides = new_slides;

        slides->set_state (state.slide_idx, state.animation_step);
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

#if CHOWDSP_SLIDES_NATIVE
    // Going full-screen needs the event loop to be running (at least on macOS), so do it on the first timer tick.
    if (! run_opts.windowed)
    {
        run_opts.full_screen_timer.onTimerCallback() = [&run_opts, &window]
        {
            run_opts.full_screen_timer.stopTimer();
            enter_full_screen (window.get_window());
        };
        run_opts.full_screen_timer.startTimer (1);
    }
#endif

    // window.showMaximized();
    window.runEventLoop();
}
} // namespace chowdsp::slides
