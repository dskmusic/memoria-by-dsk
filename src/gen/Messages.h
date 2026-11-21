#pragma once
#include "../dsp/Common.h"

namespace gx
{
// "AI is creating your landscape" console lines: random generic + mood-specific lines (ES/EN), then a final line.

juce::StringArray aiMessages (const juce::String& category, bool english, Rng& rng, int count, juce::String& lastShown);
int numMessagesFor (const juce::String& category, bool english);
}
