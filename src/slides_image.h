#pragma once

#include <filesystem>

#include "slides_content.h"

namespace chowdsp::slides
{
struct Image_Atlas : visage::ImageAtlas
{
    using ImageAtlas::ImageAtlas;
    std::mutex mutex {};
};

struct Image_Params
{
    Content_Frame_Params frame_params {};
    File* image_file {};
    std::array<float, 2> aspect_ratio {};

    std::string caption {};
    visage::Dimension caption_dim { 12_vh };
    visage::Color caption_color { 0xffffffff };
};

struct Image : Content_Frame
{
    Image_Params image_params;
    visage::ImageAtlas::PackedImage packed_image { {} };

    Image (Image_Params params)
        : Content_Frame { params.frame_params },
          image_params { params }
    {
    }

    ~Image() override
    {
        delete image_params.image_file;
    }

    std::array<float, 4> fit_and_center_image (float container_width,
                                               float container_height,
                                               float image_width,
                                               float image_height)
    {
        float container_aspect_ratio = container_width / container_height;
        float image_aspect_ratio = image_width / image_height;

        std::array<float, 4> result {};

        if (container_aspect_ratio > image_aspect_ratio)
        {
            // Container is wider than image
            result[3] = container_height;
            result[2] = container_height * image_aspect_ratio;
        }
        else
        {
            // Container is taller than image
            result[2] = container_width;
            result[3] = container_width / image_aspect_ratio;
        }

        // Center it
        result[0] = (container_width - result[2]) * 0.5f;
        result[1] = (container_height - result[3]) * 0.5f;

        return result;
    }

    void resized() override
    {
        auto image_height = height();
        if (! image_params.caption.empty())
        {
            const auto caption_height = compute_dim (image_params.caption_dim, *this);
            image_height -= caption_height;
        }

        if (image_params.image_file != nullptr)
        {
            const auto image_data = image_params.image_file->data;
            const auto image_data_size = image_params.image_file->size;

            float w = width();
            float h = image_height;
            if (image_params.aspect_ratio[0] > 0.0f)
            {
                const auto bounds = fit_and_center_image (width(),
                                                          image_height,
                                                          image_params.aspect_ratio[0],
                                                          image_params.aspect_ratio[1]);
                w = bounds[2];
                h = bounds[3];
            }
            visage::Image image { image_data,
                                  (int) image_data_size,
                                  static_cast<int> (std::round (w * dpiScale())),
                                  static_cast<int> (std::round (h * dpiScale())) };
            const std::lock_guard lock { frame_params.default_params->image_atlas->mutex };
            packed_image = frame_params.default_params->image_atlas->addImage (image);
        }
    }

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

        auto image_height = height();
        if (! image_params.caption.empty())
        {
            const auto caption_height = compute_dim (image_params.caption_dim, *this);
            image_height -= caption_height;
            canvas.setColor (image_params.caption_color.withAlpha (alpha));
            canvas.text (image_params.caption,
                         font (*frame_params.default_params, caption_height * 0.5f),
                         visage::Font::kCenter,
                         compute_dim (0_vw, *this),
                         image_height,
                         compute_dim (100_vw, *this),
                         caption_height);
        }

        if (image_params.image_file != nullptr)
        {
            canvas.setColor (visage::Color { 0xffffffff }.withAlpha (alpha));

            std::array<float, 4> bounds { 0, 0, width(), image_height };
            if (image_params.aspect_ratio[0] > 0.0f)
            {
                bounds = fit_and_center_image (width(),
                                               image_height,
                                               image_params.aspect_ratio[0],
                                               image_params.aspect_ratio[1]);
            }

            if (frame_params.default_params->image_atlas->mutex.try_lock())
            {
                canvas.addShape (
                    visage::ImageRefWrapper (
                        canvas.state()->clamp,
                        canvas.state()->brush,
                        canvas.state()->x + canvas.pixels (bounds[0]),
                        canvas.state()->y + canvas.pixels (bounds[1]),
                        canvas.pixels (bounds[2]),
                        canvas.pixels (bounds[3]),
                        packed_image,
                        frame_params.default_params->image_atlas));
                frame_params.default_params->image_atlas->mutex.unlock();
            }
            else
            {
                redraw();
            }
        }
    }
};
} // namespace chowdsp::slides
