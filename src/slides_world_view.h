#pragma once

// This header is included from chowdsp_slides.h after the Slide struct is defined,
// and therefore lives within the chowdsp::slides namespace already.

#include <algorithm>
#include <functional>

struct World_View : visage::Frame
{
    std::span<Slide*> slides {};
    const Default_Params& params;
    size_t active_slide {};

    float scroll_y = 0.0f;
    int hovered_idx = -1;

    static constexpr int kCols = 4;
    static constexpr float kPadFrac = 0.018f;
    static constexpr float kBorderThick = 3.0f;

    World_View (std::span<Slide*> s, const Default_Params& p)
        : slides (s), params (p)
    {
        setAcceptsKeystrokes (true);
    }

    // Call this instead of setVisible(true) to also center on the active slide.
    void open (size_t current_slide)
    {
        active_slide = current_slide;
        if (width() > 0.0f)
            scroll_to_active();
        hovered_idx = -1;
        setVisible (true);
        requestKeyboardFocus();
    }

    std::function<void (size_t)> on_slide_selected;
    std::function<void()> on_close;

private:
    float pad() const { return kPadFrac * width(); }

    float tile_w() const { return (width() - pad() * (kCols + 1)) / kCols; }

    float tile_h() const
    {
        if (params.aspect_ratio[0] > 0.0f)
            return tile_w() * params.aspect_ratio[1] / params.aspect_ratio[0];
        return tile_w() * 9.0f / 16.0f;
    }

    float tile_x (int i) const { return pad() + (float) (i % kCols) * (tile_w() + pad()); }
    float tile_y (int i) const { return pad() + (float) (i / kCols) * (tile_h() + pad()) + scroll_y; }

    float total_content_h() const
    {
        const int rows = ((int) slides.size() + kCols - 1) / kCols;
        return pad() * (rows + 1) + tile_h() * rows;
    }

    void clamp_scroll()
    {
        const float min_s = std::min (0.0f, height() - total_content_h());
        scroll_y = std::clamp (scroll_y, min_s, 0.0f);
    }

    void scroll_to_active()
    {
        const float target = -(float) (active_slide / kCols) * (tile_h() + pad()) + height() * 0.25f;
        scroll_y = target;
        clamp_scroll();
    }

    int idx_at (float mx, float my) const
    {
        const float tw = tile_w(), th = tile_h();
        for (int i = 0; i < (int) slides.size(); ++i)
        {
            const float tx = tile_x (i), ty = tile_y (i);
            if (mx >= tx && mx < tx + tw && my >= ty && my < ty + th)
                return i;
        }
        return -1;
    }

    void draw (visage::Canvas& canvas) override
    {
        canvas.setColor (visage::Color { 0xdd111111 });
        canvas.fill (0.0f, 0.0f, width(), height());

        const float tw = tile_w(), th = tile_h();
        if (tw <= 0.0f || th <= 0.0f)
            return;

        const float title_font_h = th * 0.12f;
        const float num_font_h = th * 0.08f;

        for (int i = 0; i < (int) slides.size(); ++i)
        {
            const float tx = tile_x (i), ty = tile_y (i);
            if (ty + th < 0.0f || ty > height())
                continue;

            const auto& sp = slides[i]->params;

            auto bg = sp.background_color;
            if (bg.alpha() == 0.0f)
                bg = params.background_color;
            canvas.setColor (bg);
            canvas.fill (tx, ty, tw, th);

            if (! sp.title.text.empty() && params.font != nullptr)
            {
                const auto text_col = sp.title.color.alpha() > 0.0f ? sp.title.color : params.text_color;
                canvas.setColor (text_col);
                canvas.text (std::string (sp.title.text),
                             visage::Font { title_font_h, params.font->data, (int) params.font->size },
                             visage::Font::kCenter,
                             tx, ty, tw, th);
            }

            if (params.font != nullptr)
            {
                canvas.setColor (visage::Color { 0xaaffffff });
                canvas.text (std::to_string (i + 1),
                             visage::Font { num_font_h, params.font->data, (int) params.font->size },
                             visage::Font::kBottomLeft,
                             tx + 4.0f, ty, tw - 8.0f, th - 4.0f);
            }

            const bool is_active = (i == (int) active_slide);
            const bool is_hovered = (i == hovered_idx);
            if (is_active || is_hovered)
            {
                canvas.setColor (is_active ? visage::Color { 0xff4c9eff } : visage::Color { 0xaaffffff });
                canvas.rectangleBorder (tx, ty, tw, th, kBorderThick);
            }
        }
    }

    bool mouseWheel (const visage::MouseEvent& e) override
    {
        const float delta = e.precise_wheel_delta_y != 0.0f ? e.precise_wheel_delta_y : e.wheel_delta_y;
        scroll_y += delta * 40.0f;
        clamp_scroll();
        redraw();
        return true;
    }

    void mouseMove (const visage::MouseEvent& e) override
    {
        const int new_hover = idx_at (e.position.x, e.position.y);
        if (new_hover != hovered_idx)
        {
            hovered_idx = new_hover;
            redraw();
        }
    }

    void mouseExit (const visage::MouseEvent& e) override
    {
        if (hovered_idx >= 0)
        {
            hovered_idx = -1;
            redraw();
        }
    }

    void mouseUp (const visage::MouseEvent& e) override
    {
        if (! e.isLeftButton())
            return;
        const int clicked = idx_at (e.position.x, e.position.y);
        if (clicked >= 0 && on_slide_selected)
            on_slide_selected (static_cast<size_t> (clicked));
    }

    bool keyPress (const visage::KeyEvent& key) override
    {
        if (key.keyCode() == visage::KeyCode::Escape || key.keyCode() == visage::KeyCode ('w'))
        {
            if (on_close) on_close();
            return true;
        }
        return false;
    }

    void resized() override
    {
        clamp_scroll();
        redraw();
    }
};


