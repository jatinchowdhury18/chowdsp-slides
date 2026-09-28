#pragma once

#include <iostream>
#include <string>
#include <vector>

namespace chowdsp::slides
{
// Collects non-fatal problems found while loading/reloading slides.gon (bad
// step_order, a reload that couldn't fully restore position, a parse error
// that would otherwise just print to the console and vanish). Cleared at the
// start of each load attempt so the UI banner only reflects the latest state.
struct Diagnostics
{
    std::vector<std::string> messages;

    void warn (std::string message)
    {
        std::cout << "WARNING: " << message << '\n';
        messages.push_back (std::move (message));
    }

    void clear() { messages.clear(); }
};

inline Diagnostics& global_diagnostics()
{
    static Diagnostics diagnostics;
    return diagnostics;
}
} // namespace chowdsp::slides
