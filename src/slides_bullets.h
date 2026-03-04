#pragma once

#include "slides_content.h"
#include "slides_text.h"

namespace chowdsp::slides
{
struct Bullet_List_Params
{
    visage::Color background_color { 0xff212529 };
    visage::Color text_color { 0xffffffff };
    visage::Dimension font_height { visage::Dimension::heightPercent (3.5) };
    visage::Dimension padding { visage::Dimension::heightPercent (2.0) };
    visage::Dimension indent { 4_vw };
    bool animate = true;
};

static Bullet_List_Params gon_bullet_list_params (Gon_Ref gon)
{
    return Bullet_List_Params {
        .background_color = gon["background_color"].UInt (0xff212529),
        .text_color = gon["text_color"].UInt (0xffffffff),
        .font_height = gon_dim (gon["font_height"], visage::Dimension::heightPercent (3.5)),
        .padding = gon_dim (gon["padding"], visage::Dimension::heightPercent (2.0)),
        .indent = gon_dim (gon["indent"], 4_vw),
        .animate = gon["animate"].Bool (true),
    };
}

enum Bullet_Flags : uint8_t
{
    BULLET_NO_BULLET = 1 << 0,
    BULLET_UNDERLINE = 1 << 1,
};

struct Bullet_Params
{
    std::string text {};
    int indent = 0;
    visage::Color text_color {};
    visage::Dimension font_height {};
    visage::Font::Justification justification { visage::Font::kTopLeft };
    visage::Dimension y_pad { 0_vh };
    uint8_t flags {};
};

static Bullet_Params gon_bullet_params (Gon_Ref gon)
{
    Bullet_Params params {
        .text = gon["text"].String ({}),
        .indent = gon["indent"].Int ({}),
        .text_color = gon["text_color"].UInt ({}),
        .font_height = gon_dim (gon["font_height"]),
        .justification = gon_justification (gon["justification"], visage::Font::kTopLeft),
        .y_pad = gon_dim (gon["y_pad"], 0_vh),
    };

    const auto flags = gon["flags"];
    for (auto flag : flags)
    {
        const auto flag_str = flag.String ({});
        if (flag_str == "BULLET_NO_BULLET")
            params.flags |= BULLET_NO_BULLET;
        else if (flag_str == "BULLET_UNDERLINE")
            params.flags |= BULLET_UNDERLINE;
    }

    return params;
}

// @TODOL vector
static std::vector<Bullet_Params> gon_bullet_params_array (Gon_Ref gon)
{
    std::vector<Bullet_Params> res {};
    res.reserve (gon.size());

    for (const auto& g : gon)
        res.push_back (gon_bullet_params (g));

    return res;
}

struct Bullet_List : Content_Frame
{
    Bullet_List_Params bullet_list_params {};

    struct Bullet : Content_Frame
    {
        Bullet_List* parent {};
        Bullet_Params bullet_params {};
        Bullet (const Default_Params& def_params, Bullet_Params ps, bool animate)
            : Content_Frame { def_params, { .animate = animate } },
              bullet_params { ps }
        {
        }

        virtual float fade_alpha() const override
        {
            const auto parent_alpha = parent->fade_alpha();
            const auto self_alpha = Content_Frame::fade_alpha();
            return parent_alpha * self_alpha;
        }

        void draw (visage::Canvas& canvas) override
        {
            Content_Frame::draw (canvas);
            const auto alpha = fade_alpha();
            if (alpha == 0.0f)
                return;

            // @TODO: different bullet point options...
            // probably treat bullet point as an image? then the text would be better aligned too...?
            auto bullet_text = std::string { bullet_params.flags & BULLET_NO_BULLET ? "" : "- " };
            bullet_text += bullet_params.text;
            canvas.setColor (bullet_params.text_color.withAlpha (alpha));
            const auto font_height = compute_dim (bullet_params.font_height, *default_params.slideshow_frame);
            auto* stored_text = canvas.getText (bullet_text,
                                                font (default_params, font_height),
                                                bullet_params.justification);
            stored_text->setMultiLine (true);
            auto&& text_block = canvas.getTextBlock (stored_text, 0.0f, 0.0f, width(), height());

            if (bullet_params.flags & BULLET_UNDERLINE)
            {
                const auto scale = 1.0f / dpiScale();

                const auto line_x = text_block.actual_bounds.left * scale;
                const auto line_width = (text_block.actual_bounds.right - text_block.actual_bounds.left) * scale;
                const auto line_width_padded = line_width * 1.15f * alpha;
                const auto line_x_padded = line_x - (line_width_padded - line_width) * 0.5f;

                const auto underline_height = compute_dim (4_vh, *this);
                const auto text_y_pad = (height() - font_height) * 0.5f;
                const auto text_bottom = text_y_pad + font_height;
                const auto line_y = std::min (text_bottom + 2 * underline_height, height() - underline_height);

                canvas.rectangle (line_x_padded,
                                  line_y,
                                  line_width_padded,
                                  underline_height);
            }

            canvas.addShape (std::move (text_block));
        }
    };
    std::span<Bullet*> bullets {};

    Bullet_List (const Default_Params& def_params,
                 Content_Frame_Params frame_params,
                 Bullet_List_Params this_list_params,
                 std::vector<Bullet_Params> bullet_params = {})
        : Content_Frame { def_params, frame_params },
          bullet_list_params { this_list_params }
    {
        if (bullet_list_params.animate)
            animation_steps = bullet_params.size();

        const auto bullets_count = bullet_params.size();
        bullets = default_params.frame_allocator->make_span<Bullet*> (bullets_count);
        for (size_t idx = 0; idx < bullets_count; ++idx)
        {
            if (bullet_params[idx].text_color.alpha() == 0.0f)
                bullet_params[idx].text_color = bullet_list_params.text_color;
            if (bullet_params[idx].font_height.amount == 0.0f)
                bullet_params[idx].font_height = bullet_list_params.font_height;
            bullets[idx] = default_params.frame_allocator->allocate<Bullet> (default_params,
                                                                             bullet_params[idx],
                                                                             bullet_list_params.animate);
            bullets[idx]->parent = this;
            addChild (bullets[idx]);
        }
    }

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);

        canvas.setColor (visage::Color { bullet_list_params.background_color }
                             .withAlpha (fade_alpha()));
        const auto pad = compute_dim (bullet_list_params.padding, *default_params.slideshow_frame);
        canvas.roundedRectangle (0, 0, width(), height(), pad);
    }

    void resized() override
    {
        const auto indent_x = compute_dim (bullet_list_params.indent, *this);
        const auto pad_x = compute_dim (bullet_list_params.padding, *default_params.slideshow_frame);
        const auto pad_y = pad_x;
        auto y = pad_y;
        for (auto* bullet : bullets)
        {
            const auto x = indent_x * bullet->bullet_params.indent + pad_x;
            const auto font_height = compute_dim (bullet->bullet_params.font_height, *default_params.slideshow_frame);
            const auto height = font_height + pad_y;
            bullet->setBounds (x, y, width() - 2 * pad_x, height);
            y += height + compute_dim (bullet->bullet_params.y_pad, *default_params.slideshow_frame);
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
} // namespace chowdsp::slides
