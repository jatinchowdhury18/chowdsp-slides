// custom_slides.h — Define your custom Content_Frame types here.
//
// This file is automatically included by the slides runner when it exists in
// the slides directory. Add as many custom frame types as you like; each one
// just needs to be registered with REGISTER_CUSTOM_FRAME.
//
// -----------------------------------------------------------------------
// Quick-start:
//   1. Inherit from chowdsp::slides::Content_Frame.
//   2. Add a constructor:  MyFrame(const Default_Params&, Content_Frame_Params, Gon_Ref)
//   3. Override draw() and, optionally, resized() / next_step() / previous_step().
//   4. Call REGISTER_CUSTOM_FRAME("my_type", MyFrame) at file scope.
//   5. Reference the type in slides.gon:
//        { type = "my_type"  frame_params = { dims = [...] }  params = { ... } }
// -----------------------------------------------------------------------

#pragma once

#include "slides_custom.h"
#include "slides_text.h"

namespace chowdsp::slides
{

// -----------------------------------------------------------------------
// Example: Color_Card
//
// A simple frame that fills its bounds with a solid color and draws a
// centered label on top. Useful as a starting point for fully custom visuals.
//
// .gon usage:
//   {
//       type = "color_card"
//       frame_params = { dims = ["10_vw", "20_vh", "80_vw", "60_vh"] }
//       params = {
//           color     = 0xff1a73e8     // ARGB hex — defaults to accent blue
//           label     = "Hello!"
//           font_size = "6_vh"         // optional, defaults to 5_vh
//       }
//   }
// -----------------------------------------------------------------------
struct Color_Card : Content_Frame
{
    visage::Color color {};
    std::string_view label {};
    Dimension font_size {};

    Color_Card (const Default_Params& def_params,
                Content_Frame_Params frame_params,
                Gon_Ref gon)
        : Content_Frame { def_params, frame_params },
          color { (uint32_t) gon["params"]["color"].UInt (0xff1a73e8) },
          label { def_params.frame_allocator->copy_string (gon["params"]["label"].StringView ({})) },
          font_size { gon_dim (gon["params"]["font_size"], height_percent (5)) }
    {
    }

    void draw (visage::Canvas& canvas) override
    {
        // Always call the base first — it drives the fade animation.
        Content_Frame::draw (canvas);

        const auto alpha = fade_alpha();
        if (alpha == 0.0f)
            return;

        // Filled rounded rectangle in the chosen color.
        canvas.setColor (color.withAlpha (alpha));
        canvas.roundedRectangle (0, 0, width(), height(), height() * 0.06f);

        // Centered label.
        if (! label.empty() && default_params.font != nullptr)
        {
            const auto fsize = compute_dim (font_size, *default_params.slideshow_frame);
            canvas.setColor (visage::Color { 0xffffffff }.withAlpha (alpha));
            canvas.text (std::string { label },
                         font (default_params, fsize),
                         visage::Font::kCenter,
                         0, 0, width(), height());
        }
    }
};
REGISTER_CUSTOM_FRAME("color_card", Color_Card);
} // namespace chowdsp::slides
