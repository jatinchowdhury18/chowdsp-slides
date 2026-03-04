#pragma once

#include "slides_js.h"
#include "slides_params.h"

namespace chowdsp::slides
{
struct Equation_Params
{
    std::string equation_string {};
    visage::Color background_color { 0xff181B1F };
    visage::Color equation_color { 0xffffffff };
    visage::Dimension padding { visage::Dimension::heightPercent (3.5) };
};

static Equation_Params gon_equation_params (Gon_Ref gon)
{
    return Equation_Params {
        .equation_string = gon["equation"].String ({}),
        .background_color = gon["background_color"].UInt (0xff181B1F),
        .equation_color = gon["equation_color"].Int (0xffffffff),
        .padding = gon_dim (gon["padding"], visage::Dimension::heightPercent (2.5)),
    };
}

struct Equation : Content_Frame
{
    Equation_Params params {};
    visage::Svg svg {};

    std::string trim_svg_xml (const std::string& xml)
    {
        auto first_close = xml.find ("<svg");
        if (first_close == std::string::npos)
            return xml;

        auto last_open = xml.rfind ("</svg>");
        if (last_open == std::string::npos || last_open <= first_close)
            return xml;

        return xml.substr (first_close, last_open - first_close + 6);
    }

    Equation (const Default_Params& def_params, Content_Frame_Params frame_params, Equation_Params params)
        : Content_Frame { def_params, frame_params },
          params { params }
    {
        auto svg_string = default_params.js_engine->render_tex (params.equation_string);
        svg_string = trim_svg_xml (svg_string);
        svg = visage::Svg { (const unsigned char*) svg_string.data(), (int) svg_string.size() };
    }

    void resized() override
    {
        const auto pad = compute_dim (params.padding, *default_params.slideshow_frame);
        const auto w = std::round (width() - 2.0f * pad);
        const auto h = std::round (height() - 2.0f * pad);
        svg.setDimensions ((int) w, (int) h, 1.0f);
    }

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

        // background
        canvas.setColor (params.background_color.withAlpha (alpha));
        canvas.roundedRectangle (0, 0, width(), height(), height() * 0.05);

        // "crosshairs" to check centering
        // canvas.setColor (0xffff00ff);
        // canvas.segment (50_vw, 0_vh, 50_vw, 100_vh, 1.0f, false);
        // canvas.segment (0_vw, 50_vh, 100_vw, 50_vh, 1.0f, false);

        canvas.setColor (params.equation_color.withAlpha (alpha));
        const auto pad = compute_dim (params.padding, *default_params.slideshow_frame);
        canvas.svg (svg, pad * 0.5f, pad * 0.5f);
    }
};
} // namespace chowdsp::slides
