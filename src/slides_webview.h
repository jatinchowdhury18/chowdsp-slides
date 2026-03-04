#pragma once

#include "slides_content.h"

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
};

static Web_View_Params gon_web_view_params (Gon_Ref gon)
{
    return Web_View_Params {
        .url = gon["url"].String ({}),
        .html = gon["html"].String ({}),
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

#if CHOWDSP_SLIDES_MACOS
    id child_window {};
#endif

    Web_View (const Default_Params& def_params, Content_Frame_Params frame_params, Web_View_Params params)
        : Content_Frame { def_params, frame_params },
          web_view_params { params }
    {
        choc::ui::WebView::Options options;
        options.enableDebugMode = false;
        options.enableDebugInspector = false; // Set to true to open dev tools
        options.transparentBackground = true;
        options.webviewIsReady = [this] (choc::ui::WebView& view)
        {
            if (! web_view_params.url.empty())
            {
                view.navigate (web_view_params.url);
            }
            else if (! web_view_params.html.empty())
            {
                view.setHTML (web_view_params.html);
            }
        };

        webview = std::make_unique<choc::ui::WebView> (options);

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

    ~Web_View() override
    {
        if (webview)
        {
#if CHOWDSP_SLIDES_MACOS
            if (auto* view = (CHOC_OBJC_CAST_BRIDGED id) webview->getViewHandle())
                choc::objc::call<void> (view, "removeFromSuperview");

            if (child_window)
            {
                // Remove child window from main window
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

    void resized() override
    {
        if (! webview)
            return;

#if CHOWDSP_SLIDES_MACOS
        const auto pos = positionInWindow();
        const auto scale = 1.0f / dpiScale();
        const auto w = static_cast<int> (std::round ((float) nativeWidth() * scale));
        const auto h = static_cast<int> (std::round ((float) nativeHeight() * scale));
        if (auto* main_window = (CHOC_OBJC_CAST_BRIDGED id) default_params.window->windowHandle())
        {
            choc::objc::CGRect main_bounds = choc::objc::call<choc::objc::CGRect> (main_window,
                                                                                   "contentRectForFrameRect:",
                                                                                   choc::objc::call<choc::objc::CGRect> (main_window, "frame"));

            // Compute child window frame relative to main window
            choc::objc::CGRect child_frame;
            child_frame.origin.x = (int) pos.x;
            child_frame.origin.y = main_bounds.size.height + main_bounds.origin.y - h - (int) pos.y; // flip Y
            child_frame.size.width = w;
            child_frame.size.height = h;

            // Apply new frame
            choc::objc::call<void> (child_window, "setFrame:display:", child_frame, true);

            if (auto* view = (CHOC_OBJC_CAST_BRIDGED id) webview->getViewHandle())
            {
                choc::objc::CGRect web_frame = { { 0, 0 }, { child_frame.size.width,
                                                             child_frame.size.height } };
                choc::objc::call<void> (view, "setFrame:", web_frame);
            }
        }
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

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

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
