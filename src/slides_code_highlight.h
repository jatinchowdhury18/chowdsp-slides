#pragma once

#include <algorithm>
#include <span>
#include <string_view>

#include <visage/app.h>

#include "slides_allocator.h"

namespace chowdsp::slides
{
// ---- Token types ----

enum class Syntax_Token_Type
{
    Default,
    Keyword,
    Comment,
    String_Literal,
    Number,
    Preprocessor,
};

struct Syntax_Token
{
    int start; // char32_t index (== byte index for ASCII source)
    int end;
    Syntax_Token_Type type;
};

// ---- Per-language highlight colors ----

struct Syntax_Highlight_Params
{
    visage::Color keyword_color      { 0xffC678DD }; // purple
    visage::Color comment_color      { 0xff7F848E }; // gray
    visage::Color string_color       { 0xff98C379 }; // green
    visage::Color number_color       { 0xffD19A66 }; // orange
    visage::Color preprocessor_color { 0xffE06C75 }; // red/pink
};

// ---- Named color themes ----
// Colors are stored as 0xAARRGGBB uint32, matching visage::Color's constructor.
// Source: Focus editor theme files (https://github.com/focus-editor/focus).
// Focus files use RRGGBBAA order; values below are converted to 0xAARRGGBB.

struct Code_Theme
{
    uint32_t background_color   {};
    uint32_t code_color         {};
    uint32_t keyword_color      {};
    uint32_t comment_color      {};
    uint32_t string_color       {};
    uint32_t number_color       {};
    uint32_t preprocessor_color {};
};

namespace themes
{
    // One Dark-ish (renamed; use "one-dark" in GON)
    static constexpr Code_Theme one_dark = {
        .background_color   = 0xFF181B1F,
        .code_color         = 0xFFFFFFFF,
        .keyword_color      = 0xFFC678DD,
        .comment_color      = 0xFF7F848E,
        .string_color       = 0xFF98C379,
        .number_color       = 0xFFD19A66,
        .preprocessor_color = 0xFFE06C75,
    };

    // Focus: gruvbox-flat  (dark)
    // background0=32302F  code_default=D4BE98  code_keyword=D79921
    // code_comment=7C6F64  code_string_literal=A9B665  code_number=D699B5
    // code_macro=E0AD82
    static constexpr Code_Theme gruvbox_flat = {
        .background_color   = 0xFF32302F,
        .code_color         = 0xFFD4BE98,
        .keyword_color      = 0xFFD79921,
        .comment_color      = 0xFF7C6F64,
        .string_color       = 0xFFA9B665,
        .number_color       = 0xFFD699B5,
        .preprocessor_color = 0xFFE0AD82,
    };

    // Focus: basic-light  (light)
    // background0=FFFFFF  code_default=181818  code_keyword=0000FF
    // code_comment=416529  code_string_literal=871C1D  code_number=0000FF
    // code_directive=8D5C0F
    static constexpr Code_Theme basic_light = {
        .background_color   = 0xFFFFFFFF,
        .code_color         = 0xFF181818,
        .keyword_color      = 0xFF0000FF,
        .comment_color      = 0xFF416529,
        .string_color       = 0xFF871C1D,
        .number_color       = 0xFF0000FF,
        .preprocessor_color = 0xFF8D5C0F,
    };

    // Focus: focus  (dark, from default.focus-config [[style]])
    // background0=15212A  code_default=BFC9DB  code_keyword=E67D74
    // code_comment=87919D  code_string_literal=D4BC7D  code_number=D699B5
    // code_macro=E0AD82
    static constexpr Code_Theme focus = {
        .background_color   = 0xFF15212A,
        .code_color         = 0xFFBFC9DB,
        .keyword_color      = 0xFFE67D74,
        .comment_color      = 0xFF87919D,
        .string_color       = 0xFFD4BC7D,
        .number_color       = 0xFFD699B5,
        .preprocessor_color = 0xFFE0AD82,
    };

    // Focus: handmade-hero  (dark, Casey Muratori's emacs theme)
    // background0=161616  code_default=CDAA7D  code_keyword=B8860B
    // code_comment=7F7F7F  code_string_literal=6B8E23  code_number=D699B5
    // code_macro=E0AD82
    static constexpr Code_Theme handmade_hero = {
        .background_color   = 0xFF161616,
        .code_color         = 0xFFCDAA7D,
        .keyword_color      = 0xFFB8860B,
        .comment_color      = 0xFF7F7F7F,
        .string_color       = 0xFF6B8E23,
        .number_color       = 0xFFD699B5,
        .preprocessor_color = 0xFFE0AD82,
    };

    // Focus: tokyo-night  (dark)
    // background0=1A1B26  code_default=A9B1D6  code_keyword=BB9AF7
    // code_comment=565F89  code_string_literal=9ECE6A  code_number=FF9E64
    // code_macro=89DDFF
    static constexpr Code_Theme tokyo_night = {
        .background_color   = 0xFF1A1B26,
        .code_color         = 0xFFA9B1D6,
        .keyword_color      = 0xFFBB9AF7,
        .comment_color      = 0xFF565F89,
        .string_color       = 0xFF9ECE6A,
        .number_color       = 0xFFFF9E64,
        .preprocessor_color = 0xFF89DDFF,
    };
} // namespace themes

static const Code_Theme* find_theme (std::string_view name) noexcept
{
    if (name == "one-dark")       return &themes::one_dark;
    if (name == "gruvbox-flat")   return &themes::gruvbox_flat;
    if (name == "basic-light")    return &themes::basic_light;
    if (name == "focus")          return &themes::focus;
    if (name == "handmade-hero")  return &themes::handmade_hero;
    if (name == "tokyo-night")    return &themes::tokyo_night;
    return nullptr;
}

// ---- Language enum ----

enum class Code_Lang { None, CPP, Python, JS, Jai };

// ---- Tokenizer config ----
// Drives all language differences; keyword lookup is a linear scan over
// a constexpr table so no heap allocation is needed.

struct Tokenizer_Config
{
    std::span<const std::string_view> keywords {};
    bool slash_line_comment  = false; // C++/JS/Jai: //
    bool slash_block_comment = false; // C++/JS/Jai: /* */
    bool hash_preprocessor   = false; // C++: whole line after # → Preprocessor
    bool hash_line_comment   = false; // Python: whole line after # → Comment
    bool hash_word_directive = false; // Jai: #identifier only → Preprocessor
    bool triple_strings      = false; // Python: """ / '''
    bool backtick_string     = false; // JS: `template literals` (may span lines)
};

// ---- Keyword tables (constexpr, no heap) ----

static constexpr std::string_view cpp_kw[] = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto",
    "bitand", "bitor", "bool", "break",
    "case", "catch", "char", "char8_t", "char16_t", "char32_t",
    "class", "compl", "concept", "const", "const_cast", "consteval",
    "constexpr", "constinit", "continue", "co_await", "co_return", "co_yield",
    "decltype", "default", "delete", "do", "double", "dynamic_cast",
    "else", "enum", "explicit", "export", "extern",
    "false", "float", "for", "friend",
    "goto", "if", "inline", "int",
    "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
    "nullptr", "operator", "or", "or_eq", "override",
    "private", "protected", "public",
    "register", "reinterpret_cast", "requires", "return",
    "short", "signed", "sizeof", "static", "static_assert", "static_cast",
    "struct", "switch",
    "template", "this", "thread_local", "throw", "true", "try",
    "typedef", "typeid", "typename",
    "union", "unsigned", "using",
    "virtual", "void", "volatile",
    "wchar_t", "while", "xor", "xor_eq",
};

static constexpr std::string_view python_kw[] = {
    "False", "None", "True",
    "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del",
    "elif", "else", "except", "finally", "for",
    "from", "global", "if", "import", "in",
    "is", "lambda", "nonlocal", "not", "or",
    "pass", "raise", "return", "try", "while",
    "with", "yield",
};

// Keywords, types, and values from Focus's js.jai
static constexpr std::string_view js_kw[] = {
    "as", "async", "abstract", "arguments", "await",
    "break", "case", "catch", "class", "const", "continue",
    "debugger", "default", "delete", "do",
    "else", "enum", "eval", "export", "extends",
    "finally", "for", "from", "function",
    "if", "import", "in", "instanceof", "interface",
    "keyof", "let", "new", "of",
    "private", "protected", "public", "return",
    "static", "super", "switch",
    "throw", "try", "type", "typeof",
    "var", "while", "with", "yield",
    // types
    "any", "bigint", "boolean", "number", "object", "string", "unknown", "void",
    // values
    "false", "null", "this", "true", "undefined",
};

// Keywords, types, and values from Focus's jai.jai
static constexpr std::string_view jai_kw[] = {
    "break", "case", "cast", "code_of", "continue", "defer",
    "else", "enum", "enum_flags", "for",
    "if", "ifx", "initializer_of", "inline", "interface",
    "is_constant", "no_inline", "operator",
    "push_context", "remove", "return",
    "size_of", "struct", "then", "type_info", "type_of",
    "union", "using", "while", "xx",
    // types
    "Any", "Code", "Type",
    "bool", "float", "float32", "float64", "int", "string", "void",
    "s8", "s16", "s32", "s64", "u8", "u16", "u32", "u64",
    // values
    "context", "false", "it", "it_index", "null", "temp", "true",
};

// ---- Language config factories ----

static Tokenizer_Config cpp_tokenizer_config()
{
    return { .keywords = cpp_kw, .slash_line_comment = true,
             .slash_block_comment = true, .hash_preprocessor = true };
}

static Tokenizer_Config python_tokenizer_config()
{
    return { .keywords = python_kw, .hash_line_comment = true, .triple_strings = true };
}

static Tokenizer_Config js_tokenizer_config()
{
    return { .keywords = js_kw, .slash_line_comment = true,
             .slash_block_comment = true, .backtick_string = true };
}

static Tokenizer_Config jai_tokenizer_config()
{
    return { .keywords = jai_kw, .slash_line_comment = true,
             .slash_block_comment = true, .hash_word_directive = true };
}

// ---- Character helpers ----

static bool is_ident_start (char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_ident_cont (char c) noexcept
{
    return is_ident_start (c) || (c >= '0' && c <= '9');
}

static bool is_digit (char c) noexcept { return c >= '0' && c <= '9'; }

static bool is_hex_digit (char c) noexcept
{
    return is_digit (c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// ---- Core tokenizer ----
// Assumes ASCII source (byte index == char32_t index).
// Tokens are confined to single lines; multi-line constructs are split at '\n'
// with the newline emitted as a Default token.

static std::span<const Syntax_Token> tokenize (std::string_view code,
                                                const Tokenizer_Config& cfg,
                                                Allocator& alloc)
{
    const int n = (int) code.size();
    if (n == 0)
        return {};

    // Upper bound: one token per source character.
    auto buf   = alloc.make_span<Syntax_Token> (n);
    int  count = 0;

    auto push = [&] (int s, int e, Syntax_Token_Type t)
    {
        buf[count++] = { s, e, t };
    };

    // Split a possibly-multi-line span at '\n' boundaries, emitting per-line
    // segments with `type` and lone Default tokens for each newline.
    auto emit_multiline = [&] (int from, int to, Syntax_Token_Type type)
    {
        int seg = from;
        for (int k = from; k < to; ++k)
        {
            if (code[k] == '\n')
            {
                if (k > seg)
                    push (seg, k, type);
                push (k, k + 1, Syntax_Token_Type::Default);
                seg = k + 1;
            }
        }
        if (to > seg)
            push (seg, to, type);
    };

    int i = 0;
    while (i < n)
    {
        const char c = code[i];

        // Newline
        if (c == '\n')
        {
            push (i, i + 1, Syntax_Token_Type::Default);
            ++i;
            continue;
        }

        // Hash: three different modes depending on language
        if (c == '#')
        {
            if (cfg.hash_preprocessor || cfg.hash_line_comment)
            {
                // C++/Python: consume the whole line
                int j = i;
                while (j < n && code[j] != '\n')
                    ++j;
                push (i, j, cfg.hash_preprocessor ? Syntax_Token_Type::Preprocessor
                                                   : Syntax_Token_Type::Comment);
                i = j;
            }
            else if (cfg.hash_word_directive)
            {
                // Jai: consume #identifier only
                int j = i + 1;
                while (j < n && is_ident_cont (code[j]))
                    ++j;
                push (i, j, Syntax_Token_Type::Preprocessor);
                i = j;
            }
            else
            {
                push (i, i + 1, Syntax_Token_Type::Default);
                ++i;
            }
            continue;
        }

        // Slash-slash line comment
        if (cfg.slash_line_comment && c == '/' && i + 1 < n && code[i + 1] == '/')
        {
            int j = i;
            while (j < n && code[j] != '\n')
                ++j;
            push (i, j, Syntax_Token_Type::Comment);
            i = j;
            continue;
        }

        // Slash-star block comment
        if (cfg.slash_block_comment && c == '/' && i + 1 < n && code[i + 1] == '*')
        {
            int j = i + 2;
            while (j < n && !(code[j] == '*' && j + 1 < n && code[j + 1] == '/'))
                ++j;
            if (j < n)
                j += 2; // consume '*/'
            emit_multiline (i, j, Syntax_Token_Type::Comment);
            i = j;
            continue;
        }

        // Backtick template literals (JavaScript) — may span multiple lines
        if (cfg.backtick_string && c == '`')
        {
            int j = i + 1;
            while (j < n && code[j] != '`')
            {
                if (code[j] == '\\')
                    ++j;
                ++j;
            }
            if (j < n && code[j] == '`')
                ++j;
            emit_multiline (i, j, Syntax_Token_Type::String_Literal);
            i = j;
            continue;
        }

        // Triple-quoted strings (Python): """ and '''
        if (cfg.triple_strings)
        {
            const char q = c;
            if ((q == '"' || q == '\'') && i + 2 < n && code[i + 1] == q && code[i + 2] == q)
            {
                int j = i + 3;
                while (j + 2 < n && !(code[j] == q && code[j + 1] == q && code[j + 2] == q))
                    ++j;
                j = (j + 2 < n) ? j + 3 : n; // consume closing ''' or reach EOF
                emit_multiline (i, j, Syntax_Token_Type::String_Literal);
                i = j;
                continue;
            }
        }

        // Single/double-quoted string literals (single line)
        if (c == '"' || c == '\'')
        {
            int j = i + 1;
            while (j < n && code[j] != c && code[j] != '\n')
            {
                if (code[j] == '\\')
                    ++j; // skip escaped char
                ++j;
            }
            if (j < n && code[j] == c)
                ++j;
            push (i, j, Syntax_Token_Type::String_Literal);
            i = j;
            continue;
        }

        // Number literals
        if (is_digit (c) || (c == '.' && i + 1 < n && is_digit (code[i + 1])))
        {
            int j = i;
            if (c == '0' && i + 1 < n && (code[i + 1] == 'x' || code[i + 1] == 'X'))
            {
                j += 2;
                while (j < n && is_hex_digit (code[j]))
                    ++j;
            }
            else
            {
                while (j < n && is_digit (code[j]))
                    ++j;
                if (j < n && code[j] == '.')
                {
                    ++j;
                    while (j < n && is_digit (code[j]))
                        ++j;
                }
                if (j < n && (code[j] == 'e' || code[j] == 'E'))
                {
                    ++j;
                    if (j < n && (code[j] == '+' || code[j] == '-'))
                        ++j;
                    while (j < n && is_digit (code[j]))
                        ++j;
                }
            }
            // Numeric suffixes: C++ (u, l, f) and Python imaginary (j)
            while (j < n && (code[j] == 'u' || code[j] == 'U' || code[j] == 'l' ||
                              code[j] == 'L' || code[j] == 'f' || code[j] == 'F' ||
                              code[j] == 'j' || code[j] == 'J'))
                ++j;
            push (i, j, Syntax_Token_Type::Number);
            i = j;
            continue;
        }

        // Identifier or keyword
        if (is_ident_start (c))
        {
            int j = i;
            while (j < n && is_ident_cont (code[j]))
                ++j;
            const auto word = code.substr (i, j - i);
            const bool is_kw = !cfg.keywords.empty() &&
                                std::find (cfg.keywords.begin(), cfg.keywords.end(), word) !=
                                    cfg.keywords.end();
            push (i, j, is_kw ? Syntax_Token_Type::Keyword : Syntax_Token_Type::Default);
            i = j;
            continue;
        }

        // Everything else: single character, default
        push (i, i + 1, Syntax_Token_Type::Default);
        ++i;
    }

    return buf.subspan (0, count);
}

// ---- Convenience: tokenize by language ----

static std::span<const Syntax_Token> tokenize_for_lang (std::string_view code,
                                                          Code_Lang lang,
                                                          Allocator& alloc)
{
    switch (lang)
    {
        case Code_Lang::CPP:    return tokenize (code, cpp_tokenizer_config(),    alloc);
        case Code_Lang::Python: return tokenize (code, python_tokenizer_config(), alloc);
        case Code_Lang::JS:     return tokenize (code, js_tokenizer_config(),     alloc);
        case Code_Lang::Jai:    return tokenize (code, jai_tokenizer_config(),    alloc);
        default:                return {};
    }
}

} // namespace chowdsp::slides
