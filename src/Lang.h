#pragma once
#include <juce_core/juce_core.h>

namespace gx
{
// UI language: set once when the editor opens (from Settings), read by every UI string via L().
inline bool& uiEnglish() { static bool e = false; return e; }
// Non-empty = overrides the saved setting (MemoriaTool screenshot uses "en").
inline juce::String& forcedLanguage() { static juce::String s; return s; }
inline juce::String L (const char* es, const char* en) { return juce::String::fromUTF8 (uiEnglish() ? en : es); }
}
