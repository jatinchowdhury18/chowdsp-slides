#pragma once

#include <array>
#include <charconv>

#include <visage/app.h>
using namespace visage::dimension;

#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio.h"

#include "third_party/GON/gon.cpp"
#include "third_party/GON/gon.h"

#include "slides_file.h"

namespace chowdsp::slides
{
using Gon_Ref = const GonObject&;

using Dims = std::array<visage::Dimension, 4>;
static visage::Dimension string_to_dim (std::string dim_str, visage::Dimension default_dim)
{
    if (dim_str.size() < 4)
        return default_dim;

    const auto suffix = dim_str.substr (dim_str.size() - 3);
    const auto value = std::stod (dim_str);
    if (suffix == "_vw")
        return visage::Dimension::widthPercent (value);
    else if (suffix == "_vh")
        return visage::Dimension::heightPercent (value);

    return default_dim;
}

static visage::Dimension gon_dim (Gon_Ref gon,
                                  visage::Dimension _default = {})
{
    return string_to_dim (gon.String ({}), _default);
}

static Dims gon_dims (Gon_Ref gon, Dims _default = {})
{
    if (gon.size() != 4)
        return _default;

    Dims dims {};
    for (size_t i = 0; i < 4; ++i)
        dims[i] = string_to_dim (gon[i].String ({}), _default[i]);

    return dims;
}

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

static File* gon_file (Gon_Ref gon)
{
    const auto file_path = gon.String ({});
    if (file_path.empty())
        return {};

    return new File { file_path };
}

struct Image_Atlas;
struct JS_Engine;
struct Default_Params
{
    File* font {};
    visage::Color background_color { 0xff33393f };
    visage::Color text_color { 0xffffffff };
    std::array<float, 2> aspect_ratio {};

    visage::Frame* slideshow_frame {};
    ma_engine* audio_engine {};
    Image_Atlas* image_atlas {};
    JS_Engine* js_engine {};

    GonObject header_params {};
    GonObject footer_params {};
};

static Default_Params gon_default_params (Gon_Ref gon)
{
    Default_Params params {};
    params.font = gon_file (gon["font"]);
    params.background_color = gon["background_color"].UInt (0xff33393f);
    params.text_color = gon["text_color"].UInt (0xffffffff);
    params.header_params = gon["header"];
    params.footer_params = gon["footer"];

    const auto gon_aspect_ratio = gon["aspect_ratio"];
    if (gon_aspect_ratio.size() == 2)
    {
        params.aspect_ratio = {
            (float) gon_aspect_ratio[0].Number ({}),
            (float) gon_aspect_ratio[1].Number ({}),
        };
    }

    return params;
}

static auto font (const Default_Params& params, float size)
{
    return visage::Font {
        size,
        params.font->data,
        (int) params.font->size,
    };
}

std::array<float, 4> fit_and_center (float container_width,
                                     float container_height,
                                     float content_width,
                                     float content_height)
{
    float container_aspect_ratio = container_width / container_height;
    float content_aspect_ratio = content_width / content_height;

    std::array<float, 4> result {};

    if (container_aspect_ratio > content_aspect_ratio)
    {
        // Container is wider than content
        result[3] = container_height;
        result[2] = container_height * content_aspect_ratio;
    }
    else
    {
        // Container is taller than content
        result[2] = container_width;
        result[3] = container_width / content_aspect_ratio;
    }

    // Center it
    result[0] = (container_width - result[2]) * 0.5f;
    result[1] = (container_height - result[3]) * 0.5f;

    return result;
}

struct Slide_Metadata
{
    size_t slide_idx {};
    size_t slides_count {};

    std::string slideshow_title {};
    std::string slideshow_author {};

    std::string slide_title {};
    std::string slide_section {};
    visage::Color slide_color {};
};
} // namespace chowdsp::slides
