#pragma once

#include <iostream>
#include <visage/app.h>

#include "slides_image.h"

namespace chowdsp::slides
{
struct Slide_Params
{
    Image* background_image {};
    visage::Color background_color {};
};

struct Slide : visage::Frame
{
    Slide_Params params {};

    Slide (Slide_Params slide_params) : params { slide_params }
    {
    }

    ~Slide()
    {
        if (params.background_image != nullptr)
            delete params.background_image;
    }

    void resized() override
    {
        redrawAll();
    }

    void draw (visage::Canvas& canvas) override
    {
        canvas.setColor (params.background_color);
        canvas.fill (0, 0, width(), height());

        if (params.background_image != nullptr)
        {
            canvas.setColor (0xffffffff);
            canvas.image (params.background_image->data, params.background_image->size, 0, 0, width(), height());
        }
    }

    bool keyPress (const visage::KeyEvent& key) override
    {
        return false;
    }
};

struct Slideshow : visage::Frame
{
    std::vector<Slide*> slides {};
    std::string_view name {};
    size_t active_slide = 0;

    explicit Slideshow (std::string_view slides_name, std::initializer_list<Slide*> init_slides)
        : slides { init_slides.begin(), init_slides.end() },
          name { slides_name }
    {
        for (auto* slide : slides)
        {
            addChild (slide, false);
        }
        if (! slides.empty())
        {
            active_slide = 0;
            slides[active_slide]->setVisible (true);
        }

        // requestKeyboardFocus();
        setAcceptsKeystrokes (true);
    }

    ~Slideshow() override
    {
        for (auto* slide : slides)
            delete slide;
    }

    void resized() override
    {
        using namespace visage::dimension;
        for (auto* slide : slides)
            slide->layout().setDimensions (100_vw, 100_vh);
    }

    void previous_slide()
    {
        if (active_slide == 0)
            return;
        slides[active_slide]->setVisible (false);
        active_slide--;
        slides[active_slide]->setVisible (true);
    }

    void next_slide()
    {
        if (active_slide == slides.size() - 1)
            return;
        slides[active_slide]->setVisible (false);
        active_slide++;
        slides[active_slide]->setVisible (true);
    }

    bool keyPress (const visage::KeyEvent& key) override
    {
        if (key.keyCode() == visage::KeyCode::Right)
        {
            next_slide();
            return true;
        }
        if (key.keyCode() == visage::KeyCode::Left)
        {
            previous_slide();
            return true;
        }

        return false;
    }
};
} // namespace chowdsp::slides
