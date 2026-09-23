#pragma once

#include "slides_code_highlight.h"
#include "slides_content.h"

namespace chowdsp::slides
{
// ---- Params ----

struct Code_View_Params
{
    File* code_file {};
    Dimension font_size {};
    visage::Color background_color { 0xff181B1F };
    visage::Color code_color { 0xffffffff };
    Dimension padding { height_percent (2.0) };
    Code_Lang lang { Code_Lang::None };
    Syntax_Highlight_Params highlight_params {};
};

static Code_Lang parse_lang (std::string_view s) noexcept
{
    if (s == "cpp" || s == "c")         return Code_Lang::CPP;
    if (s == "python" || s == "py")     return Code_Lang::Python;
    if (s == "js" || s == "javascript") return Code_Lang::JS;
    if (s == "jai")                     return Code_Lang::Jai;
    return Code_Lang::None;
}

static Code_View_Params gon_code_view_params (Gon_Ref gon, File_Allocator& file_allocator)
{
    const auto* theme_ptr = find_theme (gon["theme"].StringView ({}));
    const auto& t = theme_ptr ? *theme_ptr : themes::handmade_hero;

    return Code_View_Params {
        .code_file = gon_file (gon["code_file"], file_allocator),
        .font_size = gon_dim (gon["font_size"], height_percent (3)),
        .background_color = gon["background_color"].UInt (t.background_color),
        .code_color = gon["code_color"].UInt (t.code_color),
        .padding = gon_dim (gon["padding"], height_percent (2.0)),
        .lang = parse_lang (gon["lang"].StringView ({})),
        .highlight_params = {
            .keyword_color      = gon["keyword_color"].UInt (t.keyword_color),
            .comment_color      = gon["comment_color"].UInt (t.comment_color),
            .string_color       = gon["string_color"].UInt (t.string_color),
            .number_color       = gon["number_color"].UInt (t.number_color),
            .preprocessor_color = gon["preprocessor_color"].UInt (t.preprocessor_color),
        },
    };
}

// ---- Syntax-highlighting TextEditor subclass ----
// Overrides draw() to render each token with its own color.
// Falls back to the base TextEditor when tokens is empty (unknown/no language).

struct Syntax_Text_Editor : public visage::TextEditor
{
    std::span<const Syntax_Token> tokens {};
    visage::Color default_color { 0xffffffff };
    Syntax_Highlight_Params highlight {};
    float alpha = 1.0f;

    bool keyPress (const visage::KeyEvent& key) override
    {
        using KC = visage::KeyCode;
        const auto code  = key.keyCode();
        const bool mod   = key.isMainModifier();
        const bool shift = key.isShiftDown();

        // Block every key that would mutate the text
        if (code == KC::Backspace || code == KC::Delete)  return true;
        if (code == KC::Return)                            return true;
        if (mod   && (code == KC::V || code == KC::X))     return true; // paste / cut
        if (shift && code == KC::Insert)                   return true; // Shift+Ins paste
        if (shift && code == KC::Delete)                   return true; // Shift+Del cut
        if (mod   && code == KC::Z)                        return true; // undo
        if (mod   && code == KC::Y)                        return true; // redo

        return visage::TextEditor::keyPress (key);
    }

    visage::Color color_for_type (Syntax_Token_Type t) const noexcept
    {
        switch (t)
        {
            case Syntax_Token_Type::Keyword:        return highlight.keyword_color;
            case Syntax_Token_Type::Comment:        return highlight.comment_color;
            case Syntax_Token_Type::String_Literal: return highlight.string_color;
            case Syntax_Token_Type::Number:         return highlight.number_color;
            case Syntax_Token_Type::Preprocessor:   return highlight.preprocessor_color;
            default:                                return default_color;
        }
    }

    void draw (visage::Canvas& canvas) override
    {
        if (tokens.empty())
        {
            // No tokens: fall back to base single-color rendering.
            visage::TextEditor::draw (canvas);
            return;
        }

        drawBackground (canvas);

        if (hasKeyboardFocus())
            drawSelection (canvas);

        if (text().isEmpty())
            return;

        const float y_margin = yMargin();
        const float line_h   = font().lineHeight();
        const float y_scroll = yPosition();
        const float view_h   = (float) height();
        const auto& full_str = text();

        canvas.setPosition (0.0f, y_margin);

        for (const auto& tok : tokens)
        {
            if (tok.end <= tok.start)
                continue;

            // Skip lone newlines — invisible, no glyph to draw
            if (tok.end == tok.start + 1 && full_str[tok.start] == U'\n')
                continue;

            const auto [tx, ty] = indexToPosition (tok.start);
            const float draw_y  = ty - y_margin - y_scroll;

            // Cull tokens outside the visible area
            if (draw_y + line_h < 0.0f || draw_y > view_h)
                continue;

            const auto tok_str = full_str.substring ((size_t) tok.start,
                                                      (size_t) (tok.end - tok.start));
            const float tw = font().stringWidth (tok_str.c_str(), (int) tok_str.length());

            canvas.setColor (color_for_type (tok.type).withAlpha (alpha));
            auto* stored = canvas.getText (tok_str, font(), visage::Font::kTopLeft);
            canvas.text (stored, tx, draw_y, tw, line_h);
        }
    }
};

// ---- Code_View ----

struct Code_View : Content_Frame
{
    Code_View_Params params {};
    Syntax_Text_Editor editor {};
    visage::Palette palette {};

    Code_View (const Default_Params& def_params,
               Content_Frame_Params frame_params,
               Code_View_Params params)
        : Content_Frame { def_params, frame_params },
          params { params }
    {
        addChild (editor);
        editor.scrollBar().setVisible (false); // slides aren't meant to be scrolled
        editor.setMultiLine (true);
        editor.setJustification (visage::Font::Justification::kTopLeft);
        palette.setColor (visage::TextEditor::TextEditorBackground, params.background_color);
        palette.setColor (visage::TextEditor::TextEditorText, params.code_color);
        editor.setPalette (&palette);

        if (params.code_file != nullptr)
        {
            auto code_source = std::string_view { (const char*) params.code_file->data,
                                                  params.code_file->size };
            // drop the file's trailing newline(s), which would show as an empty last line
            while (! code_source.empty() && (code_source.back() == '\n' || code_source.back() == '\r'))
                code_source.remove_suffix (1);
            editor.setText (std::string { code_source });
            editor.tokens = tokenize_for_lang (code_source, params.lang,
                                               *def_params.frame_allocator);
        }

        editor.default_color = params.code_color;
        editor.highlight     = params.highlight_params;
    }

    void resized() override
    {
        editor.setBounds (0, 0, width(), height());

        const auto font_height = compute_dim (params.font_size, *default_params.slideshow_frame);
        const auto code_font = visage::Font { font_height,
                                              default_params.code_font->data,
                                              (int) default_params.code_font->size };
        editor.setFont (code_font);
        editor.setYPosition (0.0f); // if the code doesn't fit, show the top of it

        const auto round_width = compute_dim (params.padding, *default_params.slideshow_frame);
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

        editor.alpha = alpha;
    }
};
} // namespace chowdsp::slides
