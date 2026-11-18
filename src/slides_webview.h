#pragma once

#include <array>
#include <atomic>
#include <memory>

#include "slides_content.h"
#include "slides_image.h"

#if CHOWDSP_SLIDES_NATIVE
#include "third_party/choc/choc/gui/choc_WebView.h"
#elif CHOWDSP_SLIDES_WEB
#include <emscripten/emscripten.h>
#endif

#if CHOWDSP_SLIDES_MACOS
#include "third_party/choc/choc/platform/choc_ObjectiveCHelpers.h"
#endif

namespace chowdsp::slides
{
struct Web_View_Params
{
    std::string url {};
    std::string html {};
    File* html_file {};

    // Origin to load `html` as. Without one the page has a null origin and sends no Referer,
    // which some embeds (like YouTube) refuse to work with.
    std::string html_base_url {};
};

static Web_View_Params gon_web_view_params (Gon_Ref gon, File_Allocator& file_allocator)
{
    return Web_View_Params {
        .url = gon["url"].String ({}),
        .html = gon["html"].String ({}),
        .html_file = gon_file (gon["html_file"], file_allocator),
    };
}

void get_web_img_params (const std::string& file_path, Web_View_Params& params);

static Web_View_Params gon_web_img_params (Gon_Ref gon)
{
    const auto img_path = gon["url"].String ({});
    Web_View_Params params {};
    get_web_img_params (img_path, params);
    return params;
}

static Web_View_Params gon_youtube_params (Gon_Ref gon)
{
    std::string youtube_html = "<iframe style=\"position:fixed; top:0; left:0; bottom:0; right:0; width:100%; height:100%; border:none; margin:0; padding:0; overflow:hidden; z-index:999999;\" src=\"https://www.youtube.com/embed/";
    youtube_html += gon["url"].String ({});
    youtube_html += "\" title=\"YouTube video player\" frameborder=\"0\" allow=\"accelerometer;autoplay;clipboard-write;encrypted-media;gyroscope;picture-in-picture;web-share\" referrerpolicy=\"strict-origin-when-cross-origin\" allowfullscreen></iframe>";

    return Web_View_Params {
        .html = youtube_html,
        // YouTube's embedded player needs to see a real origin. This is deliberately generic, since it's sent to YouTube.
        .html_base_url = "https://localhost/",
    };
}

#if CHOWDSP_SLIDES_NATIVE
void get_web_img_params (const std::string& file_path, Web_View_Params& params)
{
    namespace fs = std::filesystem;
    params.url = std::string { "file://" } + fs::current_path().string() + "/" + file_path;
}

struct Web_View : Content_Frame
{
    Web_View_Params web_view_params {};
    std::unique_ptr<choc::ui::WebView> webview {};

    // webviewIsReady below fires asynchronously and captures `this`. If a hot
    // reload deletes this Web_View before that callback runs, it would
    // otherwise touch freed memory. This flag lets the callback bail out.
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

#if CHOWDSP_SLIDES_MACOS
    id child_window {};

    // Notification observers that re-position the child window whenever the main window moves or changes
    // size outside of a layout pass (e.g. once the full-screen transition finishes).
    std::array<id, 5> window_observers {};

    // When rendering windowless, the web view is snapshotted once and the result is drawn like an image.
    id snapshot_png {}; // retained NSData holding the encoded snapshot
    visage::ImageAtlas::PackedImage snapshot_image { {} };
#endif
    bool has_snapshot {};

    Web_View (const Default_Params& def_params, Content_Frame_Params frame_params, Web_View_Params params)
        : Content_Frame { def_params, frame_params },
          web_view_params { params }
    {
        // Web views live in their own native window, so there's nothing to attach to when rendering windowless.
        // In that case prepare_for_render() snapshots the page instead (where supported).
        if (default_params.window == nullptr)
            return;

        webview = std::make_unique<choc::ui::WebView> (make_options());

        if (webview != nullptr)
        {
            if (! webview->loadedOK())
            {
                std::cout << "WebView failed to load!\n";
                webview.reset();
                return;
            }

#if CHOWDSP_SLIDES_MACOS
            auto* main_window = (CHOC_OBJC_CAST_BRIDGED id) default_params.window->windowHandle(); // WindowMac’s NSWindow
            // Create a borderless child window
            child_window = choc::objc::call<id> (
                choc::objc::callClass<id> ("NSWindow", "alloc"),
                "initWithContentRect:styleMask:backing:defer:",
                choc::objc::CGRect { { 0, 0 }, { 800, 600 } }, // position relative to mainWindow
                0, // borderless
                2, // NSBackingStoreBuffered
                false);

            // Make it a child window of the main window
            choc::objc::call<void> (main_window, "addChildWindow:ordered:", child_window, 1); // 1 = NSWindowAbove

            // Make it not appear in Dock or app switcher
            choc::objc::call<void> (child_window, "setLevel:", 3); // NSFloatingWindowLevel
            choc::objc::call<void> (child_window, "setReleasedWhenClosed:", false);
            choc::objc::call<void> (child_window, "setOpaque:", false);
            choc::objc::call<void> (
                child_window,
                "setBackgroundColor:",
                choc::objc::callClass<id> ("NSColor", "clearColor"));
            choc::objc::call<void> (child_window, "setAlphaValue:", (choc::objc::CGFloat) 0);

            auto* view = (CHOC_OBJC_CAST_BRIDGED id) webview->getViewHandle();
            choc::objc::call<void> (view, "setWantsLayer:", true);
            choc::objc::call<void> (view, "setOpaque:", false);
            choc::objc::call<void> (view, "setBackgroundColor:", choc::objc::callClass<id> ("NSColor", "clearColor"));

            // Set the WebView as the content view
            choc::objc::call<void> (child_window, "setContentView:", view);

            observe_main_window (main_window);
#elif CHOWDSP_SLIDES_WINDOWS
            // Attach WebView to the Visage window
            auto* window = default_params.window;
            if (auto* parent = static_cast<HWND> (window->nativeHandle()))
            {
                if (auto* child = static_cast<HWND> (webview->getViewHandle()))
                {
                    SetParent (child, parent);
                    SetWindowLongPtr (child, GWL_STYLE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS);
                    SetWindowLongPtr (child, GWL_EXSTYLE, GetWindowLongPtr (child, GWL_EXSTYLE) | WS_EX_LAYERED);
                }
            }
#endif
        }

        visibilityChanged();
    }

    static void set_html (choc::ui::WebView& view, const std::string& html, const std::string& base_url)
    {
#if CHOWDSP_SLIDES_MACOS
        // choc's setHTML() always loads with no base URL, so go through WebKit directly.
        if (! base_url.empty())
        {
            using namespace choc::objc;
            CHOC_AUTORELEASE_BEGIN
            auto* url = callClass<id> ("NSURL", "URLWithString:", getNSString (base_url.c_str()));
            call<id> ((CHOC_OBJC_CAST_BRIDGED id) view.getViewHandle(), "loadHTMLString:baseURL:", getNSString (html.c_str()), url);
            return;
            CHOC_AUTORELEASE_END
        }
#endif
        view.setHTML (html);
    }

    choc::ui::WebView::Options make_options()
    {
        choc::ui::WebView::Options options;
        options.enableDebugMode = false;
        options.enableDebugInspector = false; // Set to true to open dev tools
        options.transparentBackground = true;
        options.webviewIsReady = [this, alive = alive] (choc::ui::WebView& view)
        {
            if (! *alive)
                return;

            if (! web_view_params.url.empty())
            {
                view.navigate (web_view_params.url);
            }
            else if (! web_view_params.html.empty())
            {
                set_html (view, web_view_params.html, web_view_params.html_base_url);
            }
            else if (web_view_params.html_file != nullptr)
            {
                const auto html_source = std::string_view { (const char*) web_view_params.html_file->data,
                                                            web_view_params.html_file->size };
                view.setHTML (std::string { html_source });
            }
        };
        return options;
    }

    ~Web_View() override
    {
        *alive = false;

#if CHOWDSP_SLIDES_MACOS
        if (snapshot_png != nullptr)
            choc::objc::call<void> (snapshot_png, "release");
#endif

        if (webview)
        {
#if CHOWDSP_SLIDES_MACOS
            if (auto* view = (CHOC_OBJC_CAST_BRIDGED id) webview->getViewHandle())
                choc::objc::call<void> (view, "removeFromSuperview");

            auto* notification_center = choc::objc::callClass<id> ("NSNotificationCenter", "defaultCenter");
            for (auto& observer : window_observers)
            {
                if (observer != nullptr)
                    choc::objc::call<void> (notification_center, "removeObserver:", observer);
                observer = nullptr;
            }

            if (child_window)
            {
                // Remove child window from main window (there isn't one when rendering windowless)
                if (default_params.window != nullptr)
                    if (auto* main_window = (CHOC_OBJC_CAST_BRIDGED id) default_params.window->windowHandle())
                        choc::objc::call<void> (main_window, "removeChildWindow:", child_window);

                // Close and release the child window
                choc::objc::call<void> (child_window, "orderOut:", nullptr);
                choc::objc::call<void> (child_window, "release");
                child_window = nullptr;
            }
#elif CHOWDSP_SLIDES_WINDOWS
            if (auto* child = static_cast<HWND> (webview->getViewHandle()))
                DestroyWindow (child);
#endif
        }
    }

#if CHOWDSP_SLIDES_MACOS
    void observe_main_window (id main_window)
    {
        using namespace choc::objc;
        auto* notification_center = callClass<id> ("NSNotificationCenter", "defaultCenter");
        const char* names[] = {
            "NSWindowDidMoveNotification",
            "NSWindowDidResizeNotification",
            "NSWindowDidEnterFullScreenNotification",
            "NSWindowDidExitFullScreenNotification",
            "NSWindowDidChangeScreenNotification",
        };
        static_assert (std::size (names) == std::tuple_size_v<decltype (window_observers)>);

        for (size_t i = 0; i < window_observers.size(); ++i)
        {
            window_observers[i] = call<id> (notification_center,
                                            "addObserverForName:object:queue:usingBlock:",
                                            getNSString (names[i]),
                                            main_window,
                                            (id) nullptr, // deliver synchronously on the posting (main) thread
                                            ^(id) {
                                                update_child_window_frame();
                                            });
        }
    }

    // Places the child window over this frame's area of the main window, in screen coordinates.
    void update_child_window_frame()
    {
        if (! webview || child_window == nullptr || default_params.window == nullptr)
            return;

        using namespace choc::objc;
        using Rect = choc::objc::CGRect;
        auto* main_window = (CHOC_OBJC_CAST_BRIDGED id) default_params.window->windowHandle();
        auto* main_view = (CHOC_OBJC_CAST_BRIDGED id) default_params.window->nativeHandle();
        if (main_window == nullptr || main_view == nullptr)
            return;

        // Go through the view that visage draws into, rather than deriving its position from the window frame.
        // That stays correct in full-screen (where there's no title bar) and on displays that aren't at the origin.
        const auto view_in_window = call<Rect> (main_view, "convertRect:toView:", call<Rect> (main_view, "bounds"), (id) nullptr);
        const auto view_on_screen = call<Rect> (main_window, "convertRectToScreen:", view_in_window);

        const auto pos = positionInWindow();
        const auto scale = 1.0f / dpiScale();
        const auto w = std::round ((float) nativeWidth() * scale);
        const auto h = std::round ((float) nativeHeight() * scale);

        // Screen coordinates have y going up, while visage has y going down.
        Rect child_frame;
        child_frame.origin.x = view_on_screen.origin.x + std::round (pos.x);
        child_frame.origin.y = view_on_screen.origin.y + view_on_screen.size.height - std::round (pos.y) - h;
        child_frame.size.width = w;
        child_frame.size.height = h;
        call<void> (child_window, "setFrame:display:", child_frame, true);

        if (auto* view = (CHOC_OBJC_CAST_BRIDGED id) webview->getViewHandle())
            call<void> (view, "setFrame:", Rect { { 0, 0 }, { child_frame.size.width, child_frame.size.height } });
    }
#endif

    void resized() override
    {
        if (! webview)
            return;

#if CHOWDSP_SLIDES_MACOS
        update_child_window_frame();
#elif CHOWDSP_SLIDES_WINDOWS
        // @TODO: this doesn't work when the window is un-maximized and moved around...
        // @TODO: why is the scaling inconsistent
#ifdef _DEBUG
        const auto scale = 1.0f;
#else
        const auto scale = 1.0f / dpiScale();
#endif
        const auto x = (int) std::round ((float) nativeX() * scale);
        const auto y = (int) std::round ((float) nativeY() * scale);
        const auto w = (int) std::round ((float) nativeWidth() * scale);
        const auto h = (int) std::round ((float) nativeHeight() * scale);
        auto* parent = static_cast<HWND> (default_params.window->nativeHandle());
        auto* hwnd = static_cast<HWND> (webview->getViewHandle());
        if (parent != nullptr && hwnd != nullptr)
        {
            POINT pt { x, y };
            ClientToScreen (parent, &pt);

            SetWindowPos (hwnd,
                          HWND_TOP,
                          pt.x,
                          pt.y,
                          w,
                          h,
                          SWP_NOACTIVATE);
        }
#endif
    }

    void visibilityChanged() override
    {
        if (! webview)
            return;

        const auto is_visible = isVisible();
#if CHOWDSP_SLIDES_MACOS
        choc::objc::call<void> (child_window,
                                is_visible ? "orderFront:" : "orderOut:",
                                nullptr);
#elif CHOWDSP_SLIDES_WINDOWS
        if (auto* hwnd = static_cast<HWND> (webview->getViewHandle()))
            ShowWindow (hwnd, is_visible ? SW_SHOW : SW_HIDE);
#endif
    }

    // Draws what stands in for the web view when rendering windowless: the snapshot if we managed to take one,
    // and otherwise a neutral placeholder (which is all that's supported on platforms without snapshots).
    void draw_for_render (visage::Canvas& canvas, float alpha)
    {
#if CHOWDSP_SLIDES_MACOS
        if (has_snapshot)
        {
            canvas.setColor (visage::Color { 0xffffffff }.withAlpha (alpha));
            const std::lock_guard lock { default_params.image_atlas->mutex };
            canvas.addShape (visage::ImageRefWrapper (canvas.state()->clamp,
                                                      canvas.state()->brush,
                                                      canvas.state()->x,
                                                      canvas.state()->y,
                                                      canvas.pixels (width()),
                                                      canvas.pixels (height()),
                                                      snapshot_image,
                                                      default_params.image_atlas));
            return;
        }
#endif

        canvas.setColor (visage::Color { 0xff454c54 }.withAlpha (alpha));
        canvas.roundedRectangle (0.0f, 0.0f, width(), height(), 6.0f);

        const auto is_remote_url = ! web_view_params.url.empty() && ! web_view_params.url.starts_with ("file://");
        const auto label = is_remote_url ? "Web content: " + web_view_params.url : std::string { "Web content" };
        canvas.setColor (default_params.text_color.withAlpha (alpha * 0.7f));
        canvas.text (label,
                     font (default_params, std::clamp (height() * 0.06f, 12.0f, 28.0f)),
                     visage::Font::kCenter,
                     0.0f,
                     0.0f,
                     width(),
                     height());
    }

#if CHOWDSP_SLIDES_MACOS
    static constexpr double load_timeout_seconds = 20.0;
    static constexpr double settle_seconds = 1.5; // extra time for scripts and late resources after the load finishes
    static constexpr double snapshot_timeout_seconds = 10.0;

    static void run_loop_for (double seconds)
    {
        using namespace choc::objc;
        auto* loop = callClass<id> ("NSRunLoop", "currentRunLoop");
        call<void> (loop, "runUntilDate:", callClass<id> ("NSDate", "dateWithTimeIntervalSinceNow:", seconds));
    }

    void tear_down_snapshot_view()
    {
        using namespace choc::objc;
        if (webview != nullptr)
            if (auto* view = (CHOC_OBJC_CAST_BRIDGED id) webview->getViewHandle())
                call<void> (view, "removeFromSuperview");

        if (child_window != nullptr)
        {
            call<void> (child_window, "orderOut:", nullptr);
            call<void> (child_window, "release");
            child_window = nullptr;
        }
        webview.reset();
    }

    // Loads the page in a hidden window, waits for it, and stores a PNG snapshot. Returns false on any failure.
    bool capture_snapshot()
    {
        using namespace choc::objc;
        CHOC_AUTORELEASE_BEGIN

        const auto width_px = nativeWidth();
        const auto height_px = nativeHeight();
        if (width_px <= 0 || height_px <= 0)
            return false;

        // WKWebView needs an NSApplication and a running run loop, neither of which exist when rendering windowless.
        call<void> (getSharedNSApplication(), "setActivationPolicy:", (long) 1); // NSApplicationActivationPolicyAccessory

        webview = std::make_unique<choc::ui::WebView> (make_options());
        if (! webview->loadedOK())
        {
            webview.reset();
            return false;
        }

        const auto scale = (choc::objc::CGFloat) dpiScale();
        const choc::objc::CGRect view_frame { { 0, 0 }, { (choc::objc::CGFloat) width_px / scale, (choc::objc::CGFloat) height_px / scale } };
        auto* view = (CHOC_OBJC_CAST_BRIDGED id) webview->getViewHandle();

        child_window = call<id> (callClass<id> ("NSWindow", "alloc"),
                                 "initWithContentRect:styleMask:backing:defer:",
                                 view_frame,
                                 (unsigned long) 0, // borderless
                                 (unsigned long) 2, // NSBackingStoreBuffered
                                 false);
        call<void> (child_window, "setReleasedWhenClosed:", false);
        call<void> (child_window, "setOpaque:", false);
        call<void> (child_window, "setBackgroundColor:", callClass<id> ("NSColor", "clearColor"));
        call<void> (child_window, "setAlphaValue:", (choc::objc::CGFloat) 0);
        call<void> (child_window, "setIgnoresMouseEvents:", true);
        call<void> (view, "setFrame:", view_frame);
        call<void> (child_window, "setContentView:", view);
        call<void> (child_window, "orderFront:", nullptr);

        // The page was told to load as soon as the web view was created. Give that a moment to start, then wait
        // for it to finish, and then a little longer for anything that renders after the load event.
        run_loop_for (0.1);
        for (auto waited = 0.1; call<bool> (view, "isLoading") && waited < load_timeout_seconds; waited += 0.05)
            run_loop_for (0.05);
        if (call<bool> (view, "isLoading"))
        {
            std::cout << "WARNING: web view timed out while loading\n";
            tear_down_snapshot_view();
            return false;
        }
        run_loop_for (settle_seconds);

        struct Snapshot_State
        {
            bool done {};
            id image {};
        } state;
        auto* state_ptr = &state;
        call<void> (view,
                    "takeSnapshotWithConfiguration:completionHandler:",
                    (id) nullptr,
                    ^(id image, id)
                    {
                        if (image != nullptr)
                            call<id> (image, "retain");
                        state_ptr->image = image;
                        state_ptr->done = true;
                    });
        for (auto waited = 0.0; ! state.done && waited < snapshot_timeout_seconds; waited += 0.05)
            run_loop_for (0.05);

        tear_down_snapshot_view();
        if (state.image == nullptr)
        {
            std::cout << "WARNING: web view snapshot failed\n";
            return false;
        }

        // Re-encode as PNG, since that's what the image atlas knows how to load.
        auto* tiff = call<id> (state.image, "TIFFRepresentation");
        auto* rep = callClass<id> ("NSBitmapImageRep", "imageRepWithData:", tiff);
        auto* png = rep != nullptr ? call<id> (rep,
                                               "representationUsingType:properties:",
                                               (unsigned long) 4, // NSBitmapImageFileTypePNG
                                               callClass<id> ("NSDictionary", "dictionary"))
                                   : (id) nullptr;
        call<void> (state.image, "release");
        if (png == nullptr)
            return false;

        snapshot_png = call<id> (png, "retain");
        const visage::Image image { call<const unsigned char*> (snapshot_png, "bytes"),
                                    (int) call<unsigned long> (snapshot_png, "length"),
                                    width_px,
                                    height_px };
        const std::lock_guard lock { default_params.image_atlas->mutex };
        snapshot_image = default_params.image_atlas->addImage (image);
        return true;

        CHOC_AUTORELEASE_END
    }

    void prepare_for_render() override
    {
        if (default_params.window != nullptr || has_snapshot)
            return;

        has_snapshot = capture_snapshot();
        if (! has_snapshot)
            std::cout << "WARNING: using a placeholder for a web view that could not be captured\n";
    }
#endif

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

        if (default_params.window == nullptr)
        {
            draw_for_render (canvas, alpha);
            return;
        }

#if CHOWDSP_SLIDES_MACOS
        if (webview != nullptr)
        {
            choc::objc::call<void> (child_window, "setAlphaValue:", (choc::objc::CGFloat) alpha);
        }
        else
#elif CHOWDSP_SLIDES_WINDOWS
        if (webview != nullptr)
        {
            if (auto* hwnd = static_cast<HWND> (webview->getViewHandle()))
            {
                const auto alpha_byte = static_cast<BYTE> (alpha * 255.0f);
                SetLayeredWindowAttributes (hwnd, 0, alpha_byte, LWA_ALPHA);
            }
        }
        else
#endif
        {
            canvas.setColor (visage::Color { 0xffff00ff }.withAlpha (alpha));
            canvas.fill();
        }
    }
};

#elif CHOWDSP_SLIDES_WEB

EM_JS (void, create_webview, (int id, const char* url), {
    let iframe = document.createElement ("iframe");
    iframe.id = "visage-webview-" + id;
    const url_str = UTF8ToString (url);
    iframe.src = url_str;

    iframe.style.position = "absolute";
    iframe.style.border = "none";

    document.body.appendChild (iframe);
});

EM_JS (void, create_webview_html, (int id, const char* html), {
    let iframe = document.createElement ("iframe");
    iframe.id = "visage-webview-" + id;
    const html_str = UTF8ToString (html);
    iframe.srcdoc = html_str;

    iframe.style.position = "absolute";
    iframe.style.border = "none";

    document.body.appendChild (iframe);
});

EM_JS (void, set_webview_bounds, (int id, int x, int y, int w, int h), {
    let iframe = document.getElementById ("visage-webview-" + id);
    if (! iframe)
        return;

    // should we do this instead?
    const canvas = document.querySelector ("canvas");
    const rect = canvas.getBoundingClientRect();
    iframe.style.left = rect.left + x + "px";
    iframe.style.top = rect.top + y + "px";

    iframe.style.width = w + "px";
    iframe.style.height = h + "px";
});

EM_JS (void, show_webview, (int id, int visible), {
    let iframe = document.getElementById ("visage-webview-" + id);
    if (! iframe)
        return;

    iframe.style.display = visible ? "block" : "none";
});

EM_JS (void, destroy_webview, (int id), {
    let iframe = document.getElementById ("visage-webview-" + id);
    if (iframe)
        iframe.remove();
});

EM_JS (void, set_webview_alpha, (int id, float alpha), {
    let iframe = document.getElementById ("visage-webview-" + id);
    if (iframe)
        iframe.style.opacity = alpha;
});

EM_JS (void, focus_canvas, (), {
    const canvas = document.querySelector ("canvas");
    if (canvas)
        canvas.focus();
});

EM_JS (char*, create_blob_url_from_fs, (const char* path), {
    const filePath = UTF8ToString (path);
    const data = FS.readFile (filePath);
    const blob = new Blob ([data],
                           { type: "image/gif" });
    const url = URL.createObjectURL (blob);

    const length = lengthBytesUTF8 (url) + 1;
    const ptr = _malloc (length);
    stringToUTF8 (url, ptr, length);
    return ptr;
});

void get_web_img_params (const std::string& file_path, Web_View_Params& params)
{
    const auto path_tweak = std::string { "/" } + file_path;
    auto url = create_blob_url_from_fs (path_tweak.c_str());

    params.html = "<html><body style='margin:0'>"
                  "<img src='";
    params.html += url;
    params.html += "'>"
                   "</body></html>";

    free (url);
}

struct Web_View : Content_Frame
{
    Web_View_Params web_view_params {};
    int webview_id {};

    Web_View (const Default_Params& def_params, Content_Frame_Params frame_params, Web_View_Params params)
        : Content_Frame { def_params, frame_params },
          web_view_params { params }
    {
        // @TODO: maybe each Content_Frame should have its own ID?
        static int next_id = 1;
        webview_id = next_id++;

        if (! web_view_params.url.empty())
            create_webview (webview_id, web_view_params.url.c_str());
        else if (! web_view_params.html.empty())
            create_webview_html (webview_id, web_view_params.html.c_str());

        visibilityChanged();

        onHierarchyChange() = [this]
        {
            if (auto* parent_frame = parent())
            {
                parent_frame->onMouseDown() += [] (const visage::MouseEvent&)
                {
                    focus_canvas();
                };
            }
        };
    }

    ~Web_View() override
    {
        destroy_webview (webview_id);
    }

    void resized() override
    {
        const auto pos = positionInWindow();
        const auto scale = 1.0f / dpiScale();
        const auto w = static_cast<int> (std::round ((float) nativeWidth() * scale));
        const auto h = static_cast<int> (std::round ((float) nativeHeight() * scale));
        set_webview_bounds (webview_id, (int) pos.x, (int) pos.y, w, h);
    }

    void visibilityChanged() override
    {
        const auto is_visible = isVisible();
        show_webview (webview_id, is_visible ? 1 : 0);
    }

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

        // helpful for debugging...
        // canvas.setColor (visage::Color { 0xffff00ff }.withAlpha (alpha));
        // canvas.fill();

        set_webview_alpha (webview_id, alpha);
    }
};

#endif
} // namespace chowdsp::slides
