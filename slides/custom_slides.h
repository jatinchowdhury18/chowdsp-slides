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
#include "slides_audio_source.h"
#include "slides_text.h"    // font(), draw_text(), etc.

// Put everything inside the chowdsp::slides namespace so you can use the
// helpers (font(), compute_dim(), etc.) without qualification.
namespace chowdsp::slides
{

// -----------------------------------------------------------------------
// Mini_Slider — a minimal horizontal slider for custom frames.
// -----------------------------------------------------------------------
struct Mini_Slider : visage::Frame
{
    double min_val {}, max_val { 1.0 }, value {};
    std::function<void (double)> on_change {};

    bool dragging {};
    float drag_start_x {};
    double drag_start_value {};



    void setRange (double min_v, double max_v)
    {
        min_val = min_v;
        max_val = max_v;
    }

    void setValue (double v)
    {
        value = std::clamp (v, min_val, max_val);
    }

    void mouseDown (const visage::MouseEvent& e) override
    {
        dragging = true;
        drag_start_x = e.position.x;
        drag_start_value = value;
        update_value_from_x (e.position.x);
    }

    void mouseDrag (const visage::MouseEvent& e) override
    {
        if (dragging)
            update_value_from_x (e.position.x);
    }

    void mouseUp (const visage::MouseEvent&) override
    {
        dragging = false;
    }

    void update_value_from_x (float x)
    {
        const auto w = width();
        if (w <= 0.0f)
            return;
        const auto norm = std::clamp (x / w, 0.0f, 1.0f);
        value = min_val + norm * (max_val - min_val);
        if (on_change)
            on_change (value);
        redraw();
    }

    void draw (visage::Canvas& canvas) override
    {
        // Track
        canvas.setColor (0xff2a2a2a);
        canvas.roundedRectangle (0, 0, width(), height(), height() * 0.5f);

        // Thumb
        const auto norm = (float) ((value - min_val) / (max_val - min_val));
        const auto thumb_x = norm * width();
        const auto thumb_r = height() * 0.6f;
        canvas.setColor (0xff44ccaa);
        canvas.circle (thumb_x, height() * 0.5f, thumb_r);
    }
};

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

// -----------------------------------------------------------------------
// Example: Sine_Oscillator
//
// Demonstrates Custom_Audio_Source. Inherits from both Content_Frame and
// Custom_Audio_Source to generate a sine tone while displaying a simple
// level meter that tracks the current output amplitude.
//
// Audio auto-connects when the frame is shown and disconnects when hidden.
// Frequency and gain are controllable via interactive sliders.
//
// process_audio() runs on the audio thread — keep it allocation-free.
// connect() / disconnect() are called automatically from show() / hide().
//
// .gon usage:
//   {
//       type = "sine_oscillator"
//       frame_params = { dims = ["10_vw", "20_vh", "30_vw", "30_vh"] }
//       params = {
//           frequency = 440.0    // Hz, defaults to A4
//           gain      = 0.25     // linear 0..1, defaults to 0.25
//       }
//   }
// -----------------------------------------------------------------------
struct Sine_Oscillator : Content_Frame, Custom_Audio_Source
{
    std::atomic<float> frequency { 440.0f };
    std::atomic<float> gain { 0.25f };

    // Shared between audio and UI threads — written atomically.
    std::atomic<float> current_amplitude { 0.0f };

    // Per-channel phase accumulators.
    double phase { 0.0 };

    // UI sliders
    Mini_Slider freq_slider {};
    Mini_Slider gain_slider {};

    Sine_Oscillator (const Default_Params& def_params,
                     Content_Frame_Params frame_params,
                     Gon_Ref gon)
        : Content_Frame { def_params, frame_params }
    {
        frequency.store ((float) gon["params"]["frequency"].Number (440.0), std::memory_order_relaxed);
        gain.store ((float) gon["params"]["gain"].Number (0.25), std::memory_order_relaxed);

        // Frequency slider: 50 Hz to 2000 Hz
        freq_slider.setRange (50.0, 2000.0);
        freq_slider.setValue (frequency.load (std::memory_order_relaxed));
        freq_slider.on_change = [this] (double val)
        {
            frequency.store ((float) val, std::memory_order_relaxed);
            redraw();
        };
        addChild (freq_slider);

        // Gain slider: 0.0 to 0.5 (prevent clipping when multiple oscillators play)
        gain_slider.setRange (0.0, 0.5);
        gain_slider.setValue (gain.load (std::memory_order_relaxed));
        gain_slider.on_change = [this] (double val)
        {
            gain.store ((float) val, std::memory_order_relaxed);
            redraw();
        };
        addChild (gain_slider);
    }

    // Reset phase when audio starts
    void on_audio_start() override
    {
        phase = 0.0;
    }

    void resized() override
    {
        const auto pad = 0.08f * width();
        const auto slider_height = 0.06f * height();
        const auto y_start = height() * 0.75f;
        const auto slider_width = width() - 2.0f * pad;

        freq_slider.setBounds (pad, y_start, slider_width, slider_height);
        gain_slider.setBounds (pad, y_start + slider_height + pad * 0.5f, slider_width, slider_height);
    }

    // --- Audio thread ---
    ma_result process_audio (float* frames_out,
                             ma_uint32 frame_count,
                             ma_uint32 ch,
                             ma_uint32 sr,
                             ma_uint64* frames_written) noexcept override
    {
        const auto freq = frequency.load (std::memory_order_relaxed);
        const auto g = gain.load (std::memory_order_relaxed);
        const double phase_inc = (2.0 * M_PI * freq) / sr;
        float peak = 0.0f;

        for (ma_uint32 i = 0; i < frame_count; ++i)
        {
            const auto sample = g * (float) std::sin (phase);
            for (ma_uint32 c = 0; c < ch; ++c)
                frames_out[i * ch + c] = sample;

            if (std::abs (sample) > peak)
                peak = std::abs (sample);

            phase += phase_inc;
            if (phase >= 2.0 * M_PI)
                phase -= 2.0 * M_PI;
        }

        current_amplitude.store (peak, std::memory_order_relaxed);
        *frames_written = frame_count;
        return MA_SUCCESS;
    }

    // --- UI thread ---
    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();
        if (alpha == 0.0f)
            return;

        freq_slider.setAlphaTransparency (alpha);
        gain_slider.setAlphaTransparency (alpha);

        // Background
        canvas.setColor (visage::Color { 0xff1b2a3b }.withAlpha (alpha));
        canvas.roundedRectangle (0, 0, width(), height(), height() * 0.06f);

        // Level bar
        const auto amp = current_amplitude.load (std::memory_order_relaxed);
        const auto pad = 0.08f * width();
        const auto bar_w = (width() - 2.0f * pad) * amp;
        canvas.setColor (visage::Color { 0xff44ccaa }.withAlpha (alpha));
        canvas.roundedRectangle (pad, height() * 0.45f, bar_w, height() * 0.08f, height() * 0.02f);

        // Frequency label
        if (default_params.font != nullptr)
        {
            const auto freq = frequency.load (std::memory_order_relaxed);
            const auto label = std::to_string ((int) std::round (freq)) + " Hz";
            canvas.setColor (visage::Color { 0xffffffff }.withAlpha (alpha));
            canvas.text (label,
                         font (default_params, height() * 0.15f),
                         visage::Font::kCenter,
                         0, 0, width(), height() * 0.4f);
        }

        redraw(); // keep ticking for the level meter
    }
};
REGISTER_CUSTOM_FRAME("sine_oscillator", Sine_Oscillator);

// -----------------------------------------------------------------------
// Add your own types below, following the same pattern:
//
//   struct My_Frame : Content_Frame { ... };
//   struct My_Audio_Frame : Content_Frame, Custom_Audio_Source { ... };
//   REGISTER_CUSTOM_FRAME("my_frame", My_Frame);
// -----------------------------------------------------------------------

} // namespace chowdsp::slides
