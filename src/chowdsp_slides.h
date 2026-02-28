#pragma once

#include <fstream>
#include <iostream>

#include "slides_background_task.h"

#include "slides_audio_player.h"
#include "slides_bullets.h"
#include "slides_footer.h"
#include "slides_image.h"
#include "slides_text.h"

namespace chowdsp::slides
{
enum Slide_Style
{
    Cover,
    Content,
};

static Slide_Style gon_slide_style (Gon_Ref gon)
{
    const auto str = gon.String ({});
    if (str == "Cover")
        return Cover;
    return Content;
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
    std::vector<Content_Frame*> content {};
};

static std::vector<Content_Frame*> gon_content_array (Gon_Ref gon)
{
    std::vector<Content_Frame*> content {};
    content.reserve (gon.size());

    for (const auto& g : gon)
    {
        const auto type = g["type"].String ({});
        const auto frame_params = gon_content_frame_params (g["frame_params"]);
        if (type == "bullet_list")
        {
            content.push_back (new Bullet_List {
                frame_params,
                gon_bullet_list_params (g["params"]),
                gon_bullet_params_array (g["bullets"]),
            });
        }
        else if (type == "audio_player")
        {
            content.push_back (new Audio_Player {
                frame_params,
                gon_audio_player_params (g["params"]),
            });
        }
        else if (type == "image")
        {
            content.push_back (new Image {
                frame_params,
                gon_image_params (g["params"]),
            });
        }
        else
        {
            // @TODO: handle "custom" content?
            assert (false);
        }
    }

    return content;
}

static Slide_Params gon_slide_params (Gon_Ref gon)
{
    return Slide_Params {
        .background_image = gon_file (gon["background_image"]),
        .background_color = gon["background_color"].UInt ({}),
        .style = gon_slide_style (gon["style"]),
        .title = gon_slide_text (gon["title"]),
        .text = gon_text_array (gon["text"]),
        .content = gon_content_array (gon["content"]),
    };
}

static void merge_params (Slide_Params& slide_params, const Default_Params& default_params)
{
    std::cout << "Background colour: " << slide_params.background_color.toARGBHexString() << '\n';
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

static std::vector<Slide*> gon_slides (Gon_Ref gon)
{
    std::vector<Slide*> res {};
    res.reserve (gon.size());
    for (const auto& g : gon)
        res.push_back (new Slide { gon_slide_params (g) });
    return res;
}

struct Slideshow : visage::Frame
{
    Default_Params* params {};
    std::vector<Slide*> slides {};
    size_t active_slide = 0;
    size_t animation_step = 0;

    Slide_Metadata slide_metadata {};

    Header_Footer* header {};
    Header_Footer* footer {};

    ma_engine audio_engine;
    Image_Atlas image_atlas { visage::ImageAtlas::DataType::RGBA8 };

    // This doesn't work on the web!
    // Background_Task background_task {};

    explicit Slideshow (Gon_Ref gon)
        : Slideshow (gon["title"].String (""),
                     gon["author"].String (""),
                     new Default_Params { gon_default_params (gon["params"]) },
                     gon_slides (gon["slides"]))
    {
    }

    Slideshow (std::string_view slides_name,
               std::string_view slides_author,
               Default_Params* default_params,
               std::vector<Slide*> init_slides)
        : params { default_params },
          slides { init_slides }
    {
        init_audio_engine();
        params->image_atlas = &image_atlas;
        params->slideshow_frame = this;

        using namespace visage::dimension;

        auto slides_y_margin = 0_vh;
        auto slides_height = 100_vh;
        if (params->header_params.type != GonObject::FieldType::NULLGON)
        {
            header = new Header_Footer { params->header_params, *params, &slide_metadata };
            addChild (header);
            slides_y_margin += header->params.height;
            slides_height -= header->params.height;
            header->layout().setDimensions (100_vw, header->params.height);
        }

        if (params->footer_params.type != GonObject::FieldType::NULLGON)
        {
            footer = new Header_Footer { params->footer_params, *params, &slide_metadata };
            addChild (footer);
            slides_height -= footer->params.height;
            footer->layout().setMarginTop (100_vh - footer->params.height);
            footer->layout().setDimensions (100_vw, footer->params.height);
        }

        for (auto* slide : slides)
        {
            slide->set_default_params (params);
            addChild (slide, false);
            slide->layout().setMarginTop (slide->params.style == Cover ? 0_vh : slides_y_margin);
            slide->layout().setDimensions (100_vw, slide->params.style == Cover ? 100_vh : slides_height);
        }
        if (! slides.empty())
        {
            active_slide = 0;
            slides[active_slide]->setVisible (true);
        }

        slide_metadata.slideshow_title = slides_name;
        slide_metadata.slideshow_author = slides_author;
        slide_metadata.slides_count = slides.size();
        update_slide_metadata();

        requestKeyboardFocus();
        setAcceptsKeystrokes (true);
    }

    Slideshow (const Slideshow&) = delete;
    Slideshow& operator= (const Slideshow&) = delete;

    ~Slideshow() override
    {
        delete header;
        delete footer;

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

    void update_slide_metadata()
    {
        slide_metadata.slide_idx = active_slide;
        slide_metadata.slide_title = slides[active_slide]->params.title.text;
        slide_metadata.slide_color = slides[active_slide]->params.background_color;

        if (header != nullptr)
            header->redraw();
        if (footer != nullptr)
            footer->redraw();
    }

    void resized() override
    {
        // @TODO: do this on background thread?
        // background_task.task_queue.enqueue (
        //     [this]
        {
            const std::lock_guard lock { image_atlas.mutex };
            const auto start = std::chrono::high_resolution_clock::now();
            if (image_atlas.count() > 0)
                image_atlas.textureHandle();
            const auto end = std::chrono::high_resolution_clock::now();
            const auto duration = std::chrono::duration_cast<std::chrono::duration<double>> (end - start).count();
            std::cout << "Updated " << image_atlas.count() << " textures in " << duration << " seconds\n";
        }
    }

    void set_state (size_t new_active_slide, size_t start_animation_steps)
    {
        active_slide = new_active_slide;
        for (size_t slide_idx = 0; slide_idx < slides.size(); ++slide_idx)
            slides[slide_idx]->setVisible (slide_idx == active_slide);

        for (size_t step = 0; step < start_animation_steps; ++step)
            next_step();
        assert (start_animation_steps == animation_step);
        update_slide_metadata();
    }

    void previous_slide()
    {
        slides[active_slide]->setVisible (false);
        active_slide--;
        slides[active_slide]->setVisible (true);
        animation_step = 0;
        update_slide_metadata();
    }

    void previous_step()
    {
        if (slides[active_slide]->previous_step())
        {
            animation_step--;
            return;
        }

        if (active_slide == 0)
            return;
        previous_slide();
    }

    void next_slide()
    {
        slides[active_slide]->setVisible (false);
        active_slide++;
        slides[active_slide]->setVisible (true);
        animation_step = 0;
        update_slide_metadata();
    }

    void next_step()
    {
        if (slides[active_slide]->next_step())
        {
            animation_step++;
            return;
        }

        if (active_slide == slides.size() - 1)
            return;
        next_slide();
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
                next_slide();
            return true;
        }
        if (key.keyCode() == visage::KeyCode::Up)
        {
            if (active_slide > 0)
                previous_slide();
            return true;
        }

        return false;
    }
};
} // namespace chowdsp::slides
