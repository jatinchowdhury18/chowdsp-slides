#pragma once

#include <filesystem>

#include "slides_content.h"

namespace chowdsp::slides
{
struct Image_Params
{
    Content_Frame_Params frame_params {};
    File* image_file {};
    std::array<float, 2> aspect_ratio {};

    // @TODO: caption
};

struct Image : Content_Frame
{
    Image_Params image_params;

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

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

        if (image_params.image_file != nullptr)
        {
            canvas.setColor (visage::Color { 0xffffffff }.withAlpha (alpha));
            const auto image_data = image_params.image_file->data;
            const auto image_data_size = image_params.image_file->size;

            if (image_params.aspect_ratio[0] > 0.0f)
            {
                const auto bounds = fit_and_center_image (width(),
                                                          height(),
                                                          image_params.aspect_ratio[0],
                                                          image_params.aspect_ratio[1]);
                canvas.image (image_data,
                              image_data_size,
                              bounds[0],
                              bounds[1],
                              bounds[2],
                              bounds[3]);
            }
            else
            {
                canvas.image (image_data,
                              image_data_size,
                              0,
                              0,
                              width(),
                              height());
            }
        }
    }
};
} // namespace chowdsp::slides
