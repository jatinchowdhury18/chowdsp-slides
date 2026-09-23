#pragma once

#include "slides_js.h"
#include "slides_params.h"

namespace chowdsp::slides
{
struct Equation_Params
{
    struct Equation_Info
    {
        std::string_view equation_string {};
        Dimension height {};
    };

    std::span<Equation_Info> equations {};
    visage::Color background_color { 0xff181B1F };
    visage::Color equation_color { 0xffffffff };
    Dimension padding { height_percent (2.5) };
};

static std::span<Equation_Params::Equation_Info> gon_equations_array (Gon_Ref gon, Allocator& allocator)
{
    auto params = allocator.make_span<Equation_Params::Equation_Info> (gon.size());
    size_t idx = 0;
    for (const auto& g : gon)
    {
        params[idx++] = Equation_Params::Equation_Info {
            .equation_string = g["equation"].StringView ({}),
            .height = gon_dim (g["height"]),
        };
    }
    return params;
}

static Equation_Params gon_equation_params (Gon_Ref gon, Allocator& allocator)
{
    return Equation_Params {
        .equations = gon_equations_array (gon["equations"], allocator),
        // .equation_string = gon["equation"].String ({}),
        .background_color = gon["background_color"].UInt (0xff181B1F),
        .equation_color = gon["equation_color"].Int (0xffffffff),
        .padding = gon_dim (gon["padding"], height_percent (2.5)),
    };
}

struct Equation : Content_Frame
{
    Equation_Params params {};
    struct SVG
    {
        visage::Svg svg {};
        Dimension height {};
        float width_ex {};
        float height_ex {};
        float height_px {};
        visage::Animation<float> animation {
            visage::Animation<float>::kRegularTime,
            visage::Animation<float>::kLinear,
            visage::Animation<float>::kLinear,
        };
    };
    std::vector<SVG> equations {};

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

    // MathJax sizes its SVGs in "ex", e.g. width="12.3ex".
    static float svg_ex_attribute (const std::string& xml, const std::string& name)
    {
        const auto start = xml.find (name + "=\"");
        if (start == std::string::npos)
            return 0.0f;
        return std::strtof (xml.c_str() + start + name.size() + 2, nullptr);
    }

    bool auto_fit() const
    {
        return std::all_of (equations.begin(), equations.end(), [] (const SVG& e)
                            { return e.height.amount == 0.0f; });
    }

    Equation (const Default_Params& def_params, Content_Frame_Params frame_params, Equation_Params params)
        : Content_Frame { def_params, frame_params },
          params { params }
    {
        if (frame_params.animate)
            animation_steps = params.equations.size();

        for (size_t idx = 0; idx < params.equations.size(); ++idx)
        {
            auto svg_string = default_params.js_engine->render_tex (std::string { params.equations[idx].equation_string });
            svg_string = trim_svg_xml (svg_string);

            SVG svg {
                .svg { (const unsigned char*) svg_string.data(), (int) svg_string.size() },
                .height = params.equations[idx].height,
                .width_ex = svg_ex_attribute (svg_string, "width"),
                .height_ex = svg_ex_attribute (svg_string, "height"),
            };

            if (! frame_params.animate || default_params.instant_animations)
                svg.animation.setAnimationTime (0);
            else
                svg.animation.setAnimationTime (visage::Animation<float>::kRegularTime / frame_params.animation_speed);
            svg.animation.setTargetValue (1.0f);
            svg.animation.target (! frame_params.animate);

            equations.emplace_back (svg);
        }
    }

    float gap_px {};

    void resized() override
    {
        const auto pad = compute_dim (params.padding, *default_params.slideshow_frame);
        const auto w = std::round (width() - 2.0f * pad);
        const auto n = float (equations.size());
        const auto h = std::round (height() - pad * (n + 1.0f));
        gap_px = pad;

        if (auto_fit())
        {
            // One scale (pixels per ex) for every equation in the frame: as large as
            // fits the width and the stacked height, capped at 3vh per ex. Leftover
            // height goes into the gaps, up to 7vh each.
            const auto vh = compute_dim (height_percent (1), *default_params.slideshow_frame);
            float total_ex = 0.0f;
            auto scale = 3.0f * vh;
            for (const auto& eqn : equations)
            {
                total_ex += eqn.height_ex;
                if (eqn.width_ex > 0.0f)
                    scale = std::min (scale, w / eqn.width_ex);
            }
            if (total_ex > 0.0f)
                scale = std::min (scale, h / total_ex);
            gap_px = std::clamp ((height() - total_ex * scale) / (n + 1.0f), pad, 7.0f * vh);
            for (auto& eqn : equations)
                eqn.height_px = eqn.height_ex * scale;
        }
        else
        {
            for (auto& eqn : equations)
                eqn.height_px = eqn.height.amount * h;
        }

        for (auto& eqn : equations)
        {
            // Rasterized at scale 1: the SVG path atlas is shared by every slide,
            // and rasterizing at the display's DPI overflows it on longer decks.
            eqn.svg.setDimensions ((int) w, (int) eqn.height_px, 1.0f);
        }
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

        const auto pad = compute_dim (params.padding, *default_params.slideshow_frame);
        auto y_off = gap_px;
        for (auto& eqn : equations)
        {
            eqn.animation.update();
            const auto anim_value = eqn.animation.value();
            if (anim_value > 0.0f)
            {
                canvas.setColor (params.equation_color.withAlpha (alpha * anim_value));
                // Canvas::svg() converts the position to pixels and then SvgDrawable
                // passes it through Canvas::fill(), which converts it again, so undo
                // one of those conversions here.
                canvas.svg (eqn.svg, pad / dpiScale(), y_off / dpiScale());
                redrawAll();
            }

            y_off += eqn.height_px + gap_px;
        }
    }

    bool previous_step() override
    {
        if (active_animation_step == 0)
            return false;

        active_animation_step--;

        if (frame_params.animate)
            equations[active_animation_step].animation.target (false);
        redrawAll();

        return true;
    }

    bool next_step() override
    {
        if (animation_steps == 0 || active_animation_step == animation_steps)
            return false;

        if (frame_params.animate)
            equations[active_animation_step].animation.target (true);
        redrawAll();

        active_animation_step++;
        return true;
    }
};
} // namespace chowdsp::slides
