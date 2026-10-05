#include "core/server_row.h"
#include <cctype>
#include <tuple>
#include <utility>

namespace gd::core {
namespace {
std::string_view trim(std::string_view s) {
  while (!s.empty() && std::isspace((unsigned char)s.front())) s.remove_prefix(1);
  while (!s.empty() && std::isspace((unsigned char)s.back())) s.remove_suffix(1);
  return s;
}
// A whole non-negative integer, else -1.
int to_int(std::string_view s) {
  s = trim(s);
  if (s.empty() || s.size() > 9) return -1;
  int v = 0;
  for (char c : s) { if (c < '0' || c > '9') return -1; v = v * 10 + (c - '0'); }
  return v;
}
// "1 / 4" -> {1, 4}; "1 - 200" -> {1, 200}; {-1, -1} when the text has another shape.
std::pair<int, int> split_pair(std::string_view s, char sep) {
  size_t at = s.find(sep);
  if (at == std::string_view::npos) return {-1, -1};
  int a = to_int(s.substr(0, at)), b = to_int(s.substr(at + 1));
  return a < 0 || b < 0 ? std::pair{-1, -1} : std::pair{a, b};
}
std::string_view cell(const std::vector<std::string>& cells, size_t i) { return i < cells.size() ? std::string_view(cells[i]) : std::string_view(); }
// The difficulty icon the exe picks from the server's GameInfo (exe+0xe9961): challenge 1 -> ChallengeMode,
// challenge 3 -> UltimateChallengeMode, else difficulty 0/1/2 -> Difficulty01/02/03 (Survival_* in the Crucible).
// Veteran is Normal's challenge mode and Ascendant Ultimate's, the order of the server list's difficulty filter.
std::string difficulty_word(const std::string& icon) {
  if (icon == "Difficulty01") return "Normal";
  if (icon == "Difficulty02") return "Elite";
  if (icon == "Difficulty03") return "Ultimate";
  if (icon == "ChallengeMode") return "Veteran";
  if (icon == "UltimateChallengeMode") return "Ascendant";
  if (icon == "Survival_IconDifficulty01") return "Aspirant";
  if (icon == "Survival_IconDifficulty02") return "Challenger";
  if (icon == "Survival_IconDifficulty03") return "Gladiator";
  return icon;   // an icon this table does not know: its own name, never a guess
}
}  // namespace

std::string server_icon_name(std::string_view c) {
  c = trim(c);
  constexpr std::string_view kTexture = "[texture]", kPrefix = "Multiplayer_Icon", kExt = ".tex";
  if (c.substr(0, kTexture.size()) != kTexture) return {};
  size_t slash = c.find_last_of('/');
  std::string_view file = slash == std::string_view::npos ? c.substr(kTexture.size()) : c.substr(slash + 1);
  if (file.size() >= kExt.size() && file.substr(file.size() - kExt.size()) == kExt) file.remove_suffix(kExt.size());
  if (file.substr(0, kPrefix.size()) == kPrefix) file.remove_prefix(kPrefix.size());
  return std::string(file);
}

ServerRow parse_server_row(const std::vector<std::string>& cells) {
  ServerRow r;
  r.locked = server_icon_name(cell(cells, 0)) == "Locked";
  r.name = std::string(trim(cell(cells, 1)));
  r.type = std::string(trim(cell(cells, 2)));
  std::tie(r.players, r.max_players) = split_pair(cell(cells, 3), '/');
  std::tie(r.min_level, r.max_level) = split_pair(cell(cells, 4), '-');
  std::string diff = server_icon_name(cell(cells, 5));
  if (!diff.empty()) r.difficulty = difficulty_word(diff);
  r.hardcore = !server_icon_name(cell(cells, 6)).empty();
  r.auto_party = !server_icon_name(cell(cells, 7)).empty();
  r.instanced_loot = !server_icon_name(cell(cells, 8)).empty();
  r.pvp = !server_icon_name(cell(cells, 9)).empty();
  r.version = std::string(trim(cell(cells, 10)));
  r.ping = to_int(cell(cells, 11));
  return r;
}
}  // namespace gd::core
