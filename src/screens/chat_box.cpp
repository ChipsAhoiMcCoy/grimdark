// The chat box while it is open for typing (Ctrl+Enter in the world, or the players overlay's Whisper). The game's
// own edit box has the keyboard: every key passes through to it -- Enter sends (the game's submit, exe+0x162c90),
// Escape closes -- except Up / Down, which review the chat history (src/chat.cpp) without leaving the box. Typed
// characters are echoed and Backspace reads back what is left, as in the menus' edit fields (screens/edit_field.h).
#include "screens/chat_box.h"
#include <string>
#include "chat.h"
#include "core/message_builder.h"
#include "core/screen.h"
#include "core/strings.h"
#include "exe_ui.h"
#include "hooks.h"
#include "log.h"
#include "speech.h"
#include "world.h"

namespace gd::screens {
using namespace gd::core;

namespace {
constexpr int kBackspace = 0x0e, kUp = 0x79, kDown = 0x7e;

std::string last_char(const std::string& s) {
  if (s.empty()) return std::string(strings::kEmpty);
  size_t i = s.size() - 1;
  while (i > 0 && ((unsigned char)s[i] & 0xc0) == 0x80) --i;
  std::string c = s.substr(i);
  return c == " " ? std::string(strings::kSpace) : c;
}

class ChatScreen : public Screen {
 public:
  std::string_view key() const override { return "chat"; }
  bool is_active() override { return world::in_world() && exe_ui::chat_open(); }
  std::string screen_name() const override { return {}; }   // on_focus speaks "chat" with what is already typed
  int layer() const override { return 15; }   // above the in-world windows: the game gives the box the keyboard
  bool exclusive() const override { return true; }
  bool captures_raw_input() const override { return true; }   // no navigation, no type-ahead: the keys are the box's
  bool owns_keyboard() const override { return true; }
  bool passes_key(int code) const override { return code != kUp && code != kDown; }
  bool allows_typeahead() const override { return false; }
  std::vector<InputCategory> input_categories() const override { return {}; }
  void on_focus() override {
    MessageBuilder m; m.fragment(strings::kChat);
    std::string typed = exe_ui::chat_input();   // a whisper opens with "/w Name " already in the box
    if (!typed.empty()) m.list_item().fragment(typed);
    speech::speak(m.build(), true);
  }
  void on_update() override {
    const KeySource& ks = hooks::key_source();
    for (char16_t c : hooks::typed_chars()) {
      std::string ch = log::utf8(std::u16string_view(&c, 1));
      speech::speak(c == u' ' ? std::string(strings::kSpace) : ch, true);
    }
    if (ks.just_pressed(kBackspace)) speech::speak(last_char(exe_ui::chat_input()), true);
    if (ks.just_pressed(kUp)) chat::review(-1);
    if (ks.just_pressed(kDown)) chat::review(1);
  }
};
}  // namespace

std::unique_ptr<Screen> make_chat() { return std::make_unique<ChatScreen>(); }
}  // namespace gd::screens
