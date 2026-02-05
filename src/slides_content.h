#pragma once

#include "slides_params.h"

namespace chowdsp::slides
{
struct Content_Frame_Params
{
    Dims dims {};
    Default_Params* default_params {};
    float animation_speed { 0.5f };
    bool animate = true;
};

struct Content_Frame : visage::Frame
{
    Content_Frame_Params frame_params {};
    size_t animation_steps {};
    size_t active_animation_step {};
    visage::Animation<float> animation {
        visage::Animation<float>::kRegularTime,
        visage::Animation<float>::kLinear,
        visage::Animation<float>::kLinear,
    };

    Content_Frame() = default;
    Content_Frame (Content_Frame_Params params)
        : frame_params { params }
    {
        if (! frame_params.animate)
            animation.setAnimationTime (0);
        else
            animation.setAnimationTime (visage::Animation<float>::kRegularTime / frame_params.animation_speed);
        animation.setTargetValue (1.0f);
        animation.target (! frame_params.animate);
    }

    virtual void set_default_params (Default_Params* default_params)
    {
        frame_params.default_params = default_params;
    }

    void show()
    {
        if (frame_params.animate)
            animation.target (true);
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
