#pragma once

#include "slides_content.h"

namespace chowdsp::slides
{
namespace code_tokenizer
{
    enum class Token_Type
    {
        DEFAULT,
        KEYWORD,
        IDENTIFIER,
        NUMBER,
        STRING,
        COMMENT,
        PREPROCESSOR,
        OPERATOR,
    };

    struct Code_Span
    {
        Token_Type type;
        uint32_t line;
        uint32_t column;
        uint32_t start_index;
        uint32_t length;
    };

    static constexpr std::string_view keyword_table[] = {
        "if",
        "else",
        "for",
        "while",
        "return",
        "struct",
        "class",
        "enum",
        "switch",
        "case",
        "break",
        "continue",
        "static",
        "const",
        "typedef",
        "namespace",
    };

    static bool is_keyword (std::string_view word)
    {
        for (auto k : keyword_table)
        {
            if (word == k)
                return true;
        }
        return false;
    }

    static bool is_identifier_start (char c)
    {
        return std::isalpha (c) || c == '_';
    }

    static bool is_identifier_char (char c)
    {
        return std::isalnum (c) || c == '_';
    }

    std::span<Code_Span> highlight_code (Allocator& arena, std::string_view source)
    {
        enum class Lex_State
        {
            NORMAL,
            STRING,
            BLOCK_COMMENT,
        };

        auto spans = arena.make_span<Code_Span> (source.size()); // this seems like it coulds be big?
        uint32_t span_count = 0;

        Lex_State state = Lex_State::NORMAL;
        uint32_t line = 0;
        uint32_t column = 0;

        for (size_t i = 0; i < source.size();)
        {
            char c = source[i];

            // Handle newlines: just advance line/column, no span
            if (c == '\n')
            {
                line++;
                column = 0;
                i++;
                continue;
            }

            uint32_t start_column = column;
            size_t start = i;

            switch (state)
            {
                case Lex_State::NORMAL:
                {
                    // Skip whitespace (spaces and tabs)
                    if (c == ' ' || c == '\t')
                    {
                        i++;
                        column++;
                        continue;
                    }

                    // Line comment
                    if (c == '/' && i + 1 < source.size() && source[i + 1] == '/')
                    {
                        start = i;
                        i += 2;
                        column += 2;
                        while (i < source.size() && source[i] != '\n')
                        {
                            i++;
                            column++;
                        }

                        spans[span_count++] = {
                            Token_Type::COMMENT,
                            line,
                            start_column,
                            (uint32_t) start,
                            (uint32_t) (i - start)
                        };
                        break;
                    }

                    // Block comment
                    if (c == '/' && i + 1 < source.size() && source[i + 1] == '*')
                    {
                        state = Lex_State::BLOCK_COMMENT;
                        start = i;
                        i += 2;
                        column += 2;
                        break;
                    }

                    // String literal
                    if (c == '"')
                    {
                        state = Lex_State::STRING;
                        start = i;
                        i++;
                        column++;
                        break;
                    }

                    // Identifier or keyword
                    if (is_identifier_start (c))
                    {
                        i++;
                        column++;
                        while (i < source.size() && is_identifier_char (source[i]))
                        {
                            i++;
                            column++;
                        }

                        std::string_view word = source.substr (start, i - start);
                        Token_Type type = is_keyword (word) ? Token_Type::KEYWORD : Token_Type::IDENTIFIER;

                        spans[span_count++] = {
                            type,
                            line,
                            start_column,
                            (uint32_t) start,
                            (uint32_t) (i - start)
                        };
                        break;
                    }

                    // Number literal
                    if (std::isdigit (c))
                    {
                        i++;
                        column++;
                        while (i < source.size() && std::isdigit (source[i]))
                        {
                            i++;
                            column++;
                        }

                        spans[span_count++] = {
                            Token_Type::NUMBER,
                            line,
                            start_column,
                            (uint32_t) start,
                            (uint32_t) (i - start)
                        };
                        break;
                    }

                    // Preprocessor
                    if (c == '#')
                    {
                        i++;
                        column++;
                        while (i < source.size() && source[i] != '\n')
                        {
                            i++;
                            column++;
                        }

                        spans[span_count++] = {
                            Token_Type::PREPROCESSOR,
                            line,
                            start_column,
                            (uint32_t) start,
                            (uint32_t) (i - start)
                        };
                        break;
                    }

                    // Everything else is treated as an operator
                    i++;
                    column++;
                    // if (! std::isspace (source[i - 1]))
                    {
                        spans[span_count++] = {
                            Token_Type::OPERATOR,
                            line,
                            start_column,
                            (uint32_t) start,
                            1
                        };
                    }
                    break;
                }

                case Lex_State::STRING:
                {
                    while (i < source.size())
                    {
                        if (source[i] == '"' && source[i - 1] != '\\')
                        {
                            i++; // include closing quote
                            column++;
                            break;
                        }
                        i++;
                        column++;
                    }

                    spans[span_count++] = {
                        Token_Type::STRING,
                        line,
                        start_column - 1,
                        (uint32_t) start - 1,
                        (uint32_t) (i - start + 1)
                    };
                    state = Lex_State::NORMAL;
                    break;
                }

                case Lex_State::BLOCK_COMMENT:
                {
                    while (i + 1 < source.size())
                    {
                        if (source[i] == '*' && source[i + 1] == '/')
                        {
                            i += 2;
                            column += 2;
                            break;
                        }
                        i++;
                        column++;
                    }

                    spans[span_count++] = {
                        Token_Type::COMMENT,
                        line,
                        start_column,
                        (uint32_t) start,
                        (uint32_t) (i - start)
                    };
                    state = Lex_State::NORMAL;
                    break;
                }

            } // switch
        }

        return spans.subspan (0, span_count);
    }
} // namespace code_tokenizer

//===================================================================
struct Code_View_Params
{
    File* code_file {};
};

static Code_View_Params gon_code_view_params (Gon_Ref gon, File_Allocator& file_allocator)
{
    return Code_View_Params {
        .code_file = gon_file (gon["code_file"], file_allocator),
    };
}

struct Code_View : Content_Frame
{
    Code_View_Params params {};
    std::string_view code_source {};
    std::span<code_tokenizer::Code_Span> code_spans {};

    Code_View (const Default_Params& def_params,
               Content_Frame_Params frame_params,
               Code_View_Params params)
        : Content_Frame { def_params, frame_params },
          params { params }
    {
        if (params.code_file != nullptr)
        {
            code_source = std::string_view { (const char*) params.code_file->data, params.code_file->size };
            code_spans = code_tokenizer::highlight_code (*default_params.frame_allocator, code_source);
        }
    }

    const visage::Color background_color { 0xff000000 };

    void draw (visage::Canvas& canvas) override
    {
        Content_Frame::draw (canvas);
        const auto alpha = fade_alpha();

        canvas.setColor (background_color.withAlpha (alpha));
        canvas.roundedRectangle (0, 0, width(), height(), height() * 0.05);

        const auto scale = dpiScale();
        const auto font_height = compute_dim (height_percent (5), *this);
        const auto font = visage::Font { font_height,
                                         default_params.code_font->data,
                                         (int) default_params.code_font->size }
                              .withDpiScale (scale);
        const auto line_height = 2 * font.lineHeight() / scale;
        const auto char_width = font.stringWidth ((const char32_t*) "a", 1) / scale;
        for (auto& span : code_spans)
        {
            float x = span.column * char_width;
            float y = span.line * line_height;
            const auto w = span.length * char_width;
            auto text = code_source.substr (span.start_index, span.length);

            using code_tokenizer::Token_Type;
            visage::Color code_color {};
            switch (span.type)
            {
                case Token_Type::IDENTIFIER:
                    code_color = 0xffff00ff;
                    break;
                case Token_Type::COMMENT:
                    code_color = 0xff898989;
                    break;
                case Token_Type::NUMBER:
                    code_color = 0xffff4d00;
                    break;
                case Token_Type::STRING:
                    code_color = 0xff0000ff;
                    break;
                case Token_Type::KEYWORD:
                    code_color = 0xff000fff;
                    break;
                case Token_Type::PREPROCESSOR:
                    code_color = 0xff00ffff;
                    break;
                case Token_Type::OPERATOR:
                case Token_Type::DEFAULT:
                default:
                    code_color = 0xffffffff;
                    break;
            }

            if (span.length == 1 && std::isspace (text[0]))
            {
                canvas.setColor (0x44ff0000);
                canvas.rectangle (x, y, w, line_height);
            }

            canvas.setColor (code_color.withAlpha (alpha));
            canvas.text (std::string { text },
                         font,
                         visage::Font::Justification::kCenter,
                         x,
                         y,
                         w,
                         line_height);
        }
    }
};
} // namespace chowdsp::slides
