#pragma once

#include <iostream>
#include <visage/app.h>
using namespace visage::dimension;

#include "slides_file.h"

namespace chowdsp::slides
{
enum Slide_Style
{
    Cover,
    Content,
};

struct Default_Params
{
    File* font {};
    visage::Color text_color { 0xffffffff };
};

using Dims = std::array<visage::Dimension, 4>;
struct Slide_Text
{
    std::string_view text {};
    float size {};
    visage::Font::Justification justification { visage::Font::kTopLeft };
    visage::Color color {};
    File* font {};
    Dims dims {};
};

static auto get_coords (Dims dims, const visage::Frame& parent)
{
    return std::make_tuple (
        dims[0].compute (parent.dpiScale(), parent.width(), parent.height()),
        dims[1].compute (parent.dpiScale(), parent.width(), parent.height()),
        dims[2].compute (parent.dpiScale(), parent.width(), parent.height()),
        dims[3].compute (parent.dpiScale(), parent.width(), parent.height()));
}

static void draw_text (const Slide_Text& text,
                       const Default_Params& params,
                       visage::Canvas& canvas,
                       const visage::Frame& parent)
{
    if (text.text.empty())
        return;

    auto* font = text.font != nullptr ? text.font : params.font;
    auto color = text.color.alpha() > 0.0f ? text.color : params.text_color;
    const auto [x, y, w, h] = get_coords (text.dims, parent);

    canvas.setColor (color);

    auto* stored_text = canvas.getText (text.text.data(),
                                        visage::Font { text.size, font->data, (int) font->size },
                                        text.justification);
    stored_text->setMultiLine (true);
    canvas.text (stored_text, x, y, w, h);

    // canvas.text (text.text.data(),
    //              visage::Font { text.size, font->data, (int) font->size },
    //              text.justification,
    //              x,
    //              y,
    //              w,
    //              h);
}

struct Slide_Params
{
    // background
    File* background_image {};
    visage::Color background_color {};

    // content
    Slide_Style style { Content };

    Slide_Text title {};
    std::vector<Slide_Text> text {};
};

struct Slide : visage::Frame
{
    Slide_Params params {};
    Default_Params* default_params {};

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
        if (params.background_image != nullptr)
        {
            canvas.setColor (0xffffffff);
            canvas.image (params.background_image->data, params.background_image->size, 0, 0, width(), height());
        }
        else if (params.background_color.alpha() == 0.0f)
        {
            params.background_color = 0xff33393f;
        }

        canvas.setColor (params.background_color);
        canvas.fill (0, 0, width(), height());

        if (params.style == Cover)
        {
            if (params.title.dims[2].amount == 0.0f)
                params.title.dims = { 0_vw, 0_vh, 100_vw, 100_vh };
            draw_text (params.title, *default_params, canvas, *this);
        }
        else
        {
            if (params.title.dims[2].amount == 0.0f)
                params.title.dims = { 2_vw, 0_vh, 96_vw, 10_vh };
            draw_text (params.title, *default_params, canvas, *this);
        }

        for (auto& text_item : params.text)
            draw_text (text_item, *default_params, canvas, *this);
    }

    bool keyPress (const visage::KeyEvent& key) override
    {
        return false;
    }
};

struct Slideshow : visage::Frame
{
    Default_Params* params {};
    std::vector<Slide*> slides {};
    std::string_view name {};
    size_t active_slide = 0;

    explicit Slideshow (std::string_view slides_name,
                        Default_Params* default_params,
                        std::initializer_list<Slide*> init_slides)
        : params { default_params },
          slides { init_slides.begin(), init_slides.end() },
          name { slides_name }
    {
        for (auto* slide : slides)
        {
            slide->default_params = params;
            addChild (slide, false);
            using namespace visage::dimension;
            slide->layout().setDimensions (100_vw, 100_vh);
        }
        if (! slides.empty())
        {
            active_slide = 0;
            slides[active_slide]->setVisible (true);
        }

        // requestKeyboardFocus();
        setAcceptsKeystrokes (true);

        onDraw() = [] (visage::Canvas& canvas)
        {
            canvas.setColor (0xff00ff00);
            canvas.fill (0, 0, canvas.width(), canvas.height());
        };
    }

    Slideshow (const Slideshow&) = delete;
    Slideshow& operator= (const Slideshow&) = delete;

    ~Slideshow() override
    {
        for (auto* slide : slides)
            delete slide;

        delete params->font;
        delete params;
    }

    void set_active_slide (size_t new_active_slide)
    {
        active_slide = new_active_slide;
        for (size_t slide_idx = 0; slide_idx < slides.size(); ++slide_idx)
            slides[slide_idx]->setVisible (slide_idx == active_slide);
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
