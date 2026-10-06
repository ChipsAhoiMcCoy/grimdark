#include <doctest/doctest.h>
#include "core/strings.h"

TEST_CASE("strip_markup removes the game's colour codes and turns line breaks into spaces") {
  CHECK(gd::strings::strip_markup("{^b}Burial Hill Entrance") == "Burial Hill Entrance");
  CHECK(gd::strings::strip_markup("^ySecure the {^w}Riftgate") == "Secure the Riftgate");
  CHECK(gd::strings::strip_markup("line one{^n}line two") == "line one line two");
  CHECK(gd::strings::strip_markup("a^nb") == "a b");
  CHECK(gd::strings::strip_markup("100^^ pure") == "100^ pure");
  CHECK(gd::strings::strip_markup("plain") == "plain");
  CHECK(gd::strings::strip_markup("") == "");
}

TEST_CASE("push_chat_line: kind word, the game's name separator, text verbatim") {
  using gd::core::MessageBuilder;
  { MessageBuilder m; gd::strings::push_chat_line(m, "", "Bob", "hello there"); CHECK(m.build() == "Bob: hello there"); }
  { MessageBuilder m; gd::strings::push_chat_line(m, "whisper", "Bob", "psst"); CHECK(m.build() == "whisper, Bob: psst"); }
  { MessageBuilder m; gd::strings::push_chat_line(m, "", "", "Player has left"); CHECK(m.build() == "Player has left"); }
}

TEST_CASE("push_player_row: name, level and class, party, distance") {
  using gd::core::MessageBuilder;
  { MessageBuilder m; gd::strings::push_player_row(m, "Bob", 12, "Soldier", true, false, 8); CHECK(m.build() == "Bob, level 12 Soldier, in your party, 8 away"); }
  { MessageBuilder m; gd::strings::push_player_row(m, "Ann", 3, "", false, true, -1); CHECK(m.build() == "Ann, level 3, party leader"); }
}
