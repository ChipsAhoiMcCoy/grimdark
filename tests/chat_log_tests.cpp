#include <doctest/doctest.h>
#include "core/chat_log.h"

using gd::core::ChatLog;

TEST_CASE("chat log: older from past the end lands on the newest line, edges stay put") {
  ChatLog log;
  bool edge = false;
  CHECK(log.step(-1, edge).empty());
  log.add("a: one");
  log.add("b: two");
  CHECK(log.step(-1, edge) == "b: two");
  CHECK_FALSE(edge);
  CHECK(log.step(-1, edge) == "a: one");
  CHECK_FALSE(edge);
  CHECK(log.step(-1, edge) == "a: one");
  CHECK(edge);
  CHECK(log.step(1, edge) == "b: two");
  CHECK_FALSE(edge);
  CHECK(log.step(1, edge) == "b: two");
  CHECK(edge);
}

TEST_CASE("chat log: a new line is followed from the newest, a reader further back keeps their place") {
  ChatLog log;
  bool edge = false;
  log.add("one");
  log.add("two");
  CHECK(log.step(-1, edge) == "two");   // on the newest
  log.add("three");
  CHECK(log.step(-1, edge) == "three");  // followed: past the end again
  CHECK(log.step(-1, edge) == "two");
  CHECK(log.step(-1, edge) == "one");    // reading back
  log.add("four");
  CHECK(log.step(1, edge) == "two");     // kept the place
}

TEST_CASE("chat log: the cap drops the oldest and keeps the cursor on the same line") {
  ChatLog log(3);
  bool edge = false;
  log.add("1");
  log.add("2");
  log.add("3");
  CHECK(log.step(-1, edge) == "3");
  CHECK(log.step(-1, edge) == "2");
  log.add("4");   // drops "1"; the cursor stays on "2"
  CHECK(log.size() == 3);
  CHECK(log.step(0, edge) == "2");
  CHECK(log.step(-1, edge) == "2");
  CHECK(edge);
  log.add("");    // empty lines are ignored
  CHECK(log.size() == 3);
}
