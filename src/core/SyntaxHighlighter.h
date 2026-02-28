//==============================================================================
// QNote - A Better Notepad for Windows
// SyntaxHighlighter.h - Syntax highlighting with VS Code-like colors
//==============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>

namespace QNote {

//------------------------------------------------------------------------------
// Supported languages for syntax highlighting
//------------------------------------------------------------------------------
enum class Language {
    None,       // No highlighting (plain text)
    Cpp,        // C / C++
    Python,
    JavaScript, // JavaScript / TypeScript
    HTML,
    CSS,
    JSON,
    Rust,
    Go,
    Java,
    CSharp,
    XML,
    Markdown,
    Batch,      // Windows batch files
    PowerShell,
    SQL,
    Lua,
    Ruby,
    PHP,
    Shell,      // Bash / sh
    Yaml,
    Ini,
};

//------------------------------------------------------------------------------
// Token types produced by the tokenizer
//------------------------------------------------------------------------------
enum class TokenType {
    Default,        // Normal text
    Keyword,        // Language keywords
    String,         // String literals
    Comment,        // Comments (single-line and multi-line)
    Number,         // Numeric literals
    Preprocessor,   // Preprocessor directives (#include, etc.)
    Type,           // Built-in type names
    Operator,       // Operators
    Function,       // Function/method names
    Attribute,      // HTML/XML attributes, decorators
    Tag,            // HTML/XML tag names
    Punctuation,    // Braces, brackets, semicolons
    Escape,         // Escape sequences in strings
    Heading,        // Markdown headings
    Key,            // JSON/YAML keys
};

//------------------------------------------------------------------------------
// A single highlighted token
//------------------------------------------------------------------------------
struct SyntaxToken {
    int start;          // Character offset from beginning of text
    int length;         // Length in characters
    TokenType type;
};

//------------------------------------------------------------------------------
// VS Code Dark+ color scheme (COLORREF = BGR)
//------------------------------------------------------------------------------
namespace DarkPlusColors {
    constexpr COLORREF Default      = RGB(212, 212, 212); // #D4D4D4
    constexpr COLORREF Keyword      = RGB( 86, 156, 214); // #569CD6
    constexpr COLORREF String       = RGB(206, 145, 120); // #CE9178
    constexpr COLORREF Comment      = RGB(106, 153,  85); // #6A9955
    constexpr COLORREF Number       = RGB(181, 206, 168); // #B5CEA8
    constexpr COLORREF Preprocessor = RGB(197, 134, 192); // #C586C0
    constexpr COLORREF Type         = RGB( 78, 201, 176); // #4EC9B0
    constexpr COLORREF Function     = RGB(220, 220, 170); // #DCDCAA
    constexpr COLORREF Attribute    = RGB(156, 220, 254); // #9CDCFE
    constexpr COLORREF Tag          = RGB( 86, 156, 214); // #569CD6
    constexpr COLORREF Punctuation  = RGB(212, 212, 212); // #D4D4D4
    constexpr COLORREF Operator     = RGB(212, 212, 212); // #D4D4D4
    constexpr COLORREF Escape       = RGB(215, 186, 125); // #D7BA7D
    constexpr COLORREF Heading      = RGB( 86, 156, 214); // #569CD6
    constexpr COLORREF Key          = RGB(156, 220, 254); // #9CDCFE
}

//------------------------------------------------------------------------------
// Syntax Highlighter class
//------------------------------------------------------------------------------
class SyntaxHighlighter {
public:
    SyntaxHighlighter() noexcept = default;
    ~SyntaxHighlighter() = default;

    // Detect language from file extension (e.g. L".cpp", L".py")
    [[nodiscard]] static Language DetectLanguage(std::wstring_view filePath);

    // Tokenize a block of text for the given language.
    // baseOffset is added to each token's start position.
    [[nodiscard]] std::vector<SyntaxToken> Tokenize(std::wstring_view text,
                                                     Language lang,
                                                     int baseOffset = 0) const;

    // Get the COLORREF for a token type
    [[nodiscard]] static COLORREF GetTokenColor(TokenType type) noexcept;

private:
    // Language-specific tokenizers
    void TokenizeCLike(std::wstring_view text, int baseOffset,
                       const std::unordered_set<std::wstring_view>& keywords,
                       const std::unordered_set<std::wstring_view>& types,
                       bool hasPreprocessor,
                       std::vector<SyntaxToken>& tokens) const;

    void TokenizePython(std::wstring_view text, int baseOffset,
                        std::vector<SyntaxToken>& tokens) const;

    void TokenizeHTML(std::wstring_view text, int baseOffset,
                      std::vector<SyntaxToken>& tokens) const;

    void TokenizeJSON(std::wstring_view text, int baseOffset,
                      std::vector<SyntaxToken>& tokens) const;

    void TokenizeMarkdown(std::wstring_view text, int baseOffset,
                          std::vector<SyntaxToken>& tokens) const;

    void TokenizeShellLike(std::wstring_view text, int baseOffset,
                           const std::unordered_set<std::wstring_view>& keywords,
                           wchar_t commentChar,
                           std::vector<SyntaxToken>& tokens) const;

    void TokenizeSQL(std::wstring_view text, int baseOffset,
                     std::vector<SyntaxToken>& tokens) const;

    void TokenizeIni(std::wstring_view text, int baseOffset,
                     std::vector<SyntaxToken>& tokens) const;

    void TokenizeYaml(std::wstring_view text, int baseOffset,
                      std::vector<SyntaxToken>& tokens) const;

    void TokenizeCSS(std::wstring_view text, int baseOffset,
                     std::vector<SyntaxToken>& tokens) const;

    // Helpers
    [[nodiscard]] static bool IsIdentChar(wchar_t ch) noexcept;
    [[nodiscard]] static bool IsDigit(wchar_t ch) noexcept;
    [[nodiscard]] static bool IsHexDigit(wchar_t ch) noexcept;

    static int SkipString(std::wstring_view text, int pos, wchar_t quote);
    static int SkipLineComment(std::wstring_view text, int pos);
    static int SkipBlockComment(std::wstring_view text, int pos);
    static int ReadIdentifier(std::wstring_view text, int pos);
    static int ReadNumber(std::wstring_view text, int pos);
};

} // namespace QNote
