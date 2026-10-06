#include "core/chat_log.h"

namespace gd::core {
void ChatLog::add(std::string line) {
  if (line.empty()) return;
  bool following = cursor_ + 1 >= lines_.size();   // on the newest line, or past it
  lines_.push_back(std::move(line));
  if (lines_.size() > cap_) {
    lines_.pop_front();
    if (cursor_ > 0) --cursor_;
  }
  if (following) cursor_ = lines_.size();
}

std::string ChatLog::step(int dir, bool& edge) {
  edge = false;
  if (lines_.empty()) { cursor_ = 0; return {}; }
  if (dir < 0) {
    if (cursor_ == 0) edge = true;
    else --cursor_;
  } else if (dir > 0) {
    if (cursor_ + 1 >= lines_.size()) { edge = true; cursor_ = lines_.size() - 1; }
    else ++cursor_;
  }
  if (cursor_ >= lines_.size()) cursor_ = lines_.size() - 1;
  return lines_[cursor_];
}
}  // namespace gd::core
