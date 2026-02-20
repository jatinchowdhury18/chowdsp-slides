#pragma once

#include <iostream>

#include "slides_background_task.h"

#include "slides_audio_player.h"
#include "slides_bullets.h"
#include "slides_image.h"
#include "slides_text.h"

namespace chowdsp::slides
{
enum Slide_Style
{
    Cover,
    Content,
};

struct Slide_Params
{
    // background
    File* background_image {};
    visage::Color background_color {};

    // content
    Slide_Style style { Content };

    Slide_Text title {};
    std::vector<Slide_Text> text {};
    std::vector<Content_Frame*> content {};
};

static void merge_params (Slide_Params& slide_params, const Default_Params& default_params)
{
    if (slide_params.background_color.alpha() == 0.0f && slide_params.background_image == nullptr)
        slide_params.background_color = default_params.background_color;

    merge_params (slide_params.title, default_params);
    for (auto& text : slide_params.text)
        merge_params (text, default_params);
}

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
            addChild (content_frame, ! content_frame->frame_params.animate);

            if (content_frame->frame_params.animate || content_frame->animation_steps != 0)
            {
                animation_frames++;
                frames_to_animate.push_back (content_frame);
            }
        }
    }

    ~Slide()
    {
        delete params.background_image;

        for (auto* content_frame : params.content)
            delete content_frame;
    }

    void set_default_params (Default_Params* new_default_params)
    {
        default_params = new_default_params;
        merge_params (params, *default_params);
        for (auto* content : params.content)
            content->set_default_params (default_params);
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

        if (params.background_color.alpha() > 0.0f)
        {
            canvas.setColor (params.background_color);
            canvas.fill (0, 0, width(), height());
        }

        if (params.style == Cover)
        {
            if (params.title.dims[2].amount == 0.0f)
                params.title.dims = { 0_vw, 0_vh, 100_vw, 100_vh };
            draw_text (params.title, canvas, *this);
        }
        else
        {
            if (params.title.dims[2].amount == 0.0f)
                params.title.dims = { 2_vw, 0_vh, 96_vw, 10_vh };
            draw_text (params.title, canvas, *this);
        }

        for (auto& text_item : params.text)
            draw_text (text_item, canvas, *this);
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

    ma_engine audio_engine;
    Image_Atlas image_atlas { visage::ImageAtlas::DataType::RGBA8 };

    Background_Task background_task {};

    explicit Slideshow (std::string_view slides_name,
                        Default_Params* default_params,
                        std::initializer_list<Slide*> init_slides)
        : params { default_params },
          slides { init_slides.begin(), init_slides.end() },
          name { slides_name }
    {
        init_audio_engine();
        params->image_atlas = &image_atlas;

        for (auto* slide : slides)
        {
            slide->set_default_params (params);
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
    }

    Slideshow (const Slideshow&) = delete;
    Slideshow& operator= (const Slideshow&) = delete;

    ~Slideshow() override
    {
        for (auto* slide : slides)
            delete slide;

        delete params->font;
        delete params;

        ma_engine_uninit (&audio_engine);
    }

    void init_audio_engine()
    {
        params->audio_engine = &audio_engine;

        auto result = ma_engine_init (nullptr, &audio_engine);
        assert (result == MA_SUCCESS);
    }

    void resized() override
    {
        // @TODO: do this on background thread?
        // background_task.task_queue.enqueue (
        //     [this]
        {
            const std::lock_guard lock { image_atlas.mutex };
            const auto start = std::chrono::high_resolution_clock::now();
            image_atlas.textureHandle();
            const auto end = std::chrono::high_resolution_clock::now();
            const auto duration = std::chrono::duration_cast<std::chrono::duration<double>> (end - start).count();
            std::cout << "Updated " << image_atlas.count() << " textures in " << duration << " seconds\n";
        }
        // );
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
        if (key.keyCode() == visage::KeyCode::Down)
        {
            if (active_slide < slides.size() - 1)
            {
                slides[active_slide]->setVisible (false);
                active_slide++;
                slides[active_slide]->setVisible (true);
            }
            return true;
        }
        if (key.keyCode() == visage::KeyCode::Up)
        {
            if (active_slide > 0)
            {
                slides[active_slide]->setVisible (false);
                active_slide--;
                slides[active_slide]->setVisible (true);
            }
            return true;
        }

        return false;
    }
};
} // namespace chowdsp::slides
