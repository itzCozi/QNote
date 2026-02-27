//==============================================================================
// QNote - A Better Notepad for Windows
// SpellCheck.h - Windows Spell Checking API wrapper
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
#include <vector>

// Forward-declare COM interfaces to avoid pulling in spellcheck.h everywhere
struct ISpellCheckerFactory;
struct ISpellChecker;

namespace QNote {

//------------------------------------------------------------------------------
// Information about a single misspelled word
//------------------------------------------------------------------------------
struct MisspelledWord {
    DWORD startPos;         // Character offset in the checked text
    DWORD length;           // Length in characters
    std::wstring word;      // The misspelled word text
};

//------------------------------------------------------------------------------
// Wrapper around the Windows Spell Checking API (Windows 8+)
//------------------------------------------------------------------------------
class SpellChecker {
public:
    SpellChecker() noexcept = default;
    ~SpellChecker();

    // Prevent copying
    SpellChecker(const SpellChecker&) = delete;
    SpellChecker& operator=(const SpellChecker&) = delete;

    // Initialize COM spell checker for the given language (e.g. "en-US").
    // Returns false if the API is unavailable or the language is unsupported.
    [[nodiscard]] bool Initialize(const std::wstring& language = L"en-US");

    // Release COM resources
    void Shutdown() noexcept;

    // True after a successful Initialize()
    [[nodiscard]] bool IsAvailable() const noexcept { return m_checker != nullptr; }

    // Check a single word — returns true if the word is spelled correctly
    [[nodiscard]] bool CheckWord(const std::wstring& word) const;

    // Get spelling suggestions for a misspelled word
    [[nodiscard]] std::vector<std::wstring> GetSuggestions(const std::wstring& word,
                                                           int maxSuggestions = 5) const;

    // Add a word to the user dictionary (persists across sessions)
    bool AddWord(const std::wstring& word);

    // Ignore a word for the lifetime of this spell checker instance
    bool IgnoreWord(const std::wstring& word);

    // Check a block of text and return all misspelled word positions.
    // baseOffset is added to each returned startPos (useful when checking a
    // substring that starts at a known document offset).
    [[nodiscard]] std::vector<MisspelledWord> CheckText(const std::wstring& text,
                                                         DWORD baseOffset = 0) const;

private:
    ISpellCheckerFactory* m_factory = nullptr;
    ISpellChecker*        m_checker = nullptr;
};

} // namespace QNote
