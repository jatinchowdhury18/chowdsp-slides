#pragma once

#include <visage/app.h>
using namespace visage::dimension;

#include "slides_file.h"

namespace chowdsp::slides
{
struct Default_Params
{
    File* font {};
    visage::Color background_color { 0xff33393f };
    visage::Color text_color { 0xffffffff };
};

using Dims = std::array<visage::Dimension, 4>;
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
} // namespace chowdsp::slides
