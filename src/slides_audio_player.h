#pragma once

#include <visage/widgets.h>

#include "slides_content.h"

namespace chowdsp::slides
{
struct Audio_Player_Params
{
    Content_Frame_Params frame_params {};
    std::string_view file_path {};
    visage::Color background_color { 0xff181B1F };
};

struct Audio_Player : Content_Frame
{
    struct Play_Pause_Button : visage::ToggleButton
    {
        Play_Pause_Button()
        {
            setToggleOnMouseDown (true);
        }

        void draw (visage::Canvas& canvas, float hover_amount) override
        {
            canvas.setColor (visage::Color { 0xffaaff88 }.withAlpha (0.5f + 0.5f * hover_amount));
            if (! toggled())
            {
                canvas.roundedTriangle (
                    compute_dim (25_vw, *this),
                    compute_dim (25_vw, *this),
                    compute_dim (75_vw, *this),
                    compute_dim (50_vw, *this),
                    compute_dim (25_vw, *this),
                    compute_dim (75_vw, *this),
                    compute_dim (5_vw, *this));
            }
            else
            {
                canvas.roundedRectangle (
                    compute_dim (25_vw, *this),
                    compute_dim (25_vw, *this),
                    compute_dim (50_vw, *this),
                    compute_dim (50_vw, *this),
                    compute_dim (5_vw, *this));
            }
        }
    } play_pause_button {};

    Audio_Player_Params player_params {};

    ma_sound sound {};

    static constexpr size_t thumbs_count = 64;
    std::array<std::array<float, 2>, thumbs_count> thumbs {};

    visage::EventTimer timer {};

    Audio_Player (Audio_Player_Params params)
        : Content_Frame { params.frame_params },
          player_params { params }
    {
        addChild (play_pause_button);
        play_pause_button.setAlphaTransparency (fade_alpha());

        timer.onTimerCallback() = [this]
        {
            const auto is_at_end = ma_sound_at_end (&sound);
            // const auto is_playing = ma_sound_is_playing (&sound);
            // const auto is_stopped = ! is_playing && is_at_end;
            if (play_pause_button.toggled() && is_at_end)
                play_pause_button.setToggledAndNotify (false);
            redraw();
        };
    }

    void set_default_params (Default_Params* default_params) override
    {
        Content_Frame::set_default_params (default_params);

        auto decoder_config = ma_decoder_config_init (ma_format_f32, 0, 0);
        ma_decoder decoder;
        auto result = ma_decoder_init_file ("assets/test.wav", &decoder_config, &decoder);
        assert (result == MA_SUCCESS);

        ma_uint64 frame_count;
        ma_decoder_get_available_frames (&decoder, &frame_count);
        const auto channels = decoder.outputChannels;

        auto* data_interleaved = (float*) malloc (frame_count * channels * sizeof (float));
        ma_uint64 frames_read;
        result = ma_decoder_read_pcm_frames (&decoder, data_interleaved, frame_count, &frames_read);
        assert (result == MA_SUCCESS);
        assert (frames_read == frame_count);

        const auto frames_per_thumb = frame_count / thumbs_count;

        for (size_t thumb_idx = 0; thumb_idx < thumbs_count; ++thumb_idx)
        {
            thumbs[thumb_idx][0] = thumbs[thumb_idx][1] = 0.0f;
            for (size_t sample_idx = thumb_idx * frames_per_thumb;
                 sample_idx < (thumb_idx + 1) * frames_per_thumb;
                 sample_idx++)
            {
                thumbs[thumb_idx][0] += std::abs (data_interleaved[sample_idx * channels]);
                if (channels > 1)
                    thumbs[thumb_idx][1] += std::abs (data_interleaved[sample_idx * channels + 1]);
            }

            const auto frame_avg = [frames_per_thumb] (float sum)
            {
                return std::min (10.0f * sum / (float) frames_per_thumb, 1.0f);
            };

            thumbs[thumb_idx][0] = frame_avg (thumbs[thumb_idx][0]);
            if (channels > 1)
                thumbs[thumb_idx][1] = frame_avg (thumbs[thumb_idx][1]);
            else
                thumbs[thumb_idx][1] = thumbs[thumb_idx][0];
        }

        free (data_interleaved);
        ma_decoder_uninit (&decoder);

        result = ma_sound_init_from_file (frame_params.default_params->audio_engine,
                                          "assets/test.wav",
                                          MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                          NULL,
                                          NULL,
                                          &sound);
        assert (result == MA_SUCCESS);

        play_pause_button.onToggle() = [this] (visage::Button*, bool toggle_state)
        {
            if (toggle_state)
            {
                ma_sound_start (&sound);

                timer.startTimer (25);
            }
            else
            {
                ma_sound_stop (&sound);
                ma_sound_seek_to_pcm_frame (&sound, 0);

                timer.stopTimer();
                redraw();
            }
        };
    }

    ~Audio_Player() override
    {
        ma_sound_uninit (&sound);
    }

    void resized() override
    {
        set_bounds ({ 0_vw, 80_vh, 20_vh, 20_vh }, play_pause_button, *this);
    }

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

        play_pause_button.setAlphaTransparency (alpha);

        canvas.setColor (visage::Color { player_params.background_color }
                             .withAlpha (alpha));
        canvas.roundedRectangle (0, 0, width(), height(), height() * 0.05);

        float cursor, length;
        ma_sound_get_cursor_in_seconds (&sound, &cursor);
        ma_sound_get_length_in_seconds (&sound, &length);
        const auto thumbs_progress = static_cast<size_t> ((float) thumbs_count * cursor / length);

        const auto pad = compute_dim (4_vw, *this);
        const auto height = compute_dim (72_vh, *this);
        const auto spacing = width() * 0.005f;
        const auto thumb_width = ((width() - 2.0f * pad) / (float) thumbs_count) - spacing;
        auto x = pad;
        for (size_t thumb_idx = 0; thumb_idx < thumbs_count; ++thumb_idx)
        {
            canvas.setColor (visage::Color { thumb_idx >= thumbs_progress ? 0xff4c4f52 : 0xff9978ee }.withAlpha (alpha));

            const auto thumb_height_above = 0.5f * height * thumbs[thumb_idx][0];
            const auto thumb_height_below = 0.5f * height * thumbs[thumb_idx][1];
            canvas.roundedRectangle (x,
                                     pad + 0.5f * height - thumb_height_above,
                                     thumb_width,
                                     thumb_height_above + thumb_height_below,
                                     spacing);
            x += thumb_width + spacing;
        }
    }
};
} // namespace chowdsp::slides
