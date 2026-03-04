#pragma once

#include "slides_params.h"

namespace chowdsp::slides
{
struct Content_Frame_Params
{
    Dims dims {};
    float animation_speed { 0.5f };
    bool animate = true;
};

static Content_Frame_Params gon_content_frame_params (Gon_Ref gon)
{
    return Content_Frame_Params {
        .dims = gon_dims (gon["dims"]),
        .animation_speed = (float) gon["animation_speed"].Number (0.5f),
        .animate = gon["animate"].Bool (true),
    };
}

struct Content_Frame : visage::Frame
{
    const Default_Params& default_params;
    Content_Frame_Params frame_params {};
    size_t animation_steps {};
    size_t active_animation_step {};
    visage::Animation<float> animation {
        visage::Animation<float>::kRegularTime,
        visage::Animation<float>::kLinear,
        visage::Animation<float>::kLinear,
    };

    Content_Frame (const Default_Params& def_params, Content_Frame_Params params)
        : default_params { def_params },
          frame_params { params }
    {
        setVisible (false);

        if (! frame_params.animate)
            animation.setAnimationTime (0);
        else
            animation.setAnimationTime (visage::Animation<float>::kRegularTime / frame_params.animation_speed);
        animation.setTargetValue (1.0f);
        animation.target (! frame_params.animate);
    }

    void show()
    {
        if (frame_params.animate)
            animation.target (true);
        setVisible (true);
        redrawAll();
    }

    void hide()
    {
        if (frame_params.animate)
            animation.target (false);
        redrawAll();
    }

    void draw (visage::Canvas& canvas) override
    {
        const auto is_animating = animation.isAnimating();
        animation.update();
        if (is_animating)
            redrawAll();
        else if (animation.value() == 0.0f)
            setVisible (false);
    }

    virtual float fade_alpha() const
    {
        return animation.value();
    }

    virtual bool previous_step()
    {
        return false;
    }

    virtual bool next_step()
    {
        return false;
    }
};
} // namespace chowdsp::slides
