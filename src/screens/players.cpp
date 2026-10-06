// The other players in a multiplayer game (Ctrl+K in the world): what a sighted player reaches through the party
// portraits' right-click menu and the group window, for everyone in the game, through gameapi_players.cpp (exports
// only, each action behind the exe's own gate). One row per player; Enter = the actions the game would offer for
// them, named with the game's own menu words; Space = where they are; a last row leaves your party when you are in
// one. A trade or inspect the game accepts opens the game's own window; an invitation or trade request the other
// player answers arrives as the game's message box.
#include "screens/players.h"
#include <format>
#include <string>
#include <vector>
#include "core/graph_builder.h"
#include "core/message_builder.h"
#include "core/screen.h"
#include "core/strings.h"
#include "gameapi.h"
#include "screens/list_picker.h"
#include "screens/window_base.h"
#include "speech.h"
#include "world.h"

namespace gd::screens {
using namespace gd::core;

namespace {
bool g_open = false;

enum Action : unsigned { kTrade = 1, kInspect, kWhisper, kInvite, kRemove, kBan };
// The menu's own captions (Text_EN: tagPortraitTrade "Trade", tagInspectOption "Inspect", tagPortraitChat "Whisper",
// tagPortraitRemove "Remove from Party", tagMPBan "Ban"); the group window's invite has no tag of its own here.
std::string caption(Action a) {
  const char* tag = nullptr; const char* fallback = "";
  switch (a) {
    case kTrade: tag = "tagPortraitTrade"; fallback = "Trade"; break;
    case kInspect: tag = "tagInspectOption"; fallback = "Inspect"; break;
    case kWhisper: tag = "tagPortraitChat"; fallback = "Whisper"; break;
    case kRemove: tag = "tagPortraitRemove"; fallback = "Remove from party"; break;
    case kBan: tag = "tagMPBan"; fallback = "Ban"; break;
    case kInvite: return std::string(strings::kInviteToParty);
  }
  std::string t = gameapi::localize(tag);
  return t.empty() ? std::string(fallback) : t;
}
void say_result(gameapi::PlayerResult r, std::string_view done) {
  switch (r) {
    case gameapi::PlayerResult::Done: if (!done.empty()) speech::speak(done, true); break;
    case gameapi::PlayerResult::TooFar: speech::speak(strings::kTooFar, true); break;
    default: speech::speak(strings::kCannot, true); break;
  }
}
void run(unsigned id, std::string name, unsigned action) {
  switch (action) {
    case kTrade: say_result(gameapi::trade_with(id), strings::kTradeRequested); break;
    case kInspect: say_result(gameapi::inspect_player(id), {}); break;   // the game's inspect window speaks for itself
    case kWhisper: if (!gameapi::whisper_player(id)) speech::speak(strings::kCannot, true); break;   // the chat box opens
    case kInvite: { MessageBuilder m; m.fragment(name).fragment(gameapi::invite_player(id) ? strings::kInvited : strings::kCannot); speech::speak(m.build(), true); break; }
    case kRemove: if (!gameapi::kick_player(id)) speech::speak(strings::kCannot, true); break;
    case kBan: if (!gameapi::ban_player(id)) speech::speak(strings::kCannot, true); break;
  }
}
void open_actions(unsigned id, const std::string& name) {
  gameapi::PlayerActions a = gameapi::player_actions(id);
  std::vector<PickerItem> items;
  // The portrait menu's order (exe+0x253f90): Remove, Inspect, Trade, Whisper, Ban; the group window's Invite first.
  if (a.invite) items.push_back({kInvite, caption(kInvite), {}});
  if (a.remove) items.push_back({kRemove, caption(kRemove), {}});
  if (a.inspect) items.push_back({kInspect, caption(kInspect), {}});
  if (a.trade) items.push_back({kTrade, caption(kTrade), {}});
  if (a.whisper) items.push_back({kWhisper, caption(kWhisper), {}});
  if (a.ban) items.push_back({kBan, caption(kBan), {}});
  if (items.empty()) { speech::speak(strings::kCannot, true); return; }
  g_open = false;   // the picker replaces the overlay; an action opens a game window or the chat box
  open_picker(name, std::move(items), [id, name](unsigned action) { run(id, name, action); });
}
}  // namespace

void open_players() {
  if (gameapi::other_players().empty()) { speech::speak(strings::kNoOtherPlayers, true); return; }
  g_open = true;
}

class PlayersScreen : public Screen {
 public:
  std::string_view key() const override { return "players"; }
  bool is_active() override { return g_open && world::in_world(); }
  std::string screen_name() const override { return std::string(strings::kPlayers); }
  int layer() const override { return 1; }   // like the pet overlay: a game window covers and closes it
  std::vector<InputCategory> input_categories() const override { return {InputCategory::UI}; }
  std::vector<ScreenAction> actions() override { return {{std::string(action_ids::Back), [] { g_open = false; }}}; }
  void on_pop() override { g_open = false; }

  void build(GraphBuilder& b) override {
    b.begin_stop("page");
    std::vector<gameapi::OtherPlayer> ps = gameapi::other_players();
    bool in_party = false;
    for (const gameapi::OtherPlayer& p : ps) {
      in_party = in_party || p.in_party;
      MessageBuilder m;
      strings::push_player_row(m, p.name, p.level, p.class_name, p.in_party, p.leader, -1);   // distance on Space: it changes every frame
      unsigned id = p.id;
      std::string name = p.name;
      auto where = [id] {
        world::Vec3 me;
        for (const gameapi::OtherPlayer& c : gameapi::other_players()) {
          if (c.id != id) continue;
          if (!c.has_pos || !world::player_position(me)) { speech::speak(strings::kUnknown, true); return; }
          MessageBuilder w; strings::push_distance_bearing(w, (float)c.distance, world::clock_hour(c.pos));
          speech::speak(w.build(), true);
          return;
        }
      };
      b.add_item(ControlId::structural(std::format("player.{}", id)), row_item(m.build(), {}, [id, name] { open_actions(id, name); }, where));
    }
    if (ps.empty()) { b.add_item(ControlId::structural("player.none"), line_item(std::string(strings::kNoOtherPlayers))); return; }
    if (in_party)
      b.add_item(ControlId::structural("player.leave"), row_item(std::string(strings::kLeaveParty), {}, [] {
        speech::speak(gameapi::leave_party() ? strings::kLeftParty : strings::kCannot, true);
      }));
  }
};

std::unique_ptr<Screen> make_players() { return std::make_unique<PlayersScreen>(); }
}  // namespace gd::screens
