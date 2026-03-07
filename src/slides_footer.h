#pragma once

#include "slides_params.h"

namespace chowdsp::slides
{
enum class Header_Footer_Style
{
    NULL_STYLE,
    SLIDE_NUMBER,
    BEAMER,
    PROGRESS,
    PROGRESS_WITH_NUMBER,
};

static Header_Footer_Style gon_footer_style (Gon_Ref gon)
{
    const auto str = gon.String ({});
    if (str == "SLIDE_NUMBER")
        return Header_Footer_Style::SLIDE_NUMBER;
    if (str == "BEAMER")
        return Header_Footer_Style::BEAMER;
    if (str == "PROGRESS")
        return Header_Footer_Style::PROGRESS;
    if (str == "PROGRESS_WITH_NUMBER")
        return Header_Footer_Style::PROGRESS_WITH_NUMBER;
    return Header_Footer_Style::NULL_STYLE;
}

struct Header_Footer_Params
{
    Header_Footer_Style style {};
    Dimension height {};
    visage::Color background_color {};
    visage::Color text_color {};
    visage::Color accent_color { 0xffaaff88 };
    float animation_speed { 0.25f };
    bool animate = true;
};

static void merge_params (Header_Footer_Params& footer, const Default_Params& params)
{
    if (footer.text_color.alpha() == 0.0f)
        footer.text_color = params.text_color;
}

static Header_Footer_Params gon_header_footer_params (Gon_Ref gon, const Default_Params& default_params)
{
    Header_Footer_Params params {
        .style = gon_footer_style (gon["style"]),
        .height = gon_dim (gon["height"]),
        .background_color = gon["background_color"].UInt ({}),
        .text_color = gon["text_color"].UInt ({}),
        .accent_color = gon["accent_color"].UInt (0xffaaff88),
        .animation_speed = (float) gon["animation_speed"].Number (0.25f),
        .animate = gon["animate"].Bool (true),
    };
    merge_params (params, default_params);
    return params;
}

struct Header_Footer : visage::Frame
{
    const Default_Params& default_params {};
    Header_Footer_Params params {};
    const Slide_Metadata* current_slide {};
    size_t previous_slide_idx {};

    visage::Animation<float> animation {
        visage::Animation<float>::kRegularTime,
        visage::Animation<float>::kLinear,
        visage::Animation<float>::kLinear,
    };

    explicit Header_Footer (Gon_Ref gon, const Default_Params& global_params, const Slide_Metadata* slide_metadata)
        : default_params { global_params },
          params { gon_header_footer_params (gon, default_params) },
          current_slide { slide_metadata }
    {
        if (! params.animate)
            animation.setAnimationTime (0);
        else
            animation.setAnimationTime (visage::Animation<float>::kRegularTime / params.animation_speed);
    }

    void draw (visage::Canvas& canvas) override
    {
        using namespace visage::dimension;

        if (params.background_color.alpha() == 0.0f)
            canvas.setColor (current_slide->slide_color);
        else
            canvas.setColor (params.background_color);
        canvas.fill();

        if (params.style == Header_Footer_Style::SLIDE_NUMBER
            || params.style == Header_Footer_Style::PROGRESS_WITH_NUMBER)
        {
            canvas.setColor (params.text_color);
            const auto text_height = 0.5f * height();
            canvas.text (std::to_string (current_slide->slide_idx),
                         visage::Font { text_height,
                                        default_params.font->data,
                                        (int) default_params.font->size },
                         visage::Font::Justification::kRight,
                         1_vw,
                         0_vh,
                         98_vw,
                         100_vh);
        }
        if (params.style == Header_Footer_Style::BEAMER)
        {
            canvas.setColor (params.text_color);
            const auto text_height = 0.5f * height();
            canvas.text (current_slide->slideshow_title + "  |  " + current_slide->slideshow_author,
                         visage::Font { text_height,
                                        default_params.font->data,
                                        (int) default_params.font->size },
                         visage::Font::Justification::kCenter,
                         1_vw,
                         0_vh,
                         98_vw,
                         100_vh);
        }
        if (params.style == Header_Footer_Style::PROGRESS
            || params.style == Header_Footer_Style::PROGRESS_WITH_NUMBER)
        {
            const auto slide_idx = current_slide->slide_idx;
            const auto slides_count = current_slide->slides_count - 1;
            const auto prev_pct = (float) previous_slide_idx / (float) slides_count;
            const auto next_pct = (float) slide_idx / (float) slides_count;
            if (slide_idx > previous_slide_idx)
            {
                animation.setSourceValue(prev_pct);
                animation.setTargetValue(next_pct);
                animation.target (false, true);
                animation.target (true);
            }
            else if (current_slide->slide_idx < previous_slide_idx)
            {
                animation.setTargetValue (prev_pct);
                animation.setSourceValue (next_pct);
                animation.target (true, true);
                animation.target (false);
            }

            const auto progress_pct = (double) animation.update();
            const auto progress_width = visage::Dimension::widthPercent (progress_pct * 100.0);

            const auto top = params.style == Header_Footer_Style::PROGRESS ? 0_vh : 95_vh;
            const auto height = params.style == Header_Footer_Style::PROGRESS ? 100_vh : 5_vh;
            canvas.setColor (params.accent_color);
            if (progress_pct == 1.0)
                canvas.rectangle (0_vw, top, progress_width, height);
            else
                canvas.rightRoundedRectangle (0_vw, top, progress_width, height, 25_vh);

            if (animation.isAnimating())
                redraw();
        }

        previous_slide_idx = current_slide->slide_idx;
    }
};
} // namespace chowdsp::slides
