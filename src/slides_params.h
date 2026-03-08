#pragma once

#include <array>
#include <charconv>

#include <visage/app.h>

#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio.h"

#include "third_party/GON/gon.cpp"
#include "third_party/GON/gon.h"

#include "slides_file.h"

namespace chowdsp::slides
{
using Gon_Ref = const GonObject&;

struct Dimension
{
    enum Type
    {
        NONE,
        WIDTH,
        HEIGHT,
    };
    float amount {};
    Type type { NONE };
};
using Dims = std::array<Dimension, 4>;

template <typename T>
static Dimension width_percent (T percent)
{
    return {
        .amount = static_cast<float> (percent) * 0.01f,
        .type = Dimension::WIDTH,
    };
}

template <typename T>
static Dimension height_percent (T percent)
{
    return {
        .amount = static_cast<float> (percent) * 0.01f,
        .type = Dimension::HEIGHT,
    };
}

static visage::Dimension to_visage (Dimension& dim)
{
    if (dim.type == Dimension::WIDTH)
        return visage::Dimension::widthPercent (dim.amount * 100.0f);
    if (dim.type == Dimension::HEIGHT)
        return visage::Dimension::heightPercent (dim.amount * 100.0f);
    return {};
}

static Dimension string_to_dim (std::string_view dim_str, Dimension default_dim)
{
    if (dim_str.size() < 4)
        return default_dim;

    const auto suffix = dim_str.substr (dim_str.size() - 3);
    const auto value = std::strtod (dim_str.data(), nullptr);
    if (suffix == "_vw")
        return width_percent (value);
    if (suffix == "_vh")
        return height_percent (value);

    return default_dim;
}

static Dimension gon_dim (Gon_Ref gon, Dimension _default = {})
{
    return string_to_dim (gon.StringView ({}), _default);
}

static Dims gon_dims (Gon_Ref gon, Dims _default = {})
{
    if (gon.size() != 4)
        return _default;

    Dims dims {};
    for (size_t i = 0; i < 4; ++i)
        dims[i] = string_to_dim (gon[i].StringView ({}), _default[i]);

    return dims;
}

static auto compute_dim (Dimension dim, const visage::Frame& parent)
{
    if (dim.type == Dimension::NONE)
        return 0.0f;
    return dim.amount * (dim.type == Dimension::WIDTH ? parent.width() : parent.height());
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

static File* gon_file (Gon_Ref gon, File_Allocator& file_alloc)
{
    const auto file_path = gon.StringView ({});
    if (file_path.empty())
        return {};

    return file_alloc.allocate<File> (file_path);
}

struct Image_Atlas;
struct JS_Engine;
using Frame_Allocator = Lifetime_Allocator<visage::Frame>;
struct Default_Params
{
    Frame_Allocator* frame_allocator {};
    File_Allocator* file_allocator {};
    ma_engine* audio_engine {};
    Image_Atlas* image_atlas {};
    JS_Engine* js_engine {};

    visage::Window* window {};
    visage::Frame* slideshow_frame {};
    GonObject header_params {};
    GonObject footer_params {};

    File* font {};
    File* code_font {};
    visage::Color background_color { 0xff33393f };
    visage::Color text_color { 0xffffffff };
    std::array<float, 2> aspect_ratio {};
};

static Default_Params gon_default_params (Gon_Ref gon, File_Allocator& file_alloc)
{
    Default_Params params {};
    params.file_allocator = &file_alloc;
    params.font = gon_file (gon["font"], file_alloc);
    params.code_font = gon_file (gon["code_font"], file_alloc);
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
