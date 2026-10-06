#pragma once
#include <memory>

namespace gd::core { class Screen; }

namespace gd::screens {
// The other players in a multiplayer game (Ctrl+K in the world): one row per player ("Bob, level 12 Soldier, in your
// party"); Enter = what the game's party portrait menu and group window offer for them (invite to party, remove from
// party, inspect, trade, whisper, ban), each behind the game's own gate; Space = distance and bearing; a last row
// leaves your party. Says "no other players" and stays shut in a game of one.
void open_players();
std::unique_ptr<gd::core::Screen> make_players();
}  // namespace gd::screens
