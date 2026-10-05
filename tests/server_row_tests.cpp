#include <doctest/doctest.h>
#include "core/message_builder.h"
#include "core/server_row.h"
#include "core/strings.h"

using gd::core::MessageBuilder;
using gd::core::parse_server_row;
using gd::core::server_icon_name;

namespace {
const std::string kIcon = "[texture]UI/MainMenu/Multiplayer/Multiplayer_Icon";
std::string icon(const char* name) { return kIcon + name + ".tex"; }
// Rows as the list widget held them, live 2026-10-04.
std::vector<std::string> locked_elite() {
  return {icon("Locked"), "Squad", "Grim Dawn", "1 / 2 ", "1 - 200", icon("Difficulty02"), " ", icon("Party"), icon("Loot"), " ", "v1.3.0.8 (x64)", "---"};
}
std::vector<std::string> open_normal() {
  return {" ", "Grim Dawn Server", "Forgotten Gods", "3 / 4 ", "1 - 200", icon("Difficulty01"), " ", icon("Party"), icon("Loot"), " ", "v1.3.0.8 (x64)", "137"};
}
}  // namespace

TEST_CASE("server row: icon names") {
  CHECK(server_icon_name(icon("Locked")) == "Locked");
  CHECK(server_icon_name(" ") == "");
  CHECK(server_icon_name("") == "");
  CHECK(server_icon_name("Grim Dawn") == "");
  CHECK(server_icon_name("[texture]UI/MainMenu/Multiplayer/Survival_IconDifficulty02.tex") == "Survival_IconDifficulty02");
}

TEST_CASE("server row: a locked game with no ping") {
  auto r = parse_server_row(locked_elite());
  CHECK(r.locked);
  CHECK(r.name == "Squad");
  CHECK(r.type == "Grim Dawn");
  CHECK(r.players == 1); CHECK(r.max_players == 2);
  CHECK(r.min_level == 1); CHECK(r.max_level == 200);
  CHECK(r.difficulty == "Elite");
  CHECK_FALSE(r.hardcore); CHECK(r.auto_party); CHECK(r.instanced_loot); CHECK_FALSE(r.pvp);
  CHECK(r.version == "v1.3.0.8 (x64)");
  CHECK(r.ping == -1);
}

TEST_CASE("server row: difficulty icons map to the filter's names") {
  auto with = [](const char* d) { auto c = open_normal(); c[5] = icon(d); return parse_server_row(c).difficulty; };
  CHECK(with("Difficulty01") == "Normal");
  CHECK(with("Difficulty02") == "Elite");
  CHECK(with("Difficulty03") == "Ultimate");
  CHECK(with("ChallengeMode") == "Veteran");
  CHECK(with("UltimateChallengeMode") == "Ascendant");
  CHECK(with("SomethingNew") == "SomethingNew");   // unknown: its own name, never a guess
}

TEST_CASE("server row: blank flag cells are the off values") {
  auto c = open_normal();
  c[6] = icon("Hardcore"); c[7] = " "; c[8] = " "; c[9] = icon("PvP");
  auto r = parse_server_row(c);
  CHECK(r.hardcore); CHECK_FALSE(r.auto_party); CHECK_FALSE(r.instanced_loot); CHECK(r.pvp);
}

TEST_CASE("server row: short or malformed rows do not throw") {
  auto r = parse_server_row({});
  CHECK(r.name.empty()); CHECK(r.players == -1); CHECK(r.ping == -1);
  r = parse_server_row({" ", "x", "y", "full", "1 to 5"});
  CHECK(r.players == -1); CHECK(r.min_level == -1);
}

TEST_CASE("server row: spoken") {
  MessageBuilder m;
  gd::strings::push_server_row(m, parse_server_row(open_normal()));
  CHECK(m.build() == "Grim Dawn Server, 3 of 4 players, Normal, Forgotten Gods, level 1 to 200, ping 137");
  MessageBuilder m2;
  auto c = locked_elite(); c[6] = icon("Hardcore"); c[7] = " "; c[8] = " "; c[9] = icon("PvP");
  gd::strings::push_server_row(m2, parse_server_row(c));
  CHECK(m2.build() == "Squad, 1 of 2 players, Elite, Grim Dawn, level 1 to 200, password, hardcore, PvP, free loot, no auto party");
}
