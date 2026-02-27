//==============================================================================
// QNote - A Lightweight Notepad Clone
// EditorSearch.cpp - Search and replace operations
//==============================================================================

#include "Editor.h"
#include <regex>

namespace QNote {

//------------------------------------------------------------------------------
// Find text
//------------------------------------------------------------------------------
bool Editor::SearchText(std::wstring_view searchText, bool matchCase, bool wrapAround,
                        bool searchUp, bool useRegex, bool selectMatch) {
    if (searchText.empty() || !m_hwndEdit) {
        return false;
    }
    
    std::wstring text = GetText();
    if (text.empty()) {
        return false;
    }
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    // Start search from after current selection (or before if searching up)
    size_t searchStart = searchUp ? (selStart > 0 ? selStart - 1 : 0) : selEnd;
    
    size_t foundPos = std::wstring::npos;
    size_t foundLen = searchText.length();
    
    const std::wstring searchStr(searchText);
    
    if (useRegex) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!matchCase) {
                flags |= std::regex::icase;
            }
            std::wregex regex(searchStr, flags);
            
            std::wsmatch match;
            if (searchUp) {
                // Search backwards - find all matches up to searchStart
                std::wstring searchRegion = text.substr(0, searchStart);
                auto it = searchRegion.cbegin();
                size_t lastPos = std::wstring::npos;
                while (std::regex_search(it, searchRegion.cend(), match, regex)) {
                    lastPos = std::distance(searchRegion.cbegin(), it) + match.position();
                    foundLen = match.length();
                    it = match[0].second;
                }
                foundPos = lastPos;
                
                // Wrap around if needed
                if (foundPos == std::wstring::npos && wrapAround) {
                    std::wstring wrapRegion = text.substr(searchStart);
                    it = wrapRegion.cbegin();
                    while (std::regex_search(it, wrapRegion.cend(), match, regex)) {
                        lastPos = searchStart + std::distance(wrapRegion.cbegin(), it) + match.position();
                        foundLen = match.length();
                        it = match[0].second;
                    }
                    foundPos = lastPos;
                }
            } else {
                std::wstring searchRegion = text.substr(searchStart);
                if (std::regex_search(searchRegion, match, regex)) {
                    foundPos = searchStart + match.position();
                    foundLen = match.length();
                } else if (wrapAround) {
                    // Wrap around
                    searchRegion = text.substr(0, searchStart);
                    if (std::regex_search(searchRegion, match, regex)) {
                        foundPos = match.position();
                        foundLen = match.length();
                    }
                }
            }
        } catch (const std::regex_error&) {
            // Invalid regex
            return false;
        }
    } else {
        // Plain text search
        if (searchUp) {
            if (matchCase) {
                foundPos = text.rfind(searchStr, searchStart);
            } else {
                // Case-insensitive search backwards
                std::wstring lowerText = text;
                std::wstring lowerSearch = searchStr;
                for (auto& c : lowerText) c = towlower(c);
                for (auto& c : lowerSearch) c = towlower(c);
                foundPos = lowerText.rfind(lowerSearch, searchStart);
            }
            
            if (foundPos == std::wstring::npos && wrapAround) {
                // Wrap to end
                if (matchCase) {
                    foundPos = text.rfind(searchStr);
                } else {
                    std::wstring lowerText = text;
                    std::wstring lowerSearch = searchStr;
                    for (auto& c : lowerText) c = towlower(c);
                    for (auto& c : lowerSearch) c = towlower(c);
                    foundPos = lowerText.rfind(lowerSearch);
                }
            }
        } else {
            if (matchCase) {
                foundPos = text.find(searchStr, searchStart);
            } else {
                // Case-insensitive search
                std::wstring lowerText = text;
                std::wstring lowerSearch = searchStr;
                for (auto& c : lowerText) c = towlower(c);
                for (auto& c : lowerSearch) c = towlower(c);
                foundPos = lowerText.find(lowerSearch, searchStart);
            }
            
            if (foundPos == std::wstring::npos && wrapAround) {
                // Wrap to beginning
                if (matchCase) {
                    foundPos = text.find(searchStr, 0);
                } else {
                    std::wstring lowerText = text;
                    std::wstring lowerSearch = searchStr;
                    for (auto& c : lowerText) c = towlower(c);
                    for (auto& c : lowerSearch) c = towlower(c);
                    foundPos = lowerText.find(lowerSearch, 0);
                }
            }
        }
    }
    
    if (foundPos != std::wstring::npos && selectMatch) {
        SetSelection(static_cast<DWORD>(foundPos), static_cast<DWORD>(foundPos + foundLen));
        return true;
    }
    
    return foundPos != std::wstring::npos;
}

//------------------------------------------------------------------------------
// Replace all occurrences
//------------------------------------------------------------------------------
int Editor::ReplaceAll(std::wstring_view searchText, std::wstring_view replaceText,
                       bool matchCase, bool useRegex) {
    if (searchText.empty() || !m_hwndEdit) {
        return 0;
    }
    
    std::wstring text = GetText();
    if (text.empty()) {
        return 0;
    }
    
    const std::wstring searchStr(searchText);
    const std::wstring replaceStr(replaceText);
    
    int count = 0;
    std::wstring result;
    
    if (useRegex) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!matchCase) {
                flags |= std::regex::icase;
            }
            std::wregex regex(searchStr, flags);
            
            // Count matches
            auto begin = std::wsregex_iterator(text.begin(), text.end(), regex);
            auto end = std::wsregex_iterator();
            count = static_cast<int>(std::distance(begin, end));
            
            // Replace all
            result = std::regex_replace(text, regex, replaceStr);
        } catch (const std::regex_error&) {
            return 0;
        }
    } else {
        result.reserve(text.size());
        size_t pos = 0;
        size_t searchLen = searchStr.length();
        
        std::wstring searchLower;
        std::wstring textLower;
        if (!matchCase) {
            searchLower = searchStr;
            textLower = text;
            for (auto& c : searchLower) c = towlower(c);
            for (auto& c : textLower) c = towlower(c);
        }
        
        while (pos < text.size()) {
            size_t found;
            if (matchCase) {
                found = text.find(searchStr, pos);
            } else {
                found = textLower.find(searchLower, pos);
            }
            
            if (found == std::wstring::npos) {
                result.append(text.substr(pos));
                break;
            }
            
            result.append(text.substr(pos, found - pos));
            result.append(replaceStr);
            pos = found + searchLen;
            count++;
        }
    }
    
    if (count > 0) {
        PushUndoCheckpoint(EditAction::Other);
        // Use SetWindowText directly to avoid clearing undo in SetText()
        m_suppressUndo = true;
        DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);
        SetWindowTextW(m_hwndEdit, result.c_str());
        ApplyCharFormat();
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);
        SetModified(true);
        m_suppressUndo = false;
    }
    
    return count;
}

} // namespace QNote
