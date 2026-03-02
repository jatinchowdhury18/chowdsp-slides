#pragma once

#include "slides_platform.h"

#if CHOWDSP_SLIDES_NATIVE
#include "third_party/choc/choc/javascript/choc_javascript.h"
#include "third_party/choc/choc/javascript/choc_javascript_QuickJS.h"
#include <embedded/embedded_js.h>
#elif CHOWDSP_SLIDES_WEB
#include <emscripten/emscripten.h>
#include <emscripten/val.h>
#endif

namespace chowdsp::slides
{
#if CHOWDSP_SLIDES_NATIVE

struct JS_Engine
{
    choc::javascript::Context context = choc::javascript::createQuickJSContext();

    JS_Engine()
    {
        try
        {
            const auto mathjax_source = std::string_view {
                (const char*) resources::js::mathjax_embedded_js.data,
                (size_t) resources::js::mathjax_embedded_js.size,
            };
            context.evaluateExpression (std::string { mathjax_source });
        }
        catch (const std::exception& e)
        {
            std::cout << "Error loading MathJax: " << e.what() << '\n';
        }
    }

    std::string render_tex (const std::string& latex)
    {
        try
        {
            auto result = context.invoke ("renderLatexToSVG", latex);
            return result.toString();
        }
        catch (const std::exception& e)
        {
            std::cout << "Error rendering latex: " << e.what() << '\n';
            return {};
        }
    }
};

#elif CHOWDSP_SLIDES_WEB

// clang-format off
EM_JS(void, load_mathjax_file, (), {
    const script = document.createElement("script");
    script.src = "src/mathjax-embedded.js";
    script.onload = () => console.log("MathJax loaded");
    document.head.appendChild(script);
});

EM_ASYNC_JS(void, wait_for_render_func, (), {
    while (typeof globalThis.renderLatexToSVG !== 'function') {
        await new Promise(r => setTimeout(r, 10));
    }
});
// clang-format on

struct JS_Engine
{
    JS_Engine()
    {
        load_mathjax_file();
        wait_for_render_func();
    }

    std::string render_tex (const std::string& latex)
    {
        using emscripten::val;

        auto render_func = val::global ("renderLatexToSVG");
        if (render_func.isUndefined() || render_func.isNull())
        {
            std::cerr << "renderLatexToSVG is not defined!\n";
            return {};
        }

        auto result = render_func (latex);
        return result.as<std::string>();
    }
};
#endif
} // namespace chowdsp::slides
