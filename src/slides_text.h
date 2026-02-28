#pragma once

#include "slides_params.h"

namespace chowdsp::slides
{
static visage::Font::Justification gon_justification (Gon_Ref gon, visage::Font::Justification _default)
{
    const auto str = gon.String ({});
    if (str == "Center")
        return visage::Font::kCenter;
    if (str == "Left")
        return visage::Font::kLeft;
    if (str == "Right")
        return visage::Font::kRight;
    if (str == "Top")
        return visage::Font::kTop;
    if (str == "Bottom")
        return visage::Font::kBottom;
    if (str == "TopLeft")
        return visage::Font::kTopLeft;
    if (str == "BottomLeft")
        return visage::Font::kBottomLeft;
    if (str == "TopRight")
        return visage::Font::kTopRight;
    if (str == "BottomRight")
        return visage::Font::kBottomRight;
    return _default;
}

struct Slide_Text
{
    std::string text {};
    visage::Dimension size {};
    visage::Font::Justification justification { visage::Font::kCenter };
    visage::Color color {};
    File* font {};
    Dims dims {};
};

Slide_Text gon_slide_text (Gon_Ref gon)
{
    return Slide_Text {
        .text = gon["text"].String ({}),
        .size = gon_dim (gon["size"]),
        .justification = gon_justification (gon["justification"], visage::Font::kCenter),
        .color = gon["color"].UInt ({}),
        .font = gon_file (gon["font"]),
        .dims = gon_dims (gon["dims"]),
    };
}

std::vector<Slide_Text> gon_text_array (Gon_Ref gon)
{
    std::vector<Slide_Text> res {};
    res.reserve (gon.size());
    for (const auto& g : gon)
        res.push_back (gon_slide_text (g));
    return res;
}

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

    const auto height = compute_dim (text.size, parent);
    auto* stored_text = canvas.getText (text.text.data(),
                                        visage::Font { height, text.font->data, (int) text.font->size },
                                        text.justification);
    stored_text->setMultiLine (true);

    const auto [x, y, w, h] = get_coords (text.dims, parent);
    canvas.setColor (text.color);
    canvas.text (stored_text, x, y, w, h);
}
} // namespace chowdsp::slides
