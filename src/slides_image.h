#pragma once

#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#include "slides_content.h"
#include "slides_url.h"

namespace chowdsp::slides
{
struct Image_Atlas : visage::ImageAtlas
{
    using ImageAtlas::ImageAtlas;
    std::mutex mutex {};
};

struct Image_Params
{
    File* image_file {};
    std::array<float, 2> aspect_ratio {};

    std::string caption {};
    visage::Dimension caption_dim { 12_vh };
    visage::Color caption_color { 0xffffffff };
    std::string link_url {};
};

static void merge_params (Image_Params& image, const Default_Params& params)
{
    if (image.caption_color.alpha() == 0.0f)
        image.caption_color = params.text_color;
}

static std::array<float, 2> get_aspect_ratio (const File* file)
{
    if (file == nullptr)
        return {};

    int width, height, channels;
    if (stbi_info_from_memory (file->data, (int) file->size, &width, &height, &channels))
    {
        return { (float) width, (float) height };
    }

    return {};
}

static Image_Params gon_image_params (Gon_Ref gon)
{
    const auto file = gon_file (gon["file_path"]);
    return Image_Params {
        .image_file = file,
        .aspect_ratio = get_aspect_ratio (file),
        .caption = gon["caption"].String ({}),
        .caption_dim = gon_dim (gon["caption_dim"], 12_vh),
        .caption_color = gon["caption_color"].Int ({}),
        .link_url = gon["link_url"].String ({}),
    };
}

struct Image : Content_Frame
{
    Image_Params image_params;
    visage::ImageAtlas::PackedImage packed_image { {} };

    Image (Content_Frame_Params frame_params, Image_Params params)
        : Content_Frame { frame_params },
          image_params { params }
    {
    }

    ~Image() override
    {
        delete image_params.image_file;
    }

    void set_default_params (Default_Params* default_params) override
    {
        Content_Frame::set_default_params (default_params);
        merge_params (image_params, *default_params);
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
                const auto bounds = fit_and_center (width(),
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
                bounds = fit_and_center (width(),
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

    bool is_mouse_on_image (const visage::MouseEvent& e) const
    {
        auto image_height = height();
        if (! image_params.caption.empty())
        {
            const auto caption_height = compute_dim (image_params.caption_dim, *this);
            image_height -= caption_height;
        }

        std::array<float, 4> bounds { 0, 0, width(), image_height };
        if (image_params.aspect_ratio[0] > 0.0f)
        {
            bounds = fit_and_center (width(),
                                     image_height,
                                     image_params.aspect_ratio[0],
                                     image_params.aspect_ratio[1]);
        }

        const auto is_in_x_range = e.position.x >= bounds[0] && e.position.x <= (bounds[0] + bounds[2]);
        const auto is_in_y_range = e.position.y >= bounds[1] && e.position.y <= (bounds[1] + bounds[3]);
        return is_in_x_range && is_in_y_range;
    }

    bool can_click_url (const visage::MouseEvent& e) const
    {
        return is_mouse_on_image (e) && ! image_params.link_url.empty();
    }

    void mouseEnter (const visage::MouseEvent& e) override
    {
        mouseMove (e);
    }

    void mouseMove (const visage::MouseEvent& e) override
    {
        setCursorStyle (can_click_url (e) ? visage::MouseCursor::Pointing : visage::MouseCursor::Arrow);
    }

    void mouseExit (const visage::MouseEvent&) override
    {
        setCursorStyle (visage::MouseCursor::Arrow);
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        if (can_click_url (e))
            launch_url (image_params.link_url);
    }
};
} // namespace chowdsp::slides
