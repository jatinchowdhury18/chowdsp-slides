#pragma once

#include "slides_content.h"
#include "slides_text.h"

namespace chowdsp::slides
{
struct Bullet_List_Params
{
    visage::Color background_color { 0xff212529 };
    visage::Color text_color { 0xffffffff };
    Dimension font_height { height_percent (3.5) };
    Dimension padding { height_percent (2.0) };
    Dimension indent { width_percent (4) };
    std::span<std::string_view> markers {}; // one per indent level, the last one is reused for deeper levels
    bool animate = true;
};

static Bullet_List_Params gon_bullet_list_params (Gon_Ref gon, Allocator& allocator)
{
    const auto markers_gon = gon["markers"];
    auto markers = allocator.make_span<std::string_view> (markers_gon.size());
    size_t idx = 0;
    for (const auto& g : markers_gon)
        markers[idx++] = allocator.copy_string (g.StringView ({}));

    return Bullet_List_Params {
        .background_color = gon["background_color"].UInt (0xff212529),
        .text_color = gon["text_color"].UInt (0xffffffff),
        .font_height = gon_dim (gon["font_height"], height_percent (3.5)),
        .padding = gon_dim (gon["padding"], height_percent (2.0)),
        .indent = gon_dim (gon["indent"], width_percent (4)),
        .markers = markers,
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
    std::string_view text {};
    std::string_view marker {};
    int indent = 0;
    visage::Color text_color {};
    Dimension font_height {};
    visage::Font::Justification justification { visage::Font::kTopLeft };
    Dimension y_pad { height_percent (0) };
    uint8_t flags {};
};

static Bullet_Params gon_bullet_params (Gon_Ref gon, Allocator& allocator)
{
    Bullet_Params params {
        .text = allocator.copy_string (gon["text"].StringView ({})),
        .marker = allocator.copy_string (gon["marker"].StringView ({})),
        .indent = gon["indent"].Int ({}),
        .text_color = gon["text_color"].UInt ({}),
        .font_height = gon_dim (gon["font_height"]),
        .justification = gon_justification (gon["justification"], visage::Font::kTopLeft),
        .y_pad = gon_dim (gon["y_pad"], height_percent (0)),
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

static std::span<Bullet_Params> gon_bullet_params_array (Gon_Ref gon, Allocator& allocator)
{
    auto params = allocator.make_span<Bullet_Params> (gon.size());
    size_t idx = 0;
    for (const auto& g : gon)
        params[idx++] = gon_bullet_params (g, allocator);
    return params;
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

        float marker_width (float font_height) const
        {
            if (bullet_params.flags & BULLET_NO_BULLET)
                return 0.0f;
            const auto marker = visage::String::convertToUtf32 (std::string { bullet_params.marker } + " ");
            return font (default_params, font_height).withDpiScale (dpiScale()).stringWidth (marker);
        }

        int line_count (float font_height, float width) const
        {
            const auto text = visage::String::convertToUtf32 (std::string { bullet_params.text });
            const auto text_width = width - marker_width (font_height);
            return (int) font (default_params, font_height).withDpiScale (dpiScale()).lineBreaks (text.c_str(), (int) text.size(), text_width).size() + 1;
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

            canvas.setColor (bullet_params.text_color.withAlpha (alpha));
            const auto font_height = compute_dim (bullet_params.font_height, *default_params.slideshow_frame);
            const auto text_font = font (default_params, font_height);

            // The marker sits in its own gutter, so wrapped lines align with the text rather than the marker.
            const auto text_x = marker_width (font_height);
            if (text_x > 0.0f)
                canvas.text (std::string { bullet_params.marker }, text_font, bullet_params.justification, 0.0f, 0.0f, text_x, height());

            auto* stored_text = canvas.getText (std::string { bullet_params.text },
                                                text_font,
                                                bullet_params.justification);
            stored_text->setMultiLine (true);
            auto&& text_block = canvas.getTextBlock (stored_text, text_x, 0.0f, width() - text_x, height());

            if (bullet_params.flags & BULLET_UNDERLINE)
            {
                const auto scale = 1.0f / dpiScale();

                const auto line_x = text_block.actual_bounds.left * scale;
                const auto line_width = (text_block.actual_bounds.right - text_block.actual_bounds.left) * scale;
                const auto line_width_padded = line_width * 1.15f * alpha;
                const auto line_x_padded = line_x - (line_width_padded - line_width) * 0.5f;

                const auto underline_height = 0.04f * height();
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
                 std::span<Bullet_Params> bullet_params = {})
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
            if (bullet_params[idx].marker.empty())
                bullet_params[idx].marker = marker_for_indent (bullet_params[idx].indent);
            bullets[idx] = default_params.frame_allocator->allocate<Bullet> (default_params,
                                                                             bullet_params[idx],
                                                                             bullet_list_params.animate);
            bullets[idx]->parent = this;
            addChild (bullets[idx]);
        }
    }

    std::string_view marker_for_indent (int indent) const
    {
        const auto& markers = bullet_list_params.markers;
        if (markers.empty())
            return "•";
        return markers[std::min ((size_t) std::max (indent, 0), markers.size() - 1)];
    }

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);

        const auto background_color = visage::Color { bullet_list_params.background_color };
        canvas.setColor (background_color.withAlpha (background_color.alpha() * fade_alpha()));
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
            const auto bullet_width = width() - x - pad_x;
            const auto extra_lines = bullet->line_count (font_height, bullet_width) - 1;
            const auto height = font_height + pad_y + float (extra_lines) * font (default_params, font_height).withDpiScale (dpiScale()).lineHeight();
            bullet->setBounds (x, y, bullet_width, height);
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
