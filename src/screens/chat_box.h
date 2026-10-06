#pragma once
#include <memory>

namespace gd::core { class Screen; }

namespace gd::screens {
// The chat box open for typing: the game's edit box keeps every key but Up / Down (chat history); typed characters
// are echoed, Backspace reads back. Enter sends, Escape closes -- both the game's own.
std::unique_ptr<gd::core::Screen> make_chat();
}  // namespace gd::screens
