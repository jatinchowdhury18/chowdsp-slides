#pragma once

#include <filesystem>
#include <visage/widgets.h>

#include "slides_content.h"

namespace chowdsp::slides
{
// @TODO: probably move this somewhere else...
struct Audio_Engine : ma_engine
{
    Audio_Engine()
    {
        auto result = ma_engine_init (nullptr, this);
        assert (result == MA_SUCCESS);
    }

    ~Audio_Engine()
    {
        ma_engine_uninit (this);
    }
};

struct Audio_Player_Params
{
    std::string_view file_path {};
    visage::Color background_color { 0xff181B1F };
    visage::Color label_color { 0xffffffff };
    std::string_view label {};
};

static Audio_Player_Params gon_audio_player_params (Gon_Ref gon, Allocator& allocator)
{
    return Audio_Player_Params {
        .file_path = allocator.copy_string (gon["file_path"].StringView ({})),
        .background_color = gon["background_color"].UInt (0xff181B1F),
        .label_color = gon["label_color"].Int (0xffffffff),
        .label = allocator.copy_string (gon["label"].StringView ({})),
    };
}

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
            const auto quarter_width = 0.25f * width();
            const auto round_width = 0.05f * width();
            if (! toggled())
            {
                canvas.roundedTriangle (
                    quarter_width,
                    quarter_width,
                    3 * quarter_width,
                    2 * quarter_width,
                    quarter_width,
                    3 * quarter_width,
                    round_width);
            }
            else
            {
                canvas.roundedRectangle (
                    quarter_width,
                    quarter_width,
                    2 * quarter_width,
                    2 * quarter_width,
                    round_width);
            }
        }
    } play_pause_button {};

    Audio_Player_Params player_params {};

    ma_sound sound {};

    static constexpr size_t thumbs_count = 64;
    std::array<std::array<float, 2>, thumbs_count> thumbs {};

    visage::EventTimer timer {};

    Audio_Player (const Default_Params& def_params, Content_Frame_Params frame_params, Audio_Player_Params params)
        : Content_Frame { def_params, frame_params },
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

        if (player_params.label_color.alpha() == 0.0f)
            player_params.label_color = default_params.text_color;

        if (player_params.label.empty())
        {
            // use the file name as the label
            const auto last_slash_pos = player_params.file_path.find_last_of ("/\\");
            player_params.label = player_params.file_path.substr (last_slash_pos + 1);
        }

        load_thumbnail();
        load_audio_file();
    }

    ~Audio_Player() override
    {
        ma_sound_uninit (&sound);
    }

    void load_thumbnail()
    {
        auto decoder_config = ma_decoder_config_init (ma_format_f32, 0, 0);
        ma_decoder decoder;
        auto result = ma_decoder_init_file (player_params.file_path.data(), &decoder_config, &decoder);
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
                return std::min (8.0f * sum / (float) frames_per_thumb, 1.0f);
            };

            thumbs[thumb_idx][0] = frame_avg (thumbs[thumb_idx][0]);
            if (channels > 1)
                thumbs[thumb_idx][1] = frame_avg (thumbs[thumb_idx][1]);
            else
                thumbs[thumb_idx][1] = thumbs[thumb_idx][0];
        }

        free (data_interleaved);
        ma_decoder_uninit (&decoder);
    }

    void load_audio_file()
    {
        auto result = ma_sound_init_from_file (default_params.audio_engine,
                                               player_params.file_path.data(),
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

    void resized() override
    {
        play_pause_button.setBounds (0.0f,
                                     0.8f * height(),
                                     0.2f * height(),
                                     0.2f * height());
    }

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

        play_pause_button.setAlphaTransparency (alpha);

        // background
        canvas.setColor (visage::Color { player_params.background_color }
                             .withAlpha (alpha));
        canvas.roundedRectangle (0, 0, width(), height(), height() * 0.05);

        // label
        const auto has_label = ! player_params.label.empty();
        if (has_label)
        {
            canvas.setColor (visage::Color { player_params.label_color }
                                 .withAlpha (alpha));
            canvas.text (std::string { player_params.label },
                         font (default_params, 0.12f * height()),
                         visage::Font::kCenter,
                         0.0f,
                         0.8f * height(),
                         width(),
                         0.2f * height());
        }

        // thumbnail
        float cursor, length;
        ma_sound_get_cursor_in_seconds (&sound, &cursor);
        ma_sound_get_length_in_seconds (&sound, &length);
        const auto thumbs_progress = static_cast<size_t> ((float) thumbs_count * cursor / length);
        const auto thumbs_progress_frac = ((float) thumbs_count * cursor / length) - (float) thumbs_progress;

        const auto pad = 0.04f * width();
        const auto h = 0.72f * height();
        const auto spacing = width() * 0.005f;
        const auto thumb_width = ((width() - 2.0f * pad) / (float) thumbs_count) - spacing;
        auto x = pad;
        const auto y_off = has_label ? (0.06f * height()) : 0.0f;
        for (size_t thumb_idx = 0; thumb_idx < thumbs_count; ++thumb_idx)
        {
            canvas.setColor (visage::Color { thumb_idx >= thumbs_progress ? 0xff4c4f52 : 0xff9978ee }
                                 .withAlpha (alpha));

            const auto thumb_height_above = 0.5f * h * thumbs[thumb_idx][0];
            const auto thumb_height_below = 0.5f * h * thumbs[thumb_idx][1];
            canvas.roundedRectangle (x,
                                     pad + 0.5f * h - thumb_height_above - y_off,
                                     thumb_width,
                                     thumb_height_above + thumb_height_below,
                                     spacing);

            if (thumb_idx == thumbs_progress)
            {
                canvas.setColor (visage::Color { 0xff9978ee }
                                     .withAlpha (alpha * thumbs_progress_frac));
                canvas.roundedRectangle (x,
                                         pad + 0.5f * h - thumb_height_above - y_off,
                                         thumb_width,
                                         thumb_height_above + thumb_height_below,
                                         spacing);
            }

            x += thumb_width + spacing;
        }
    }
};
} // namespace chowdsp::slides
