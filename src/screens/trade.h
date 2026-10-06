#pragma once
#include <memory>

namespace gd::core { class Screen; }

namespace gd::screens {
// The trade window with another player: their offer, your offer (iron bits and items), your bags, Accept / Cancel.
// The other player's changes are announced as they happen.
std::unique_ptr<gd::core::Screen> make_trade();
}  // namespace gd::screens
