#pragma once

#include "slides_content.h"

namespace chowdsp::slides
{
// Signature for a custom-frame factory function.
// Receives the shared Default_Params (with the arena allocator), the parsed
// Content_Frame_Params, and the raw GonObject for the content entry so the
// factory can pull any extra fields it needs (e.g. gon["params"]).
using Custom_Frame_Factory_Fn = Content_Frame* (*) (const Default_Params&,
                                                    Content_Frame_Params,
                                                    Gon_Ref);

// Lightweight registry populated at static-init time.
// Call register_type() (or use REGISTER_CUSTOM_FRAME)
// before the Slideshow is constructed;
struct Custom_Frame_Registry
{
    static constexpr size_t max_custom_types = 32;

    struct Entry
    {
        std::string_view type {};
        Custom_Frame_Factory_Fn factory {};
    };

    Entry entries[max_custom_types] {};
    size_t count = 0;

    void register_type (std::string_view type, Custom_Frame_Factory_Fn factory)
    {
        assert (count < max_custom_types && "Too many custom frame types registered");
        entries[count++] = { type, factory };
    }

    // Returns nullptr if no factory is registered for `type`.
    Content_Frame* make (std::string_view type,
                         const Default_Params& params,
                         Content_Frame_Params frame_params,
                         Gon_Ref gon) const
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (entries[i].type == type)
                return entries[i].factory (params, frame_params, gon);
        }
        return nullptr;
    }
};

// Singleton registry — safe to call before main() during static init.
inline Custom_Frame_Registry& global_custom_registry()
{
    static Custom_Frame_Registry registry;
    return registry;
}

// RAII helper that registers a factory at static-init time.
struct Custom_Frame_Registrar
{
    Custom_Frame_Registrar (std::string_view type, Custom_Frame_Factory_Fn factory)
    {
        global_custom_registry().register_type (type, factory);
    }
};

// ------------------------------------------------------------------
// REGISTER_CUSTOM_FRAME(type_string, FrameClass)
//
// Registers FrameClass to handle content entries whose "type" field
// matches type_string. FrameClass must expose a constructor:
//
//   FrameClass(const Default_Params&, Content_Frame_Params, Gon_Ref)
//
// The FrameClass is arena-allocated via the frame allocator, so its
// lifetime is tied to the Slideshow — no manual memory management needed.
//
// This macro must be invoked inside the same namespace as FrameClass
// (or at file scope when no namespace is used) so that the unqualified
// class name can form a valid C++ identifier for the static variable.
//
// Example (inside namespace chowdsp::slides, or a user namespace):
//   struct My_Frame : chowdsp::slides::Content_Frame { ... };
//   REGISTER_CUSTOM_FRAME("my_frame", My_Frame);
// ------------------------------------------------------------------
#define REGISTER_CUSTOM_FRAME(type_string, FrameClass)                            \
    static chowdsp::slides::Custom_Frame_Registrar _registrar_##FrameClass        \
    {                                                                             \
        type_string,                                                              \
            [] (const chowdsp::slides::Default_Params& _params,                   \
                chowdsp::slides::Content_Frame_Params _frame_params,              \
                chowdsp::slides::Gon_Ref _gon) -> chowdsp::slides::Content_Frame* \
        {                                                                         \
            return _params.frame_allocator->allocate<FrameClass> (                \
                _params, _frame_params, _gon);                                    \
        }                                                                         \
    }

} // namespace chowdsp::slides
