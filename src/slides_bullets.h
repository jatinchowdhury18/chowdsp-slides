#pragma once

#include "slides_content.h"

namespace chowdsp::slides
{
struct Bullet_List_Params
{
    Content_Frame_Params frame_params {};
    visage::Color background_color { 0xff212529 };
    visage::Color text_color { 0xffffffff };
    float font { 30.0f };
    visage::Dimension padding { 2_vh };
    visage::Dimension indent { 3_vw };
    bool animate = true;
};

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
    float font {};
    visage::Font::Justification justification { visage::Font::kTopLeft };
    uint8_t flags {};
};

struct Bullet_List : Content_Frame
{
    Bullet_List_Params bullet_list_params {};

    struct Bullet : Content_Frame
    {
        Bullet_List* parent {};
        Bullet_Params bullet_params {};
        Bullet (Bullet_Params ps, bool animate)
            : Content_Frame { {
                  .animate = animate,
              } },
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
            auto* stored_text = canvas.getText (bullet_text,
                                                font (*frame_params.default_params, bullet_params.font),
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
                canvas.rectangle (line_x_padded,
                                  height() - underline_height,
                                  line_width_padded,
                                  underline_height);
            }

            canvas.addShape (std::move (text_block));
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
            new_bullet->parent = this;
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

        canvas.setColor (visage::Color { bullet_list_params.background_color }
                             .withAlpha (fade_alpha()));
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
} // namespace chowdsp::slides
