//==============================================================================
// QNote - A Better Notepad for Windows
// SyntaxHighlighter.cpp - Syntax highlighting implementation
//==============================================================================

#include "SyntaxHighlighter.h"
#include <algorithm>
#include <cctype>

namespace QNote {

//------------------------------------------------------------------------------
// Keyword sets for each language
//------------------------------------------------------------------------------

// C/C++ keywords
static const std::unordered_set<std::wstring_view> s_cppKeywords = {
    L"auto", L"break", L"case", L"catch", L"class", L"const", L"constexpr",
    L"continue", L"default", L"delete", L"do", L"else", L"enum", L"explicit",
    L"export", L"extern", L"false", L"final", L"for", L"friend", L"goto",
    L"if", L"inline", L"mutable", L"namespace", L"new", L"noexcept",
    L"nullptr", L"operator", L"override", L"private", L"protected", L"public",
    L"register", L"return", L"sizeof", L"static", L"static_assert",
    L"static_cast", L"dynamic_cast", L"reinterpret_cast", L"const_cast",
    L"struct", L"switch", L"template", L"this", L"throw", L"true", L"try",
    L"typedef", L"typeid", L"typename", L"union", L"using", L"virtual",
    L"void", L"volatile", L"while", L"co_await", L"co_return", L"co_yield",
    L"concept", L"consteval", L"constinit", L"requires", L"module", L"import",
};

static const std::unordered_set<std::wstring_view> s_cppTypes = {
    L"int", L"long", L"short", L"char", L"float", L"double", L"bool",
    L"unsigned", L"signed", L"wchar_t", L"char8_t", L"char16_t", L"char32_t",
    L"size_t", L"int8_t", L"int16_t", L"int32_t", L"int64_t",
    L"uint8_t", L"uint16_t", L"uint32_t", L"uint64_t",
    L"string", L"wstring", L"vector", L"map", L"set", L"unordered_map",
    L"unordered_set", L"shared_ptr", L"unique_ptr", L"weak_ptr",
    L"BOOL", L"BYTE", L"WORD", L"DWORD", L"LONG", L"UINT", L"WPARAM",
    L"LPARAM", L"LRESULT", L"HWND", L"HINSTANCE", L"HDC", L"HFONT",
    L"HBRUSH", L"HPEN", L"HMENU", L"HICON", L"RECT", L"POINT", L"SIZE",
    L"COLORREF", L"LPCWSTR", L"LPWSTR", L"LPCSTR", L"LPSTR",
};

// Java keywords
static const std::unordered_set<std::wstring_view> s_javaKeywords = {
    L"abstract", L"assert", L"break", L"case", L"catch", L"class", L"const",
    L"continue", L"default", L"do", L"else", L"enum", L"extends", L"false",
    L"final", L"finally", L"for", L"goto", L"if", L"implements", L"import",
    L"instanceof", L"interface", L"native", L"new", L"null", L"package",
    L"private", L"protected", L"public", L"return", L"static", L"strictfp",
    L"super", L"switch", L"synchronized", L"this", L"throw", L"throws",
    L"transient", L"true", L"try", L"void", L"volatile", L"while", L"var",
    L"yield", L"record", L"sealed", L"permits", L"non-sealed",
};

static const std::unordered_set<std::wstring_view> s_javaTypes = {
    L"boolean", L"byte", L"char", L"double", L"float", L"int", L"long",
    L"short", L"String", L"Integer", L"Boolean", L"Double", L"Float",
    L"Long", L"Short", L"Byte", L"Character", L"Object", L"List", L"Map",
    L"Set", L"ArrayList", L"HashMap", L"HashSet",
};

// C# keywords
static const std::unordered_set<std::wstring_view> s_csharpKeywords = {
    L"abstract", L"as", L"base", L"break", L"case", L"catch", L"checked",
    L"class", L"const", L"continue", L"default", L"delegate", L"do", L"else",
    L"enum", L"event", L"explicit", L"extern", L"false", L"finally", L"fixed",
    L"for", L"foreach", L"goto", L"if", L"implicit", L"in", L"interface",
    L"internal", L"is", L"lock", L"namespace", L"new", L"null", L"operator",
    L"out", L"override", L"params", L"private", L"protected", L"public",
    L"readonly", L"ref", L"return", L"sealed", L"sizeof", L"stackalloc",
    L"static", L"struct", L"switch", L"this", L"throw", L"true", L"try",
    L"typeof", L"unchecked", L"unsafe", L"using", L"virtual", L"void",
    L"volatile", L"while", L"async", L"await", L"var", L"dynamic",
    L"yield", L"record", L"init", L"required", L"global",
};

static const std::unordered_set<std::wstring_view> s_csharpTypes = {
    L"bool", L"byte", L"char", L"decimal", L"double", L"float", L"int",
    L"long", L"object", L"sbyte", L"short", L"string", L"uint", L"ulong",
    L"ushort", L"nint", L"nuint",
};

// JavaScript/TypeScript keywords
static const std::unordered_set<std::wstring_view> s_jsKeywords = {
    L"async", L"await", L"break", L"case", L"catch", L"class", L"const",
    L"continue", L"debugger", L"default", L"delete", L"do", L"else",
    L"export", L"extends", L"false", L"finally", L"for", L"from",
    L"function", L"if", L"import", L"in", L"instanceof", L"let", L"new",
    L"null", L"of", L"return", L"static", L"super", L"switch", L"this",
    L"throw", L"true", L"try", L"typeof", L"undefined", L"var", L"void",
    L"while", L"with", L"yield",
    // TypeScript
    L"type", L"interface", L"enum", L"namespace", L"declare", L"module",
    L"implements", L"abstract", L"as", L"is", L"keyof", L"readonly",
    L"private", L"protected", L"public", L"override",
};

static const std::unordered_set<std::wstring_view> s_jsTypes = {
    L"number", L"string", L"boolean", L"any", L"never", L"unknown",
    L"object", L"symbol", L"bigint", L"Array", L"Map", L"Set",
    L"Promise", L"Date", L"RegExp", L"Error", L"Function",
};

// Rust keywords
static const std::unordered_set<std::wstring_view> s_rustKeywords = {
    L"as", L"async", L"await", L"break", L"const", L"continue", L"crate",
    L"dyn", L"else", L"enum", L"extern", L"false", L"fn", L"for", L"if",
    L"impl", L"in", L"let", L"loop", L"match", L"mod", L"move", L"mut",
    L"pub", L"ref", L"return", L"self", L"Self", L"static", L"struct",
    L"super", L"trait", L"true", L"type", L"union", L"unsafe", L"use",
    L"where", L"while", L"yield",
};

static const std::unordered_set<std::wstring_view> s_rustTypes = {
    L"i8", L"i16", L"i32", L"i64", L"i128", L"isize",
    L"u8", L"u16", L"u32", L"u64", L"u128", L"usize",
    L"f32", L"f64", L"bool", L"char", L"str",
    L"String", L"Vec", L"Option", L"Result", L"Box",
    L"Rc", L"Arc", L"Cell", L"RefCell", L"HashMap", L"HashSet",
};

// Go keywords
static const std::unordered_set<std::wstring_view> s_goKeywords = {
    L"break", L"case", L"chan", L"const", L"continue", L"default", L"defer",
    L"else", L"fallthrough", L"for", L"func", L"go", L"goto", L"if",
    L"import", L"interface", L"map", L"package", L"range", L"return",
    L"select", L"struct", L"switch", L"type", L"var",
    L"true", L"false", L"nil", L"iota",
};

static const std::unordered_set<std::wstring_view> s_goTypes = {
    L"bool", L"byte", L"complex64", L"complex128", L"error", L"float32",
    L"float64", L"int", L"int8", L"int16", L"int32", L"int64", L"rune",
    L"string", L"uint", L"uint8", L"uint16", L"uint32", L"uint64", L"uintptr",
};

// Python keywords
static const std::unordered_set<std::wstring_view> s_pythonKeywords = {
    L"False", L"None", L"True", L"and", L"as", L"assert", L"async", L"await",
    L"break", L"class", L"continue", L"def", L"del", L"elif", L"else",
    L"except", L"finally", L"for", L"from", L"global", L"if", L"import",
    L"in", L"is", L"lambda", L"nonlocal", L"not", L"or", L"pass", L"raise",
    L"return", L"try", L"while", L"with", L"yield",
};

static const std::unordered_set<std::wstring_view> s_pythonTypes = {
    L"int", L"float", L"str", L"bool", L"list", L"dict", L"set", L"tuple",
    L"bytes", L"bytearray", L"range", L"type", L"object", L"complex",
    L"frozenset", L"memoryview", L"property", L"classmethod", L"staticmethod",
};

// Shell (bash) keywords
static const std::unordered_set<std::wstring_view> s_shellKeywords = {
    L"if", L"then", L"else", L"elif", L"fi", L"case", L"esac", L"for",
    L"while", L"until", L"do", L"done", L"in", L"function", L"select",
    L"time", L"coproc", L"local", L"return", L"exit", L"export",
    L"readonly", L"declare", L"typeset", L"unset", L"shift", L"source",
    L"echo", L"printf", L"read", L"eval", L"exec", L"set", L"trap",
};

// Batch keywords
static const std::unordered_set<std::wstring_view> s_batchKeywords = {
    L"echo", L"set", L"if", L"else", L"for", L"do", L"goto", L"call",
    L"exit", L"pause", L"rem", L"cls", L"del", L"copy", L"move", L"ren",
    L"mkdir", L"rmdir", L"cd", L"dir", L"type", L"find", L"sort",
    L"not", L"exist", L"defined", L"errorlevel", L"equ", L"neq", L"lss",
    L"leq", L"gtr", L"geq", L"setlocal", L"endlocal", L"enabledelayedexpansion",
};

// PowerShell keywords
static const std::unordered_set<std::wstring_view> s_psKeywords = {
    L"if", L"else", L"elseif", L"switch", L"while", L"for", L"foreach",
    L"do", L"until", L"break", L"continue", L"return", L"exit", L"throw",
    L"try", L"catch", L"finally", L"trap", L"function", L"filter", L"param",
    L"begin", L"process", L"end", L"class", L"enum", L"using", L"in",
};

// SQL keywords
static const std::unordered_set<std::wstring_view> s_sqlKeywords = {
    L"SELECT", L"FROM", L"WHERE", L"INSERT", L"INTO", L"UPDATE", L"DELETE",
    L"CREATE", L"ALTER", L"DROP", L"TABLE", L"INDEX", L"VIEW", L"DATABASE",
    L"AND", L"OR", L"NOT", L"NULL", L"IS", L"IN", L"BETWEEN", L"LIKE",
    L"ORDER", L"BY", L"GROUP", L"HAVING", L"JOIN", L"INNER", L"LEFT",
    L"RIGHT", L"OUTER", L"FULL", L"ON", L"AS", L"SET", L"VALUES",
    L"DISTINCT", L"COUNT", L"SUM", L"AVG", L"MIN", L"MAX", L"CASE",
    L"WHEN", L"THEN", L"ELSE", L"END", L"UNION", L"ALL", L"EXISTS",
    L"LIMIT", L"OFFSET", L"PRIMARY", L"KEY", L"FOREIGN", L"REFERENCES",
    L"CONSTRAINT", L"DEFAULT", L"CHECK", L"UNIQUE", L"CASCADE",
    L"BEGIN", L"COMMIT", L"ROLLBACK", L"TRANSACTION", L"GRANT", L"REVOKE",
    L"TRIGGER", L"PROCEDURE", L"FUNCTION", L"DECLARE", L"EXEC", L"EXECUTE",
    // lowercase variants
    L"select", L"from", L"where", L"insert", L"into", L"update", L"delete",
    L"create", L"alter", L"drop", L"table", L"index", L"view", L"database",
    L"and", L"or", L"not", L"null", L"is", L"in", L"between", L"like",
    L"order", L"by", L"group", L"having", L"join", L"inner", L"left",
    L"right", L"outer", L"full", L"on", L"as", L"set", L"values",
    L"distinct", L"count", L"sum", L"avg", L"min", L"max", L"case",
    L"when", L"then", L"else", L"end", L"union", L"all", L"exists",
    L"limit", L"offset", L"primary", L"key", L"foreign", L"references",
    L"constraint", L"default", L"check", L"unique", L"cascade",
    L"begin", L"commit", L"rollback", L"transaction", L"grant", L"revoke",
    L"trigger", L"procedure", L"function", L"declare", L"exec", L"execute",
};

// Lua keywords
static const std::unordered_set<std::wstring_view> s_luaKeywords = {
    L"and", L"break", L"do", L"else", L"elseif", L"end", L"false", L"for",
    L"function", L"goto", L"if", L"in", L"local", L"nil", L"not", L"or",
    L"repeat", L"return", L"then", L"true", L"until", L"while",
};

// Ruby keywords
static const std::unordered_set<std::wstring_view> s_rubyKeywords = {
    L"alias", L"and", L"begin", L"break", L"case", L"class", L"def",
    L"defined?", L"do", L"else", L"elsif", L"end", L"ensure", L"false",
    L"for", L"if", L"in", L"module", L"next", L"nil", L"not", L"or",
    L"redo", L"rescue", L"retry", L"return", L"self", L"super", L"then",
    L"true", L"undef", L"unless", L"until", L"when", L"while", L"yield",
    L"require", L"include", L"extend", L"attr_reader", L"attr_writer",
    L"attr_accessor", L"puts", L"print", L"raise",
};

// PHP keywords
static const std::unordered_set<std::wstring_view> s_phpKeywords = {
    L"abstract", L"and", L"as", L"break", L"case", L"catch", L"class",
    L"clone", L"const", L"continue", L"declare", L"default", L"do", L"else",
    L"elseif", L"enddeclare", L"endfor", L"endforeach", L"endif",
    L"endswitch", L"endwhile", L"extends", L"false", L"final", L"finally",
    L"fn", L"for", L"foreach", L"function", L"global", L"goto", L"if",
    L"implements", L"include", L"instanceof", L"interface", L"isset",
    L"list", L"match", L"namespace", L"new", L"null", L"or", L"print",
    L"private", L"protected", L"public", L"readonly", L"require", L"return",
    L"static", L"switch", L"throw", L"trait", L"true", L"try", L"unset",
    L"use", L"var", L"while", L"xor", L"yield", L"echo", L"empty",
};

// CSS keywords (property names handled differently, these are at-rules/values)
static const std::unordered_set<std::wstring_view> s_cssKeywords = {
    L"important", L"inherit", L"initial", L"unset", L"revert", L"none",
    L"auto", L"normal", L"bold", L"italic", L"block", L"inline", L"flex",
    L"grid", L"absolute", L"relative", L"fixed", L"sticky", L"static",
    L"hidden", L"visible", L"solid", L"dashed", L"dotted", L"transparent",
};

//------------------------------------------------------------------------------
// Detect language from file extension
//------------------------------------------------------------------------------
Language SyntaxHighlighter::DetectLanguage(std::wstring_view filePath) {
    if (filePath.empty()) return Language::None;

    // Find last dot
    auto dot = filePath.rfind(L'.');
    if (dot == std::wstring_view::npos) return Language::None;

    std::wstring ext;
    for (auto ch : filePath.substr(dot)) {
        ext += static_cast<wchar_t>(towlower(ch));
    }

    if (ext == L".c" || ext == L".cpp" || ext == L".cc" || ext == L".cxx" ||
        ext == L".h" || ext == L".hpp" || ext == L".hxx" || ext == L".hh" ||
        ext == L".inl" || ext == L".ipp")
        return Language::Cpp;

    if (ext == L".py" || ext == L".pyw" || ext == L".pyi")
        return Language::Python;

    if (ext == L".js" || ext == L".jsx" || ext == L".ts" || ext == L".tsx" ||
        ext == L".mjs" || ext == L".cjs")
        return Language::JavaScript;

    if (ext == L".html" || ext == L".htm" || ext == L".xhtml")
        return Language::HTML;

    if (ext == L".css" || ext == L".scss" || ext == L".less")
        return Language::CSS;

    if (ext == L".json" || ext == L".jsonc")
        return Language::JSON;

    if (ext == L".rs")
        return Language::Rust;

    if (ext == L".go")
        return Language::Go;

    if (ext == L".java")
        return Language::Java;

    if (ext == L".cs")
        return Language::CSharp;

    if (ext == L".xml" || ext == L".xsl" || ext == L".xslt" || ext == L".svg")
        return Language::XML;

    if (ext == L".md" || ext == L".markdown")
        return Language::Markdown;

    if (ext == L".bat" || ext == L".cmd")
        return Language::Batch;

    if (ext == L".ps1" || ext == L".psm1" || ext == L".psd1")
        return Language::PowerShell;

    if (ext == L".sql")
        return Language::SQL;

    if (ext == L".lua")
        return Language::Lua;

    if (ext == L".rb" || ext == L".rake" || ext == L".gemspec")
        return Language::Ruby;

    if (ext == L".php")
        return Language::PHP;

    if (ext == L".sh" || ext == L".bash" || ext == L".zsh")
        return Language::Shell;

    if (ext == L".yml" || ext == L".yaml")
        return Language::Yaml;

    if (ext == L".ini" || ext == L".cfg" || ext == L".conf" || ext == L".properties")
        return Language::Ini;

    return Language::None;
}

//------------------------------------------------------------------------------
// Get color for a token type (VS Code Dark+ theme)
//------------------------------------------------------------------------------
COLORREF SyntaxHighlighter::GetTokenColor(TokenType type) noexcept {
    switch (type) {
        case TokenType::Keyword:      return DarkPlusColors::Keyword;
        case TokenType::String:       return DarkPlusColors::String;
        case TokenType::Comment:      return DarkPlusColors::Comment;
        case TokenType::Number:       return DarkPlusColors::Number;
        case TokenType::Preprocessor: return DarkPlusColors::Preprocessor;
        case TokenType::Type:         return DarkPlusColors::Type;
        case TokenType::Function:     return DarkPlusColors::Function;
        case TokenType::Attribute:    return DarkPlusColors::Attribute;
        case TokenType::Tag:          return DarkPlusColors::Tag;
        case TokenType::Operator:     return DarkPlusColors::Operator;
        case TokenType::Punctuation:  return DarkPlusColors::Punctuation;
        case TokenType::Escape:       return DarkPlusColors::Escape;
        case TokenType::Heading:      return DarkPlusColors::Heading;
        case TokenType::Key:          return DarkPlusColors::Key;
        default:                      return DarkPlusColors::Default;
    }
}

//------------------------------------------------------------------------------
// Helper: Is identifier character
//------------------------------------------------------------------------------
bool SyntaxHighlighter::IsIdentChar(wchar_t ch) noexcept {
    return iswalnum(ch) || ch == L'_';
}

bool SyntaxHighlighter::IsDigit(wchar_t ch) noexcept {
    return ch >= L'0' && ch <= L'9';
}

bool SyntaxHighlighter::IsHexDigit(wchar_t ch) noexcept {
    return IsDigit(ch) || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F');
}

//------------------------------------------------------------------------------
// Skip a string literal, returns position after closing quote
//------------------------------------------------------------------------------
int SyntaxHighlighter::SkipString(std::wstring_view text, int pos, wchar_t quote) {
    int len = static_cast<int>(text.size());
    pos++; // skip opening quote
    while (pos < len) {
        if (text[pos] == L'\\') {
            pos += 2; // skip escape
        } else if (text[pos] == quote) {
            pos++; // skip closing quote
            break;
        } else {
            pos++;
        }
    }
    return pos;
}

//------------------------------------------------------------------------------
// Skip a line comment (// ...), returns position at end of line
//------------------------------------------------------------------------------
int SyntaxHighlighter::SkipLineComment(std::wstring_view text, int pos) {
    int len = static_cast<int>(text.size());
    while (pos < len && text[pos] != L'\r' && text[pos] != L'\n') {
        pos++;
    }
    return pos;
}

//------------------------------------------------------------------------------
// Skip a block comment (/* ... */), returns position after */
//------------------------------------------------------------------------------
int SyntaxHighlighter::SkipBlockComment(std::wstring_view text, int pos) {
    int len = static_cast<int>(text.size());
    pos += 2; // skip /*
    while (pos < len - 1) {
        if (text[pos] == L'*' && text[pos + 1] == L'/') {
            return pos + 2;
        }
        pos++;
    }
    return len; // unclosed block comment
}

//------------------------------------------------------------------------------
// Read an identifier, returns position after identifier
//------------------------------------------------------------------------------
int SyntaxHighlighter::ReadIdentifier(std::wstring_view text, int pos) {
    int len = static_cast<int>(text.size());
    while (pos < len && IsIdentChar(text[pos])) {
        pos++;
    }
    return pos;
}

//------------------------------------------------------------------------------
// Read a number literal, returns position after number
//------------------------------------------------------------------------------
int SyntaxHighlighter::ReadNumber(std::wstring_view text, int pos) {
    int len = static_cast<int>(text.size());
    // Check for hex (0x), binary (0b), octal (0o)
    if (pos + 1 < len && text[pos] == L'0') {
        wchar_t next = text[pos + 1];
        if (next == L'x' || next == L'X') {
            pos += 2;
            while (pos < len && IsHexDigit(text[pos])) pos++;
            return pos;
        }
        if (next == L'b' || next == L'B') {
            pos += 2;
            while (pos < len && (text[pos] == L'0' || text[pos] == L'1')) pos++;
            return pos;
        }
    }
    // Decimal (possibly float)
    while (pos < len && IsDigit(text[pos])) pos++;
    if (pos < len && text[pos] == L'.') {
        pos++;
        while (pos < len && IsDigit(text[pos])) pos++;
    }
    // Exponent
    if (pos < len && (text[pos] == L'e' || text[pos] == L'E')) {
        pos++;
        if (pos < len && (text[pos] == L'+' || text[pos] == L'-')) pos++;
        while (pos < len && IsDigit(text[pos])) pos++;
    }
    // Suffix (u, l, f, etc.)
    while (pos < len && (text[pos] == L'u' || text[pos] == L'U' ||
                         text[pos] == L'l' || text[pos] == L'L' ||
                         text[pos] == L'f' || text[pos] == L'F')) {
        pos++;
    }
    return pos;
}

//------------------------------------------------------------------------------
// Main tokenize entry point
//------------------------------------------------------------------------------
std::vector<SyntaxToken> SyntaxHighlighter::Tokenize(std::wstring_view text,
                                                       Language lang,
                                                       int baseOffset) const {
    std::vector<SyntaxToken> tokens;
    if (lang == Language::None || text.empty()) return tokens;

    // Rough estimate: ~1 token per 4 characters on average
    tokens.reserve(text.size() / 4);

    switch (lang) {
        case Language::Cpp:
            TokenizeCLike(text, baseOffset, s_cppKeywords, s_cppTypes, true, tokens);
            break;
        case Language::Java:
            TokenizeCLike(text, baseOffset, s_javaKeywords, s_javaTypes, false, tokens);
            break;
        case Language::CSharp:
            TokenizeCLike(text, baseOffset, s_csharpKeywords, s_csharpTypes, true, tokens);
            break;
        case Language::JavaScript:
            TokenizeCLike(text, baseOffset, s_jsKeywords, s_jsTypes, false, tokens);
            break;
        case Language::Rust:
            TokenizeCLike(text, baseOffset, s_rustKeywords, s_rustTypes, false, tokens);
            break;
        case Language::Go:
            TokenizeCLike(text, baseOffset, s_goKeywords, s_goTypes, false, tokens);
            break;
        case Language::PHP:
            TokenizeCLike(text, baseOffset, s_phpKeywords, {}, false, tokens);
            break;
        case Language::Python:
            TokenizePython(text, baseOffset, tokens);
            break;
        case Language::HTML:
        case Language::XML:
            TokenizeHTML(text, baseOffset, tokens);
            break;
        case Language::JSON:
            TokenizeJSON(text, baseOffset, tokens);
            break;
        case Language::Markdown:
            TokenizeMarkdown(text, baseOffset, tokens);
            break;
        case Language::Shell:
            TokenizeShellLike(text, baseOffset, s_shellKeywords, L'#', tokens);
            break;
        case Language::Batch:
            TokenizeShellLike(text, baseOffset, s_batchKeywords, L'\0', tokens);
            break;
        case Language::PowerShell:
            TokenizeShellLike(text, baseOffset, s_psKeywords, L'#', tokens);
            break;
        case Language::SQL:
            TokenizeSQL(text, baseOffset, tokens);
            break;
        case Language::Lua:
            TokenizeCLike(text, baseOffset, s_luaKeywords, {}, false, tokens);
            break;
        case Language::Ruby:
            TokenizeShellLike(text, baseOffset, s_rubyKeywords, L'#', tokens);
            break;
        case Language::CSS:
            TokenizeCSS(text, baseOffset, tokens);
            break;
        case Language::Yaml:
            TokenizeYaml(text, baseOffset, tokens);
            break;
        case Language::Ini:
            TokenizeIni(text, baseOffset, tokens);
            break;
        default:
            break;
    }

    return tokens;
}

//------------------------------------------------------------------------------
// C-like language tokenizer (C, C++, Java, C#, JavaScript, Rust, Go, Lua, PHP)
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizeCLike(std::wstring_view text, int baseOffset,
                                       const std::unordered_set<std::wstring_view>& keywords,
                                       const std::unordered_set<std::wstring_view>& types,
                                       bool hasPreprocessor,
                                       std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        wchar_t ch = text[i];

        // Preprocessor (# may have leading whitespace on the line)
        if (hasPreprocessor && ch == L'#') {
            // Verify only whitespace precedes # on this line
            bool validPreproc = (i == 0);
            if (!validPreproc) {
                int j = i - 1;
                validPreproc = true;
                while (j >= 0 && text[j] != L'\n' && text[j] != L'\r') {
                    if (text[j] != L' ' && text[j] != L'\t') {
                        validPreproc = false;
                        break;
                    }
                    j--;
                }
            }
            if (validPreproc) {
                int start = i;
                i++; // skip #
                // Skip whitespace after #
                while (i < len && (text[i] == L' ' || text[i] == L'\t')) i++;
                // Read directive name
                int dirEnd = ReadIdentifier(text, i);
                tokens.push_back({ baseOffset + start, dirEnd - start, TokenType::Preprocessor });
                i = dirEnd;
                continue;
            }
        }

        // Line comment
        if (ch == L'/' && i + 1 < len && text[i + 1] == L'/') {
            int start = i;
            i = SkipLineComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Block comment
        if (ch == L'/' && i + 1 < len && text[i + 1] == L'*') {
            int start = i;
            i = SkipBlockComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Strings
        if (ch == L'"' || ch == L'\'') {
            int start = i;
            i = SkipString(text, i, ch);
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // Raw strings (R"(...)" in C++, `...` in JS/Go)
        if (ch == L'`') {
            int start = i;
            i++;
            while (i < len && text[i] != L'`') i++;
            if (i < len) i++; // skip closing backtick
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // Numbers
        if (IsDigit(ch) || (ch == L'.' && i + 1 < len && IsDigit(text[i + 1]))) {
            int start = i;
            i = ReadNumber(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Number });
            continue;
        }

        // Identifiers and keywords
        if (IsIdentChar(ch) && !IsDigit(ch)) {
            int start = i;
            i = ReadIdentifier(text, i);
            std::wstring_view word = text.substr(start, i - start);

            if (keywords.count(word)) {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Keyword });
            } else if (!types.empty() && types.count(word)) {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Type });
            } else if (i < len && text[i] == L'(') {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Function });
            }
            continue;
        }

        // Skip whitespace and other characters
        i++;
    }
}

//------------------------------------------------------------------------------
// Python tokenizer
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizePython(std::wstring_view text, int baseOffset,
                                        std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        wchar_t ch = text[i];

        // Comment
        if (ch == L'#') {
            int start = i;
            i = SkipLineComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Triple-quoted strings
        if ((ch == L'"' || ch == L'\'') && i + 2 < len &&
            text[i + 1] == ch && text[i + 2] == ch) {
            int start = i;
            wchar_t q = ch;
            i += 3; // skip opening triple quote
            while (i + 2 < len) {
                if (text[i] == L'\\') { i += 2; continue; }
                if (text[i] == q && text[i + 1] == q && text[i + 2] == q) {
                    i += 3;
                    break;
                }
                i++;
            }
            if (i >= len) i = len; // unclosed triple quote
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // Regular strings
        if (ch == L'"' || ch == L'\'') {
            // Check for f-string/r-string/b-string prefix
            int start = i;
            i = SkipString(text, i, ch);
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // String prefix (f", r", b", u", etc.)
        if ((ch == L'f' || ch == L'r' || ch == L'b' || ch == L'u' ||
             ch == L'F' || ch == L'R' || ch == L'B' || ch == L'U') &&
            i + 1 < len && (text[i + 1] == L'"' || text[i + 1] == L'\'')) {
            int start = i;
            i++; // skip prefix
            wchar_t q = text[i];
            // Check for triple-quoted
            if (i + 2 < len && text[i + 1] == q && text[i + 2] == q) {
                i += 3;
                while (i + 2 < len) {
                    if (text[i] == L'\\') { i += 2; continue; }
                    if (text[i] == q && text[i + 1] == q && text[i + 2] == q) {
                        i += 3;
                        break;
                    }
                    i++;
                }
                if (i >= len) i = len;
            } else {
                i = SkipString(text, i, q);
            }
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // Decorator
        if (ch == L'@' && (i == 0 || text[i - 1] == L'\n' || text[i - 1] == L'\r' ||
                            text[i - 1] == L' ' || text[i - 1] == L'\t')) {
            int start = i;
            i++; // skip @
            i = ReadIdentifier(text, i);
            // Allow dotted names (e.g., @app.route)
            while (i < len && text[i] == L'.') {
                i++;
                i = ReadIdentifier(text, i);
            }
            tokens.push_back({ baseOffset + start, i - start, TokenType::Attribute });
            continue;
        }

        // Numbers
        if (IsDigit(ch) || (ch == L'.' && i + 1 < len && IsDigit(text[i + 1]))) {
            int start = i;
            i = ReadNumber(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Number });
            continue;
        }

        // Identifiers
        if (IsIdentChar(ch) && !IsDigit(ch)) {
            int start = i;
            i = ReadIdentifier(text, i);
            std::wstring_view word = text.substr(start, i - start);

            if (s_pythonKeywords.count(word)) {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Keyword });
            } else if (s_pythonTypes.count(word)) {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Type });
            } else if (i < len && text[i] == L'(') {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Function });
            }
            continue;
        }

        i++;
    }
}

//------------------------------------------------------------------------------
// HTML/XML tokenizer
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizeHTML(std::wstring_view text, int baseOffset,
                                      std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        // HTML comment
        if (i + 3 < len && text[i] == L'<' && text[i + 1] == L'!' &&
            text[i + 2] == L'-' && text[i + 3] == L'-') {
            int start = i;
            i += 4;
            while (i + 2 < len) {
                if (text[i] == L'-' && text[i + 1] == L'-' && text[i + 2] == L'>') {
                    i += 3;
                    break;
                }
                i++;
            }
            if (i >= len) i = len;
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Tag
        if (text[i] == L'<') {
            int start = i;
            i++; // skip <

            // Closing tag slash
            bool isClosing = (i < len && text[i] == L'/');
            if (isClosing) i++;

            // Tag name
            if (i < len && (iswalpha(text[i]) || text[i] == L'!' || text[i] == L'?')) {
                int tagStart = i;
                while (i < len && IsIdentChar(text[i]) && text[i] != L'>') i++;
                tokens.push_back({ baseOffset + (isClosing ? start : tagStart),
                                   i - (isClosing ? start : tagStart), TokenType::Tag });

                // Attributes
                while (i < len && text[i] != L'>') {
                    // Skip whitespace
                    while (i < len && (text[i] == L' ' || text[i] == L'\t' ||
                                       text[i] == L'\r' || text[i] == L'\n')) i++;

                    if (i < len && text[i] == L'>') break;
                    if (i < len && text[i] == L'/' && i + 1 < len && text[i + 1] == L'>') {
                        i++;
                        break;
                    }

                    // Attribute name
                    if (i < len && (iswalpha(text[i]) || text[i] == L'-' || text[i] == L'_' || text[i] == L':')) {
                        int attrStart = i;
                        while (i < len && (IsIdentChar(text[i]) || text[i] == L'-' || text[i] == L':')) i++;
                        tokens.push_back({ baseOffset + attrStart, i - attrStart, TokenType::Attribute });

                        // Skip = and attribute value
                        while (i < len && (text[i] == L' ' || text[i] == L'\t')) i++;
                        if (i < len && text[i] == L'=') {
                            i++;
                            while (i < len && (text[i] == L' ' || text[i] == L'\t')) i++;
                            if (i < len && (text[i] == L'"' || text[i] == L'\'')) {
                                int valStart = i;
                                i = SkipString(text, i, text[i]);
                                tokens.push_back({ baseOffset + valStart, i - valStart, TokenType::String });
                            }
                        }
                    } else {
                        i++;
                    }
                }
                if (i < len) i++; // skip >
            } else {
                i++;
            }
            continue;
        }

        i++;
    }
}

//------------------------------------------------------------------------------
// JSON tokenizer
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizeJSON(std::wstring_view text, int baseOffset,
                                      std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        wchar_t ch = text[i];

        // Line comment (JSONC)
        if (ch == L'/' && i + 1 < len && text[i + 1] == L'/') {
            int start = i;
            i = SkipLineComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Block comment (JSONC)
        if (ch == L'/' && i + 1 < len && text[i + 1] == L'*') {
            int start = i;
            i = SkipBlockComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // String - detect if it's a key (followed by :)
        if (ch == L'"') {
            int start = i;
            i = SkipString(text, i, L'"');

            // Check if this is a key (followed by :)
            int j = i;
            while (j < len && (text[j] == L' ' || text[j] == L'\t' ||
                               text[j] == L'\r' || text[j] == L'\n')) j++;
            if (j < len && text[j] == L':') {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Key });
            } else {
                tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            }
            continue;
        }

        // Numbers
        if (IsDigit(ch) || (ch == L'-' && i + 1 < len && IsDigit(text[i + 1]))) {
            int start = i;
            if (ch == L'-') i++;
            i = ReadNumber(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Number });
            continue;
        }

        // Keywords: true, false, null
        if (IsIdentChar(ch) && !IsDigit(ch)) {
            int start = i;
            i = ReadIdentifier(text, i);
            std::wstring_view word = text.substr(start, i - start);
            if (word == L"true" || word == L"false" || word == L"null") {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Keyword });
            }
            continue;
        }

        i++;
    }
}

//------------------------------------------------------------------------------
// Markdown tokenizer
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizeMarkdown(std::wstring_view text, int baseOffset,
                                          std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        // At start of line
        bool atLineStart = (i == 0 || text[i - 1] == L'\n' || text[i - 1] == L'\r');

        // Headings (# at start of line)
        if (atLineStart && text[i] == L'#') {
            int start = i;
            while (i < len && text[i] != L'\r' && text[i] != L'\n') i++;
            tokens.push_back({ baseOffset + start, i - start, TokenType::Heading });
            continue;
        }

        // Code blocks (```)
        if (atLineStart && i + 2 < len && text[i] == L'`' &&
            text[i + 1] == L'`' && text[i + 2] == L'`') {
            int start = i;
            i += 3;
            // Skip to end of line (language tag)
            while (i < len && text[i] != L'\r' && text[i] != L'\n') i++;
            // Find closing ```
            while (i < len) {
                if (i + 2 < len && text[i] == L'`' && text[i + 1] == L'`' && text[i + 2] == L'`') {
                    i += 3;
                    break;
                }
                i++;
            }
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // Inline code (`...`)
        if (text[i] == L'`') {
            int start = i;
            i++;
            while (i < len && text[i] != L'`' && text[i] != L'\r' && text[i] != L'\n') i++;
            if (i < len && text[i] == L'`') i++;
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // Bold/italic markers
        if (text[i] == L'*' || text[i] == L'_') {
            int start = i;
            wchar_t marker = text[i];
            while (i < len && text[i] == marker) i++;
            tokens.push_back({ baseOffset + start, i - start, TokenType::Keyword });
            continue;
        }

        // Links [text](url)
        if (text[i] == L'[') {
            int start = i;
            i++;
            while (i < len && text[i] != L']' && text[i] != L'\r' && text[i] != L'\n') i++;
            if (i < len && text[i] == L']') {
                i++;
                if (i < len && text[i] == L'(') {
                    while (i < len && text[i] != L')' && text[i] != L'\r' && text[i] != L'\n') i++;
                    if (i < len && text[i] == L')') i++;
                }
            }
            tokens.push_back({ baseOffset + start, i - start, TokenType::Attribute });
            continue;
        }

        i++;
    }
}

//------------------------------------------------------------------------------
// Shell-like language tokenizer (bash, batch, PowerShell, Ruby)
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizeShellLike(std::wstring_view text, int baseOffset,
                                           const std::unordered_set<std::wstring_view>& keywords,
                                           wchar_t commentChar,
                                           std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        wchar_t ch = text[i];

        // REM comment (batch files)
        if (commentChar == L'\0') {
            // Batch: REM and :: are comments
            bool atLineStart = (i == 0 || text[i - 1] == L'\n' || text[i - 1] == L'\r');
            if (atLineStart) {
                // Check for REM (case-insensitive)
                if (i + 2 < len &&
                    (text[i] == L'r' || text[i] == L'R') &&
                    (text[i + 1] == L'e' || text[i + 1] == L'E') &&
                    (text[i + 2] == L'm' || text[i + 2] == L'M') &&
                    (i + 3 >= len || text[i + 3] == L' ' || text[i + 3] == L'\t' || text[i + 3] == L'\r' || text[i + 3] == L'\n')) {
                    int start = i;
                    i = SkipLineComment(text, i);
                    tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
                    continue;
                }
                // Check for ::
                if (i + 1 < len && text[i] == L':' && text[i + 1] == L':') {
                    int start = i;
                    i = SkipLineComment(text, i);
                    tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
                    continue;
                }
            }
        }

        // Line comment
        if (commentChar != L'\0' && ch == commentChar) {
            int start = i;
            i = SkipLineComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Strings
        if (ch == L'"' || ch == L'\'') {
            int start = i;
            i = SkipString(text, i, ch);
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // Variables ($VAR or ${VAR} or %VAR%)
        if (ch == L'$' || (commentChar == L'\0' && ch == L'%')) {
            int start = i;
            i++;
            if (i < len && text[i] == L'{') {
                while (i < len && text[i] != L'}') i++;
                if (i < len) i++;
            } else if (ch == L'%') {
                while (i < len && IsIdentChar(text[i])) i++;
                if (i < len && text[i] == L'%') i++;
            } else {
                i = ReadIdentifier(text, i);
            }
            tokens.push_back({ baseOffset + start, i - start, TokenType::Attribute });
            continue;
        }

        // Numbers
        if (IsDigit(ch)) {
            int start = i;
            i = ReadNumber(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Number });
            continue;
        }

        // Identifiers / keywords
        if (IsIdentChar(ch) && !IsDigit(ch)) {
            int start = i;
            i = ReadIdentifier(text, i);
            std::wstring_view word = text.substr(start, i - start);
            if (keywords.count(word)) {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Keyword });
            }
            continue;
        }

        i++;
    }
}

//------------------------------------------------------------------------------
// SQL tokenizer
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizeSQL(std::wstring_view text, int baseOffset,
                                     std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        wchar_t ch = text[i];

        // Line comment (--)
        if (ch == L'-' && i + 1 < len && text[i + 1] == L'-') {
            int start = i;
            i = SkipLineComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Block comment
        if (ch == L'/' && i + 1 < len && text[i + 1] == L'*') {
            int start = i;
            i = SkipBlockComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Strings
        if (ch == L'\'') {
            int start = i;
            i++;
            while (i < len) {
                if (text[i] == L'\'' && i + 1 < len && text[i + 1] == L'\'') {
                    i += 2; // escaped quote
                } else if (text[i] == L'\'') {
                    i++;
                    break;
                } else {
                    i++;
                }
            }
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // Numbers
        if (IsDigit(ch)) {
            int start = i;
            i = ReadNumber(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Number });
            continue;
        }

        // Identifiers / keywords
        if (IsIdentChar(ch) && !IsDigit(ch)) {
            int start = i;
            i = ReadIdentifier(text, i);
            std::wstring_view word = text.substr(start, i - start);
            if (s_sqlKeywords.count(word)) {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Keyword });
            }
            continue;
        }

        i++;
    }
}

//------------------------------------------------------------------------------
// INI file tokenizer
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizeIni(std::wstring_view text, int baseOffset,
                                     std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        bool atLineStart = (i == 0 || text[i - 1] == L'\n' || text[i - 1] == L'\r');

        // Comments (# or ;)
        if (atLineStart && (text[i] == L';' || text[i] == L'#')) {
            int start = i;
            i = SkipLineComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Section headers [section]
        if (atLineStart && text[i] == L'[') {
            int start = i;
            while (i < len && text[i] != L']' && text[i] != L'\r' && text[i] != L'\n') i++;
            if (i < len && text[i] == L']') i++;
            tokens.push_back({ baseOffset + start, i - start, TokenType::Heading });
            continue;
        }

        // Key = value (highlight key)
        if (atLineStart && IsIdentChar(text[i])) {
            int start = i;
            while (i < len && text[i] != L'=' && text[i] != L'\r' && text[i] != L'\n') i++;
            if (i < len && text[i] == L'=') {
                // Trim trailing whitespace from key
                int keyEnd = i;
                while (keyEnd > start && (text[keyEnd - 1] == L' ' || text[keyEnd - 1] == L'\t')) keyEnd--;
                tokens.push_back({ baseOffset + start, keyEnd - start, TokenType::Key });
                i++; // skip =
                // String value
                int valStart = i;
                while (i < len && text[i] != L'\r' && text[i] != L'\n') i++;
                // Trim leading whitespace from value
                while (valStart < i && (text[valStart] == L' ' || text[valStart] == L'\t')) valStart++;
                if (i > valStart) {
                    tokens.push_back({ baseOffset + valStart, i - valStart, TokenType::String });
                }
            }
            continue;
        }

        i++;
    }
}

//------------------------------------------------------------------------------
// YAML tokenizer
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizeYaml(std::wstring_view text, int baseOffset,
                                      std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        // Comment
        if (text[i] == L'#') {
            int start = i;
            i = SkipLineComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Strings
        if (text[i] == L'"' || text[i] == L'\'') {
            int start = i;
            i = SkipString(text, i, text[i]);
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // Key: value (detect keys)
        bool atLineStart = (i == 0 || text[i - 1] == L'\n' || text[i - 1] == L'\r');
        if (atLineStart || text[i - 1] == L' ' || text[i - 1] == L'\t') {
            // Skip leading whitespace and dashes
            int keyStart = i;
            while (i < len && (text[i] == L' ' || text[i] == L'\t' || text[i] == L'-')) i++;
            if (i < len && text[i] != L'#' && text[i] != L'\r' && text[i] != L'\n') {
                int idStart = i;
                while (i < len && text[i] != L':' && text[i] != L'\r' && text[i] != L'\n') i++;
                if (i < len && text[i] == L':') {
                    tokens.push_back({ baseOffset + idStart, i - idStart, TokenType::Key });
                    i++; // skip :
                    continue;
                }
            }
            i = keyStart; // reset if not a key
        }

        // Numbers
        if (IsDigit(text[i])) {
            int start = i;
            i = ReadNumber(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Number });
            continue;
        }

        // Keywords
        if (IsIdentChar(text[i]) && !IsDigit(text[i])) {
            int start = i;
            i = ReadIdentifier(text, i);
            std::wstring_view word = text.substr(start, i - start);
            if (word == L"true" || word == L"false" || word == L"null" ||
                word == L"yes" || word == L"no" || word == L"on" || word == L"off") {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Keyword });
            }
            continue;
        }

        i++;
    }
}

//------------------------------------------------------------------------------
// CSS tokenizer
//------------------------------------------------------------------------------
void SyntaxHighlighter::TokenizeCSS(std::wstring_view text, int baseOffset,
                                     std::vector<SyntaxToken>& tokens) const {
    int len = static_cast<int>(text.size());
    int i = 0;

    while (i < len) {
        wchar_t ch = text[i];

        // Block comment
        if (ch == L'/' && i + 1 < len && text[i + 1] == L'*') {
            int start = i;
            i = SkipBlockComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Line comment (SCSS/Less)
        if (ch == L'/' && i + 1 < len && text[i + 1] == L'/') {
            int start = i;
            i = SkipLineComment(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Comment });
            continue;
        }

        // Strings
        if (ch == L'"' || ch == L'\'') {
            int start = i;
            i = SkipString(text, i, ch);
            tokens.push_back({ baseOffset + start, i - start, TokenType::String });
            continue;
        }

        // At-rules (@media, @import, etc.)
        if (ch == L'@') {
            int start = i;
            i++;
            i = ReadIdentifier(text, i);
            tokens.push_back({ baseOffset + start, i - start, TokenType::Keyword });
            continue;
        }

        // Selectors (., #)
        if ((ch == L'.' || ch == L'#') && i + 1 < len && iswalpha(text[i + 1])) {
            int start = i;
            i++;
            while (i < len && (IsIdentChar(text[i]) || text[i] == L'-')) i++;
            tokens.push_back({ baseOffset + start, i - start, TokenType::Tag });
            continue;
        }

        // Property names (inside braces)
        if (iswalpha(ch) || ch == L'-') {
            int start = i;
            while (i < len && (IsIdentChar(text[i]) || text[i] == L'-')) i++;
            // Check if followed by :
            int j = i;
            while (j < len && (text[j] == L' ' || text[j] == L'\t')) j++;
            if (j < len && text[j] == L':') {
                tokens.push_back({ baseOffset + start, i - start, TokenType::Attribute });
            }
            continue;
        }

        // Numbers (including units like px, em, %)
        if (IsDigit(ch) || (ch == L'.' && i + 1 < len && IsDigit(text[i + 1]))) {
            int start = i;
            i = ReadNumber(text, i);
            // Include unit suffix
            while (i < len && (iswalpha(text[i]) || text[i] == L'%')) i++;
            tokens.push_back({ baseOffset + start, i - start, TokenType::Number });
            continue;
        }

        // Color literals (#hex)
        if (ch == L'#' && i + 1 < len && IsHexDigit(text[i + 1])) {
            int start = i;
            i++;
            while (i < len && IsHexDigit(text[i])) i++;
            tokens.push_back({ baseOffset + start, i - start, TokenType::Number });
            continue;
        }

        i++;
    }
}

} // namespace QNote
