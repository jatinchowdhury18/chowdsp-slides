#pragma once

#include "slides_params.h"

#include <cassert>

namespace chowdsp::slides
{
// ---------------------------------------------------------------------------
// Custom_Audio_Source — mixin for custom frames that need to produce audio.
//
// Usage:
//   1. Inherit from both Content_Frame and Custom_Audio_Source.
//   2. Implement process_audio() to fill the output buffer with audio.
//   3. Call connect(*default_params.audio_engine) in your constructor.
//   4. Call disconnect() in your destructor.
//
// process_audio() is called on the audio thread. Do not allocate memory,
// take locks, or call visage/UI functions from inside it.
//
// Multiple Custom_Audio_Source objects can be connected simultaneously;
// miniaudio mixes them all together at the engine endpoint.
// ---------------------------------------------------------------------------
struct Custom_Audio_Source
{
    // -----------------------------------------------------------------------
    // C interop wrapper — miniaudio accesses this struct via a void* cast.
    // ma_data_source_base MUST be the first member.
    // -----------------------------------------------------------------------
    struct MA_Wrapper
    {
        ma_data_source_base base; // MUST be first
        Custom_Audio_Source* owner {};
        ma_uint32 channels {};
        ma_uint32 sample_rate {};
    };

    virtual ~Custom_Audio_Source() = default;

    // -----------------------------------------------------------------------
    // Optional hooks called when audio starts/stops.
    // Override these to reset phase accumulators, clear delay lines, etc.
    // Called on the UI thread, not the audio thread.
    // -----------------------------------------------------------------------
    virtual void on_audio_start() {}
    virtual void on_audio_stop() {}

    // -----------------------------------------------------------------------
    // Override this to produce audio.
    //
    //   frames_out    — buffer to fill with interleaved f32 samples.
    //                   May be nullptr (forward-seek); skip generation but
    //                   still advance any internal state by frame_count.
    //   frame_count   — number of frames requested.
    //   channels      — interleaved channel count (matches the engine).
    //   sample_rate   — sample rate in Hz (matches the engine).
    //   frames_written — set this to the number of frames actually written.
    //
    // Return MA_SUCCESS for continuous sources, or MA_AT_END to stop.
    // -----------------------------------------------------------------------
    virtual ma_result process_audio (float* frames_out,
                                     ma_uint32 frame_count,
                                     ma_uint32 channels,
                                     ma_uint32 sample_rate,
                                     ma_uint64* frames_written) noexcept = 0;

    // Wire this source into the engine's node graph.
    // Safe to call from the UI thread (e.g. your Content_Frame constructor).
    void connect (ma_engine& engine)
    {
        if (connected_)
            return;

        wrapper_.owner = this;
        wrapper_.channels = ma_engine_get_channels (&engine);
        wrapper_.sample_rate = ma_engine_get_sample_rate (&engine);

        auto ds_config = ma_data_source_config_init();
        ds_config.vtable = &s_vtable;
        auto result = ma_data_source_init (&ds_config, &wrapper_);
        assert (result == MA_SUCCESS);

        auto node_config = ma_data_source_node_config_init (&wrapper_);
        result = ma_data_source_node_init (ma_engine_get_node_graph (&engine),
                                           &node_config,
                                           nullptr,
                                           &node_);
        assert (result == MA_SUCCESS);

        result = ma_node_attach_output_bus (&node_, 0, ma_engine_get_endpoint (&engine), 0);
        assert (result == MA_SUCCESS);

        connected_ = true;
        on_audio_start();
    }

    // Remove this source from the node graph.
    // Blocks briefly until the audio thread finishes its current callback —
    // safe to call from the UI thread (e.g. your Content_Frame destructor).
    void disconnect()
    {
        if (! connected_)
            return;

        on_audio_stop();

        // ma_node_detach_output_bus() waits for the audio thread to finish
        // the current buffer before returning, so no extra synchronisation needed.
        ma_node_detach_output_bus (&node_, 0);
        ma_data_source_node_uninit (&node_, nullptr);
        ma_data_source_uninit (&wrapper_);

        connected_ = false;
    }

    bool is_connected() const noexcept { return connected_; }
    ma_uint32 channels() const noexcept { return wrapper_.channels; }
    ma_uint32 sample_rate() const noexcept { return wrapper_.sample_rate; }

private:
    MA_Wrapper wrapper_ {};
    ma_data_source_node node_ {};
    bool connected_ {};

    // ------------------------------------------------------------------
    // Miniaudio vtable callbacks — called on the audio thread.
    // ------------------------------------------------------------------
    static ma_result s_on_read (ma_data_source* ds,
                                void* frames_out,
                                ma_uint64 frame_count,
                                ma_uint64* frames_read)
    {
        auto* w = static_cast<MA_Wrapper*> (ds);
        // When frames_out is nullptr miniaudio is performing a forward seek.
        // Advance internal state but skip the write.
        if (frames_out == nullptr)
        {
            *frames_read = frame_count;
            return MA_SUCCESS;
        }
        return w->owner->process_audio (static_cast<float*> (frames_out),
                                        static_cast<ma_uint32> (frame_count),
                                        w->channels,
                                        w->sample_rate,
                                        frames_read);
    }

    static ma_result s_on_get_data_format (ma_data_source* ds,
                                           ma_format* fmt,
                                           ma_uint32* ch,
                                           ma_uint32* sr,
                                           ma_channel* channel_map,
                                           size_t channel_map_cap)
    {
        auto* w = static_cast<MA_Wrapper*> (ds);
        *fmt = ma_format_f32;
        *ch = w->channels;
        *sr = w->sample_rate;
        ma_channel_map_init_standard (ma_standard_channel_map_default,
                                      channel_map,
                                      channel_map_cap,
                                      *ch);
        return MA_SUCCESS;
    }

    inline static const ma_data_source_vtable s_vtable {
        s_on_read,            // onRead
        nullptr,              // onSeek          — not applicable for generators
        s_on_get_data_format, // onGetDataFormat
        nullptr,              // onGetCursor     — not applicable for generators
        nullptr,              // onGetLength     — not applicable for generators
        nullptr,              // onSetLooping
        0,                    // flags
    };
};
} // namespace chowdsp::slides
