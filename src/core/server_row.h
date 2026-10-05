#pragma once
#include <string>
#include <string_view>
#include <vector>

// One row of the multiplayer screen's server list, as the game's list widget holds it: twelve cells of drawn text,
// icon cells as "[texture]UI/MainMenu/Multiplayer/Multiplayer_Icon<Name>.tex" (or " " when the column is blank).
// Column order (measured live 2026-10-04, docs/exe-ui-layout.md "Multiplayer"): lock, game name, type (expansion),
// players "1 / 4 ", level range "1 - 200", difficulty icon, hardcore icon, auto-party icon, loot icon, PvP icon,
// version "v1.3.0.8 (x64)", ping ("---" = not measured). Engine-free, unit-tested.
namespace gd::core {
struct ServerRow {
  bool locked = false;           // a password is required
  std::string name, type;        // the game's own text
  int players = -1, max_players = -1;
  int min_level = -1, max_level = -1;
  std::string difficulty;        // "Normal" ... "Ascendant" (Crucible: "Aspirant" ...), the icon's own name when unknown
  // The game ships one icon per flag (UI.arc mainmenu/multiplayer/): the cell is blank when the flag is off. Loot's
  // two values are Instanced (the host default, icon shown) and Free (blank).
  bool hardcore = false, auto_party = false, pvp = false, instanced_loot = false;
  std::string version;
  int ping = -1;                 // -1 = not measured ("---")
};
// The icon's name from a texture cell: "Multiplayer_IconDifficulty01" -> "Difficulty01"; empty for a blank cell.
std::string server_icon_name(std::string_view cell);
ServerRow parse_server_row(const std::vector<std::string>& cells);
}  // namespace gd::core
