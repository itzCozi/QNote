//==============================================================================
// QNote - A Better Notepad for Windows
// SpellCheck.cpp - Windows Spell Checking API wrapper implementation
//==============================================================================

// Must be included before Windows.h to avoid WIN32_LEAN_AND_MEAN stripping
#undef WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objbase.h>
#include <spellcheck.h>
#include <algorithm>
#include "SpellCheck.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace QNote {

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
SpellChecker::~SpellChecker() {
    Shutdown();
}

//------------------------------------------------------------------------------
// Initialize the spell checker for a given language
//------------------------------------------------------------------------------
bool SpellChecker::Initialize(const std::wstring& language) {
    Shutdown();

    // Create the spell checker factory
    HRESULT hr = CoCreateInstance(
        __uuidof(SpellCheckerFactory),
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(ISpellCheckerFactory),
        reinterpret_cast<void**>(&m_factory)
    );
    if (FAILED(hr) || !m_factory) {
        Shutdown();
        return false;
    }

    // Check if the requested language is supported
    BOOL supported = FALSE;
    hr = m_factory->IsSupported(language.c_str(), &supported);
    if (FAILED(hr) || !supported) {
        Shutdown();
        return false;
    }

    // Create the spell checker for that language
    hr = m_factory->CreateSpellChecker(language.c_str(), &m_checker);
    if (FAILED(hr) || !m_checker) {
        Shutdown();
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Release COM resources
//------------------------------------------------------------------------------
void SpellChecker::Shutdown() noexcept {
    if (m_checker) {
        m_checker->Release();
        m_checker = nullptr;
    }
    if (m_factory) {
        m_factory->Release();
        m_factory = nullptr;
    }
}

//------------------------------------------------------------------------------
// Check a single word
//------------------------------------------------------------------------------
bool SpellChecker::CheckWord(const std::wstring& word) const {
    if (!m_checker || word.empty()) return true;

    IEnumSpellingError* errors = nullptr;
    HRESULT hr = m_checker->Check(word.c_str(), &errors);
    if (FAILED(hr) || !errors) return true;

    ISpellingError* error = nullptr;
    bool correct = true;
    if (errors->Next(&error) == S_OK && error) {
        correct = false;
        error->Release();
    }

    errors->Release();
    return correct;
}

//------------------------------------------------------------------------------
// Get suggestions for a misspelled word
//------------------------------------------------------------------------------
std::vector<std::wstring> SpellChecker::GetSuggestions(const std::wstring& word,
                                                        int maxSuggestions) const {
    std::vector<std::wstring> results;
    if (!m_checker || word.empty()) return results;

    IEnumString* suggestions = nullptr;
    HRESULT hr = m_checker->Suggest(word.c_str(), &suggestions);
    if (FAILED(hr) || !suggestions) return results;

    LPOLESTR str = nullptr;
    ULONG fetched = 0;
    while (static_cast<int>(results.size()) < maxSuggestions &&
           suggestions->Next(1, &str, &fetched) == S_OK && fetched > 0) {
        results.emplace_back(str);
        CoTaskMemFree(str);
    }

    suggestions->Release();
    return results;
}

//------------------------------------------------------------------------------
// Add a word to the user dictionary
//------------------------------------------------------------------------------
bool SpellChecker::AddWord(const std::wstring& word) {
    if (!m_checker || word.empty()) return false;
    return SUCCEEDED(m_checker->Add(word.c_str()));
}

//------------------------------------------------------------------------------
// Ignore a word for this session
//------------------------------------------------------------------------------
bool SpellChecker::IgnoreWord(const std::wstring& word) {
    if (!m_checker || word.empty()) return false;
    return SUCCEEDED(m_checker->Ignore(word.c_str()));
}

//------------------------------------------------------------------------------
// Check a block of text and return misspelled word positions
//------------------------------------------------------------------------------
std::vector<MisspelledWord> SpellChecker::CheckText(const std::wstring& text,
                                                      DWORD baseOffset) const {
    std::vector<MisspelledWord> results;
    if (!m_checker || text.empty()) return results;

    IEnumSpellingError* errors = nullptr;
    HRESULT hr = m_checker->Check(text.c_str(), &errors);
    if (FAILED(hr) || !errors) return results;

    ISpellingError* error = nullptr;
    while (errors->Next(&error) == S_OK && error) {
        ULONG startIndex = 0;
        ULONG length = 0;
        error->get_StartIndex(&startIndex);
        error->get_Length(&length);

        CORRECTIVE_ACTION action = CORRECTIVE_ACTION_NONE;
        error->get_CorrectiveAction(&action);

        // Only flag words that need correction (not auto-correctable ones)
        if (action != CORRECTIVE_ACTION_NONE && length > 0 &&
            startIndex + length <= static_cast<ULONG>(text.size())) {
            MisspelledWord mw;
            mw.startPos = baseOffset + startIndex;
            mw.length = length;
            mw.word = text.substr(startIndex, length);
            results.push_back(std::move(mw));
        }

        error->Release();
    }

    errors->Release();
    return results;
}

} // namespace QNote
