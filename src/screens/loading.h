#pragma once
#include <memory>
#include "core/screen.h"

namespace gd::screens {
// The loading screen between the menu and the world: the game draws a "Tip: ..." line and no HUD. Speaks
// "loading" and the tip once; nothing to navigate. Layer 40 so it covers whatever was below.
std::unique_ptr<gd::core::Screen> make_loading();
// Leaving the world for the main menu: the session is being torn down (app state 1, no world screen). Silent, keys
// with the game; "leaving the game" only if it lasts 1.5 s.
std::unique_ptr<gd::core::Screen> make_leaving();
}  // namespace gd::screens
