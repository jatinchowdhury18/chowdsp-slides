#pragma once

#include "slides_params.h"

namespace chowdsp::slides
{
struct Slide_Text
{
    std::string_view text {};
    float size {};
    visage::Font::Justification justification { visage::Font::kTopLeft };
    visage::Color color {};
    File* font {};
    Dims dims {};
};

static void merge_params (Slide_Text& text, const Default_Params& params)
{
    if (text.font == nullptr)
        text.font = params.font;

    if (text.color.alpha() == 0.0f)
        text.color = params.text_color;
}

static void draw_text (const Slide_Text& text,
                       visage::Canvas& canvas,
                       const visage::Frame& parent)
{
    if (text.text.empty())
        return;

    auto* stored_text = canvas.getText (text.text.data(),
                                        visage::Font { text.size, text.font->data, (int) text.font->size },
                                        text.justification);
    stored_text->setMultiLine (true);

    const auto [x, y, w, h] = get_coords (text.dims, parent);
    canvas.setColor (text.color);
    canvas.text (stored_text, x, y, w, h);
}
} // namespace chowdsp::slides
