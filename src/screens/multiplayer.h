#pragma once
#include <memory>
#include "core/screen.h"

namespace gd::screens {
// The Multiplayer screen (app state 6): Join / Host, Internet / LAN, the server filters and list, the Steam friends
// list, the host settings, the character picker and Start / Back -- all read from and driven through the exe's
// widgets (docs/exe-ui-layout.md "Multiplayer"). Layer 0 (it replaces the main menu while it is up).
std::unique_ptr<gd::core::Screen> make_multiplayer();
}  // namespace gd::screens
