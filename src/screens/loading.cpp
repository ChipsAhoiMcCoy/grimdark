#include "screens/loading.h"
#include <windows.h>
#include "core/graph_builder.h"
#include "core/message_builder.h"
#include "core/strings.h"
#include "exe_ui.h"
#include "speech.h"
#include "textcap.h"

namespace gd::screens {
using namespace gd::core;

// The loading screen's tip line ("^gTip: ^h..." -> "Tip: ..." once colour codes are stripped).
static bool find_tip(std::string& out) {
  for (const textcap::Item& it : textcap::snapshot()) {
    std::string t = textcap::speakable(it.text);
    if (t.rfind("Tip:", 0) == 0) { out = t; return true; }
  }
  return false;
}

class LoadingScreen : public Screen {
 public:
  std::string_view key() const override { return "loading"; }
  // App state 10 = entering the world (App::ApplyPendingState, exe+0xbe410); it holds until the world is up.
  bool is_active() override { return exe_ui::available() && exe_ui::app_state() == 10; }
  std::string screen_name() const override { return std::string(strings::kLoading); }
  int layer() const override { return 40; }
  bool exclusive() const override { return true; }
  bool owns_keyboard() const override { return false; }  // nothing to do here; let the game keep its keys
  std::vector<InputCategory> input_categories() const override { return {}; }
  bool start_unfocused() const override { return true; }
  void build(GraphBuilder&) override {}
  void on_focus() override {
    Screen::on_focus();  // "loading"
    std::string tip;
    if (find_tip(tip)) speech::speak(tip, false);  // game text, verbatim
  }
};

// Leaving the world (Exit to Main Menu, a lost connection): the app state stays 1 while the session is torn down --
// the world screen and its InGameUI are already gone and nothing is on the UI root (logged live 2026-10-05:
// "app_state=1 requested=0 world_screen=false ingame_ui=false ... root:" for ~100 ms, then app state 2 with the same empty
// root for ~90 ms, then the main menu). Saving and
// unloading take longer on a slower machine or when a hosted multiplayer session shuts down, and the unsupported
// fallback then said "this screen is not supported" on the way out. Quiet unless it lasts: "leaving the game" after
// 1.5 s, so a long teardown is not mistaken for a hang.
class LeavingScreen : public Screen {
 public:
  std::string_view key() const override { return "leaving"; }
  // Only after a world session was up (the boot title is never "leaving"); the main menu ends it.
  bool is_active() override {
    if (!exe_ui::available()) return false;
    if (exe_ui::ingame_ui()) { in_session_ = true; return false; }
    int st = exe_ui::app_state();
    if (st != 1 && st != 2) { if (st == 3 || st == 4 || st == 8) in_session_ = false; return false; }
    return in_session_ && !exe_ui::world_screen();
  }
  std::string screen_name() const override { return {}; }
  int layer() const override { return 40; }
  bool exclusive() const override { return true; }
  bool owns_keyboard() const override { return false; }
  std::vector<InputCategory> input_categories() const override { return {}; }
  bool start_unfocused() const override { return true; }
  void on_focus() override { since_ = 0; said_ = false; }
  void on_update() override {
    if (said_) return;
    if (since_ == 0) since_ = GetTickCount64();
    if (GetTickCount64() - since_ >= 1500) { said_ = true; speech::speak(strings::kLeavingGame, false); }
  }

 private:
  unsigned long long since_ = 0;
  bool said_ = false;
  bool in_session_ = false;
};

std::unique_ptr<Screen> make_loading() { return std::make_unique<LoadingScreen>(); }
std::unique_ptr<Screen> make_leaving() { return std::make_unique<LeavingScreen>(); }
}  // namespace gd::screens
