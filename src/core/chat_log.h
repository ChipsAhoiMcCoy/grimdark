#pragma once
// The chat history the player reviews (Ctrl+Up / Ctrl+Down in the world, Up / Down while typing): the game's own
// chat lines in arrival order, capped, with a review cursor. Engine-free, unit-tested.
//
// Cursor rule: the cursor starts past the newest line (nothing reviewed yet). Older (-1) from there lands on the
// newest line; at either end the cursor stays put and the step reports the edge so the caller can say so. A line
// arriving while the cursor sits on the newest line (or past it) moves the cursor past the end again, so the next
// "older" reads the new line; a player reading back through older lines keeps their place.
#include <cstddef>
#include <deque>
#include <string>

namespace gd::core {
class ChatLog {
 public:
  explicit ChatLog(size_t cap = 200) : cap_(cap ? cap : 1) {}
  void add(std::string line);
  // Move the cursor: dir < 0 = older, dir > 0 = newer. Returns the line under the cursor afterwards, or "" when
  // the log is empty. `edge` is set when the cursor could not move (already on the oldest / newest line).
  std::string step(int dir, bool& edge);
  std::string newest() const { return lines_.empty() ? std::string() : lines_.back(); }
  size_t size() const { return lines_.size(); }
  // 0-based index of the reviewed line, == size() when past the end.
  size_t cursor() const { return cursor_; }
  const std::deque<std::string>& lines() const { return lines_; }
  void clear() { lines_.clear(); cursor_ = 0; }

 private:
  size_t cap_;
  std::deque<std::string> lines_;
  size_t cursor_ = 0;
};
}  // namespace gd::core
