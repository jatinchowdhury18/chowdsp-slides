#pragma once

#include <unordered_set>

#include "slides_content.h"

namespace chowdsp::slides
{
struct Code_View_Params
{
    File* code_file {};
    Dimension font_size {};
    visage::Color background_color { 0xff181B1F };
    visage::Color code_color { 0xffffffff };
};

static Code_View_Params gon_code_view_params (Gon_Ref gon, File_Allocator& file_allocator)
{
    return Code_View_Params {
        .code_file = gon_file (gon["code_file"], file_allocator),
        .font_size = gon_dim (gon["font_size"], height_percent (4)),
        .background_color = gon["background_color"].UInt (0xff181B1F),
        .code_color = gon["code_color"].UInt (0xffffffff),
    };
}

struct Code_View : Content_Frame
{
    Code_View_Params params {};
    visage::TextEditor editor {};
    visage::Palette palette {};

    Code_View (const Default_Params& def_params,
               Content_Frame_Params frame_params,
               Code_View_Params params)
        : Content_Frame { def_params, frame_params },
          params { params }
    {
        addChild (editor);
        editor.setMultiLine (true);
        editor.setJustification (visage::Font::Justification::kTopLeft);
        palette.setColor (visage::TextEditor::TextEditorBackground, params.background_color);
        palette.setColor (visage::TextEditor::TextEditorText, params.code_color);
        editor.setPalette (&palette);

        if (params.code_file != nullptr)
        {
            const auto code_source = std::string_view { (const char*) params.code_file->data,
                                                        params.code_file->size };
            editor.setText (std::string { code_source });
        }
    }

    void resized() override
    {
        editor.setBounds (0, 0, width(), height());

        const auto font_height = compute_dim (params.font_size, *this);
        const auto font = visage::Font { font_height,
                                         default_params.code_font->data,
                                         (int) default_params.code_font->size };
        editor.setFont (font);

        const auto round_width = 0.05f * width();
        editor.setBackgroundRounding (round_width);
        editor.setMargin (round_width * 0.5f, round_width * 0.5f);
    }

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

        palette.setColor (visage::TextEditor::TextEditorBackground,
                          params.background_color.withAlpha (alpha));
        palette.setColor (visage::TextEditor::TextEditorText,
                          params.code_color.withAlpha (alpha));
    }
};
} // namespace chowdsp::slides
