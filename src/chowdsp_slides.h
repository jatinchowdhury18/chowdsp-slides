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

static auto compute_dim (visage::Dimension dim, const visage::Frame& parent)
{
    return dim.compute (parent.dpiScale(), parent.width(), parent.height());
}

static auto get_coords (Dims dims, const visage::Frame& parent)
{
    return std::make_tuple (
        compute_dim (dims[0], parent),
        compute_dim (dims[1], parent),
        compute_dim (dims[2], parent),
        compute_dim (dims[3], parent));
}

static auto set_bounds (Dims dims, visage::Frame& child, const visage::Frame& parent)
{
    child.setBounds (compute_dim (dims[0], parent),
                     compute_dim (dims[1], parent),
                     compute_dim (dims[2], parent),
                     compute_dim (dims[3], parent));
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
}

struct Content_Frame_Params
{
    Dims dims {};
    Default_Params** default_params {};
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
        animation.setTargetValue (1.0f);
        animation.target (! frame_params.animate);
    }

    void show()
    {
        animation.target (true);
        redraw();
    }

    void hide()
    {
        animation.target (false);
        redraw();
    }

    void draw (visage::Canvas& canvas) override
    {
        animation.update();
        if (animation.isAnimating())
            redraw();
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

struct Bullet_List_Params
{
    Content_Frame_Params frame_params {};
    visage::Color text_color { 0xffffffff };
    float font { 30.0f };
    visage::Dimension padding { 2_vh };
    visage::Dimension indent { 3_vw };
    bool animate = true;
};

struct Bullet_Params
{
    std::string text {};
    int indent = 0;
    visage::Color text_color {};
    float font {};
};

struct Bullet_List : Content_Frame
{
    Bullet_List_Params bullet_list_params {};

    struct Bullet : Content_Frame
    {
        Bullet_Params bullet_params {};
        Bullet (Bullet_Params ps, bool animate)
            : Content_Frame { {
                  .animate = animate,
              } },
              bullet_params { ps }
        {
        }

        void draw (visage::Canvas& canvas) override
        {
            Content_Frame::draw (canvas);
            if (animation.value() == 0.0f)
                return;

            // @TODO: different bullet point options...
            // probably treat bullet point as an image? then the text would be better aligned too...?
            const auto bullet_text = std::string { "- " } + bullet_params.text;
            canvas.setColor (bullet_params.text_color.withAlpha (animation.value()));
            auto* stored_text = canvas.getText (bullet_text,
                                                visage::Font { bullet_params.font,
                                                               (*frame_params.default_params)->font->data,
                                                               (int) (*frame_params.default_params)->font->size },
                                                visage::Font::kTopLeft);
            stored_text->setMultiLine (true);
            canvas.text (stored_text, 0.0f, 0.0f, width(), height());
        }
    };
    std::vector<Bullet*> bullets {};

    Bullet_List (Bullet_List_Params this_list_params,
                 std::initializer_list<Bullet_Params> bullet_params = {})
        : Content_Frame { this_list_params.frame_params },
          bullet_list_params { this_list_params }
    {
        if (bullet_list_params.animate)
            animation_steps = bullet_params.size();

        for (auto one_bullet_params : bullet_params)
        {
            if (one_bullet_params.text_color.alpha() == 0.0f)
                one_bullet_params.text_color = bullet_list_params.text_color;
            if (one_bullet_params.font == 0.0f)
                one_bullet_params.font = bullet_list_params.font;
            auto& new_bullet = bullets.emplace_back (new Bullet { one_bullet_params, bullet_list_params.animate });
            addChild (new_bullet);
        }
    }

    ~Bullet_List() override
    {
        for (auto* bullet : bullets)
            delete bullet;
    }

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);

        canvas.setColor (visage::Color { 0xff212529 }.withAlpha (animation.value()));
        const auto pad = compute_dim (bullet_list_params.padding, *this);
        canvas.roundedRectangle (0, 0, width(), height(), pad * 2);
    }

    void resized() override
    {
        const auto indent_x = compute_dim (bullet_list_params.indent, *this);
        const auto pad_x = compute_dim (bullet_list_params.padding, *this);
        const auto pad_y = compute_dim (bullet_list_params.padding, *this);
        auto y = pad_y;
        for (auto* bullet : bullets)
        {
            bullet->frame_params.default_params = frame_params.default_params;

            const auto x = indent_x * bullet->bullet_params.indent + pad_x;
            const auto height = bullet->bullet_params.font + pad_y;
            bullet->setBounds (x, y, width(), height);
            y += height;
        }
    }

    bool previous_step() override
    {
        if (active_animation_step == 0)
            return false;

        active_animation_step--;
        bullets[active_animation_step]->hide();
        return true;
    }

    bool next_step() override
    {
        if (animation_steps == 0 || active_animation_step == animation_steps)
            return false;

        bullets[active_animation_step]->show();
        active_animation_step++;
        return true;
    }
};

struct Slide_Params
{
    // background
    File* background_image {};
    visage::Color background_color {};

    // content
    Slide_Style style { Content };

    Slide_Text title {};

    std::vector<Content_Frame*> content {};

    std::vector<Slide_Text> text {};
};

struct Slide : visage::Frame
{
    Slide_Params params {};
    Default_Params* default_params {};
    size_t animation_frames {};
    size_t active_animation_frame {};
    std::vector<Content_Frame*> frames_to_animate {};

    Slide (Slide_Params slide_params) : params { slide_params }
    {
        for (auto* content_frame : params.content)
        {
            addChild (content_frame);
            content_frame->frame_params.default_params = &default_params;

            if (content_frame->frame_params.animate || content_frame->animation_steps != 0)
            {
                animation_frames++;
                frames_to_animate.push_back (content_frame);
            }
        }
    }

    ~Slide()
    {
        if (params.background_image != nullptr)
            delete params.background_image;

        for (auto* content_frame : params.content)
            delete content_frame;
    }

    bool previous_step()
    {
        if (active_animation_frame == 0)
            return false;

        if (active_animation_frame > 0)
            frames_to_animate[active_animation_frame - 1]->previous_step();

        if (frames_to_animate[active_animation_frame - 1]->active_animation_step == 0)
        {
            active_animation_frame--;
            frames_to_animate[active_animation_frame]->hide();
        }
        return true;
    }

    bool next_step()
    {
        if (animation_frames == 0)
            return false;

        if (active_animation_frame > 0 && frames_to_animate[active_animation_frame - 1]->next_step())
            return true;

        if (active_animation_frame == animation_frames)
            return false;

        frames_to_animate[active_animation_frame]->show();
        active_animation_frame++;
        frames_to_animate[active_animation_frame - 1]->next_step();
        return true;
    }

    void resized() override
    {
        for (auto* content_frame : params.content)
            set_bounds (content_frame->frame_params.dims, *content_frame, *this);
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

    void previous_step()
    {
        if (slides[active_slide]->previous_step())
            return;

        if (active_slide == 0)
            return;
        slides[active_slide]->setVisible (false);
        active_slide--;
        slides[active_slide]->setVisible (true);
    }

    void next_step()
    {
        if (slides[active_slide]->next_step())
            return;

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
            next_step();
            return true;
        }
        if (key.keyCode() == visage::KeyCode::Left)
        {
            previous_step();
            return true;
        }

        return false;
    }
};
} // namespace chowdsp::slides
