#include "screens/multiplayer.h"
#include <format>
#include <iterator>
#include <optional>
#include <utility>
#include "core/graph_builder.h"
#include "core/message_builder.h"
#include "core/server_row.h"
#include "core/strings.h"
#include "exe_ui.h"
#include "gameapi.h"
#include "hooks.h"
#include "screens/controls.h"
#include "screens/edit_field.h"
#include "speech.h"
#include "textcap.h"

namespace gd::screens {
using namespace gd::core;
using exe_ui::MultiplayerScreen;
using exe_ui::WidgetA;

namespace {
const ControlType kServerRowType{"server", {"label", "position"}, [] { return std::vector<NodeAnnouncement>{}; }};
const ControlType kFriendRowType{"friend", {"label", "value", "position"}, [] { return std::vector<NodeAnnouncement>{}; }};
const ControlType kPickerRowType{"character", {"label", "value", "selected", "position"}, [] { return std::vector<NodeAnnouncement>{}; }};
const ControlType kCountLineType{"text", {"value"}, [] { return std::vector<NodeAnnouncement>{}; }};

// The filter bar, in its tree order (measured 2026-10-04): the refresh icon button, "Game Name" + its edit box, "Type"
// + its drop-down, six unlabeled drop-downs, then the Hide Full / Hide Locked / Hide In Progress check boxes (the last
// one inactive in a plain game). A TEXT widget labels the control right after it, as on the Options pages.
struct Filters {
  WidgetA refresh, name;
  std::string name_label;
  std::vector<std::pair<std::string, WidgetA>> combos;
  std::vector<WidgetA> toggles;
};
Filters filters(const MultiplayerScreen& s) {
  Filters f;
  std::string pending;
  size_t unlabeled = 0;
  for (WidgetA c : s.member(MultiplayerScreen::kFilterBar).children()) {
    if (!c.active()) continue;
    if (c.is_text()) { pending = textcap::speakable(c.text()); continue; }
    if (c.is_edit()) { f.name = c; f.name_label = pending; }
    else if (c.is_combo()) {
      std::string label = pending;
      if (label.empty() && unlabeled < std::size(strings::kServerFilterLabels)) label = std::string(strings::kServerFilterLabels[unlabeled++]);
      f.combos.push_back({label, c});
    } else if (c.is_button() && c.is_toggle()) f.toggles.push_back(c);
    else if (c.is_button()) f.refresh = c;
    pending.clear();
  }
  return f;
}

// A button press with the game's own "greyed" gate spoken instead of ignored.
void press(WidgetA w) {
  if (!w) return;
  if (!w.enabled()) { speech::speak(strings::kDisabled, true); return; }
  w.activate();
}

// Join / Host as the windows' tab strip (window_base.h add_tabs): landing on a tab OPENS it, so the tab under the cursor is
// always the open one and no "selected" is spoken. Enter re-selects (a no-op on the open tab: the game keeps one of the
// pair pressed, and pressing the open one again would flip it off).
NodeVtablePtr tab_item(WidgetA t) {
  std::string label = t.caption();
  auto v = std::make_shared<NodeVtable>();
  v->control_type = &kTabType;
  v->announcements = {NodeAnnouncement([label] { return label; }, false, announcement_kinds::kLabel)};
  v->on_focus = [t] { if (!t.pressed()) t.activate(); };
  v->on_activate = [t] { if (!t.pressed()) t.activate(); };
  return v;
}
// Internet / LAN: a two-way choice, not a tab strip -- one drop-down-like row ("network, Internet"), Left/Right switch.
std::string network_value(WidgetA internet, WidgetA lan) {
  return textcap::speakable(lan.pressed() ? lan.caption() : internet.caption());
}
NodeVtablePtr network_item(WidgetA internet, WidgetA lan) {
  std::string value = network_value(internet, lan);
  auto v = std::make_shared<NodeVtable>();
  v->control_type = &kComboType;
  v->announcements = {NodeAnnouncement([] { return std::string(strings::kNetwork); }, false, announcement_kinds::kLabel),
                      NodeAnnouncement([value] { return value; }, true, announcement_kinds::kValue)};
  v->on_adjust = [internet, lan](int sign, bool) {
    WidgetA to = sign > 0 ? lan : internet;
    if (to && !to.pressed()) to.activate();
  };
  v->state_text = [internet, lan] {
    MessageBuilder m;
    m.fragment(network_value(internet, lan));
    strings::push_position(m, lan.pressed() ? 2 : 1, 2);
    return m.build();
  };
  return v;
}
NodeVtablePtr checkbox_item(WidgetA c) {
  std::string label = c.caption();
  bool on = c.pressed();
  auto v = std::make_shared<NodeVtable>();
  v->control_type = &kToggleType;
  v->announcements = {NodeAnnouncement([label] { return label; }, false, announcement_kinds::kLabel),
                      NodeAnnouncement([on] { return std::string(on ? strings::kOn : strings::kOff); }, true, announcement_kinds::kValue)};
  v->on_activate = [c] { c.activate(); };
  v->state_text = [c] { return std::string(c.pressed() ? strings::kOn : strings::kOff); };
  return v;
}
std::string combo_value(WidgetA c) {
  std::vector<std::string> items = c.combo_items();
  int i = c.combo_index();
  return i >= 0 && i < (int)items.size() ? textcap::speakable(items[(size_t)i]) : std::string(strings::kEmpty);
}
NodeVtablePtr combo_item(std::string label, WidgetA c) {
  std::string value = combo_value(c);
  auto v = std::make_shared<NodeVtable>();
  v->control_type = &kComboType;
  v->announcements = {NodeAnnouncement([label] { return label; }, false, announcement_kinds::kLabel),
                      NodeAnnouncement([value] { return value; }, true, announcement_kinds::kValue)};
  v->on_adjust = [c](int sign, bool) {
    int n = (int)c.combo_items().size(), i = c.combo_index() + sign;
    if (n == 0) return;
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    if (i != c.combo_index()) c.set_combo(i);
  };
  v->state_text = [c] {
    MessageBuilder m;
    m.fragment(combo_value(c));
    strings::push_position(m, c.combo_index() + 1, (int)c.combo_items().size());
    return m.build();
  };
  return v;
}
NodeVtablePtr count_item(std::string text) {
  auto v = std::make_shared<NodeVtable>();
  v->control_type = &kCountLineType;
  v->announcements = {NodeAnnouncement([text] { return text; }, false, announcement_kinds::kValue)};
  return v;
}
}  // namespace

class MultiplayerMenuScreen : public Screen {
 public:
  std::string_view key() const override { return "multiplayer"; }
  // The screen surfaces only once it has stood without a popup for a few frames: the connecting box (or the password
  // prompt's Ok) closes a few frames BEFORE the app enters the loading state, and the screen showing in between read
  // "Multiplayer" and the focused control ahead of "loading" (user log 2026-10-16: 7 ms). Counted in frames, below the
  // unsupported fallback's 30-frame debounce, so the gap is silent; a real return to the screen waits ~15 frames.
  bool is_active() override {
    if (!exe_ui::available() || !exe_ui::multiplayer_screen() || exe_ui::popup()) { quiet_frames_ = 0; return false; }
    uint64_t f = hooks::frame();
    if (f != last_frame_) { last_frame_ = f; if (quiet_frames_ < kSettleFrames) ++quiet_frames_; }
    return quiet_frames_ >= kSettleFrames;
  }
  std::string screen_name() const override { return std::string(strings::kMultiplayerScreen); }
  int layer() const override { return 0; }
  bool captures_raw_input() const override { return edit_.active(); }
  bool allows_typeahead() const override { return !edit_.active(); }
  bool passes_key(int code) const override { return edit_.passes_key(code); }
  std::vector<ScreenAction> actions() override {
    return {{std::string(action_ids::Back), [] { MultiplayerScreen s = exe_ui::multiplayer_screen(); if (s) press(s.member(MultiplayerScreen::kBack)); }}};
  }
  // Ctrl+Tab / Ctrl+Shift+Tab: Join <-> Host from anywhere, as in the windows: land on the opened tab and say "label, tab".
  bool switch_tab(int) override {
    MultiplayerScreen s = exe_ui::multiplayer_screen();
    if (!s) return false;
    bool to_host = !s.hosting();
    WidgetA other = s.member(to_host ? MultiplayerScreen::kHostTab : MultiplayerScreen::kJoinTab);
    if (!other) return false;
    other.activate();
    if (core::GraphNavigator* nav = app::navigator()) nav->focus_node(tab_id(to_host), false);
    MessageBuilder m;
    strings::push_control(m, other.caption(), strings::kTab, false, false);
    speech::speak(m.build(), true);
    return true;
  }
  // Every popup ("Building server list" after a refresh, a refused join, the password prompt) takes this screen off the
  // stack and its nav state with it. Coming back from one lands where the player was; a fresh entry lands on the OPEN
  // tab, not the first one, so the tab under the cursor is the one showing.
  void on_push() override { edit_.stop(); seat_ = true; }
  void on_unfocus() override {
    edit_.stop();
    core::GraphNavigator* nav = app::navigator();
    return_to_ = exe_ui::popup() && nav ? nav->focused_id() : std::nullopt;
  }
  void on_update() override {
    if (seat_) {
      seat_ = false;
      MultiplayerScreen s = exe_ui::multiplayer_screen();
      core::GraphNavigator* nav = app::navigator();
      if (s && nav) nav->focus_node(return_to_ ? *return_to_ : tab_id(s.hosting()));
      return_to_.reset();
    }
    if (edit_.active()) edit_.tick(field_box(field_));
  }

  void build(GraphBuilder& b) override {
    MultiplayerScreen s = exe_ui::multiplayer_screen();
    if (!s) return;
    bool hosting = s.hosting();
    b.begin_stop("tabs");
    b.start_row("tabs");
    b.add_item(tab_id(false), tab_item(s.member(MultiplayerScreen::kJoinTab)));
    b.add_item(tab_id(true), tab_item(s.member(MultiplayerScreen::kHostTab)));
    b.end_row();
    if (hosting) build_host(b, s);
    else {
      build_filters(b, s);
      build_servers(b, s);
      build_friends(b, s);
    }
    build_characters(b, s);
    b.begin_stop("buttons");
    b.start_row("buttons");
    b.add_item(ControlId::structural("mp.start"), widget_button(s.member(MultiplayerScreen::kStart)));   // Join: the focused server; Host: host
    b.add_item(ControlId::structural("mp.difficulty"), widget_button(s.member(MultiplayerScreen::kDifficulty)));
    b.add_item(ControlId::structural("mp.gamemode"), widget_button(s.member(MultiplayerScreen::kGameMode)));
    b.add_item(ControlId::structural("mp.back"), widget_button(s.member(MultiplayerScreen::kBack)));
    b.end_row();
  }

 private:
  // The edit box a typing session is in, re-resolved every frame (the widgets are rebuilt on a mode switch).
  enum class Field { None, FilterName, HostName, HostPassword, HostLevelRange };
  static WidgetA field_box(Field f) {
    MultiplayerScreen s = exe_ui::multiplayer_screen();
    if (!s) return {};
    switch (f) {
      case Field::FilterName: return filters(s).name;
      case Field::HostName: return s.member(MultiplayerScreen::kHostName);
      case Field::HostPassword: return s.member(MultiplayerScreen::kHostPassword);
      case Field::HostLevelRange: return s.member(MultiplayerScreen::kHostLevelRange);
      default: return {};
    }
  }
  NodeVtablePtr edit(std::string label, Field f, WidgetA box) {
    NodeVtablePtr v = edit_item(std::move(label), box, edit_);
    v->on_activate = [this, f, box] { if (edit_.active()) edit_.stop(); else { field_ = f; edit_.start(box); } };
    return v;
  }
  static std::string label_of(const MultiplayerScreen& s, unsigned off) { return textcap::speakable(s.member(off).text()); }
  static ControlId tab_id(bool host) { return ControlId::structural(host ? "mp.host" : "mp.join"); }
  static void add_network(GraphBuilder& b, const MultiplayerScreen& s) {
    WidgetA internet = s.member(MultiplayerScreen::kInternet), lan = s.member(MultiplayerScreen::kLan);
    if (internet && lan) b.add_item(ControlId::structural("mp.network"), network_item(internet, lan));
  }

  void build_filters(GraphBuilder& b, const MultiplayerScreen& s) {
    Filters f = filters(s);
    b.begin_stop("filters");
    add_network(b, s);
    if (f.refresh) b.add_item(ControlId::structural("mp.refresh"), widget_button(f.refresh, std::string(strings::kRefresh)));
    if (f.name) b.add_item(ControlId::structural("mp.filter.name"), edit(f.name_label, Field::FilterName, f.name));
    int i = 0;
    for (const auto& [label, c] : f.combos) b.add_item(ControlId::structural(std::format("mp.filter.combo{}", i++)), combo_item(label, c));
    for (WidgetA t : f.toggles) b.add_item(ControlId::structural("mp.filter." + t.caption()), checkbox_item(t));
  }

  // One row per game, in the game's own (sorted, filtered) order. Landing on a row selects it in the game -- what a
  // single click does -- so Start always joins the game last heard; Enter joins it (the game's double click).
  void build_servers(GraphBuilder& b, const MultiplayerScreen& s) {
    WidgetA list = s.member(MultiplayerScreen::kServerList);
    std::vector<std::vector<std::string>> rows = list.list_rows();
    b.begin_stop("servers");
    b.add_item(ControlId::structural("mp.servers.count"), count_item(label_of(s, MultiplayerScreen::kCountText)));
    b.push_context(strings::kServers, strings::kList);
    for (size_t i = 0; i < rows.size(); ++i) {
      MessageBuilder m;
      strings::push_server_row(m, parse_server_row(rows[i]));
      std::string text = m.build();
      int index = (int)i;
      auto v = std::make_shared<NodeVtable>();
      v->control_type = &kServerRowType;
      v->announcements = {NodeAnnouncement([text] { return text; }, false, announcement_kinds::kLabel)};
      v->on_focus = [index] { MultiplayerScreen ms = exe_ui::multiplayer_screen(); if (ms) ms.member(MultiplayerScreen::kServerList).list_select(index); };
      v->on_activate = [index] {
        MultiplayerScreen ms = exe_ui::multiplayer_screen();
        if (!ms) return;
        ms.member(MultiplayerScreen::kServerList).list_select(index);
        press(ms.member(MultiplayerScreen::kStart));
      };
      b.add_item(ControlId::structural(std::format("mp.server.{}", i)), v);
    }
    b.pop_context();
  }

  // The Steam friends list: a row's cell is "Name\nStatus". Landing selects the friend; Enter is Join Game (enabled by
  // the game when that friend is in a game you can join).
  void build_friends(GraphBuilder& b, const MultiplayerScreen& s) {
    WidgetA list = s.member(MultiplayerScreen::kFriendsList);
    std::vector<std::vector<std::string>> rows = list.list_rows();
    b.begin_stop("friends");
    b.push_context(strings::kFriends, strings::kList);
    for (size_t i = 0; i < rows.size(); ++i) {
      std::string cell = rows[i].size() > 1 ? rows[i][1] : (rows[i].empty() ? std::string() : rows[i][0]);
      size_t nl = cell.find('\n');
      std::string name = textcap::speakable(cell.substr(0, nl)), status = nl == std::string::npos ? std::string() : textcap::speakable(cell.substr(nl + 1));
      int index = (int)i;
      auto v = std::make_shared<NodeVtable>();
      v->control_type = &kFriendRowType;
      v->announcements = {NodeAnnouncement([name] { return name; }, false, announcement_kinds::kLabel),
                          NodeAnnouncement([status] { return status; }, false, announcement_kinds::kValue)};
      v->on_focus = [index] { MultiplayerScreen ms = exe_ui::multiplayer_screen(); if (ms) ms.member(MultiplayerScreen::kFriendsList).list_select(index); };
      v->on_activate = [index] {
        MultiplayerScreen ms = exe_ui::multiplayer_screen();
        if (!ms) return;
        ms.member(MultiplayerScreen::kFriendsList).list_select(index);
        press(ms.member(MultiplayerScreen::kJoinFriend));
      };
      b.add_item(ControlId::structural(std::format("mp.friend.{}", i)), v);
    }
    b.pop_context();
    b.add_item(ControlId::structural("mp.joinfriend"), widget_button(s.member(MultiplayerScreen::kJoinFriend)));
    b.add_item(ControlId::structural("mp.addfriend"), widget_button(s.member(MultiplayerScreen::kAddFriend)));
  }

  // Host settings in reading order: the left column (name, password), the middle (level range, max players), then the
  // check boxes. Every label is the game's own text widget.
  void build_host(GraphBuilder& b, const MultiplayerScreen& s) {
    b.begin_stop("host");
    add_network(b, s);
    b.push_context(strings::kHostSettings);
    b.add_item(ControlId::structural("mp.host.name"), edit(label_of(s, MultiplayerScreen::kHostNameLabel), Field::HostName, s.member(MultiplayerScreen::kHostName)));
    b.add_item(ControlId::structural("mp.host.password"), edit(label_of(s, MultiplayerScreen::kHostPasswordLabel), Field::HostPassword, s.member(MultiplayerScreen::kHostPassword)));
    b.add_item(ControlId::structural("mp.host.levelrange"), edit(label_of(s, MultiplayerScreen::kHostLevelRangeLabel), Field::HostLevelRange, s.member(MultiplayerScreen::kHostLevelRange)));
    WidgetA max_players = s.member(MultiplayerScreen::kHostMaxPlayers);
    if (max_players.is_combo()) b.add_item(ControlId::structural("mp.host.maxplayers"), combo_item(label_of(s, MultiplayerScreen::kHostMaxPlayersLabel), max_players));
    for (unsigned off : {MultiplayerScreen::kHostAutoParty, MultiplayerScreen::kHostInstancedLoot, MultiplayerScreen::kHostPvp}) {
      WidgetA t = s.member(off);
      if (t.is_button()) b.add_item(ControlId::structural(std::format("mp.host.toggle{:x}", off)), checkbox_item(t));
    }
    b.pop_context();
  }

  // The character you play as (the same picker class as the main menu's). Enter selects.
  void build_characters(GraphBuilder& b, const MultiplayerScreen& s) {
    std::vector<exe_ui::MainMenu::Character> chars = s.characters();
    if (chars.empty()) return;
    int selected = s.selected_character();
    b.begin_stop("characters");
    b.push_context(strings::kCharacters, strings::kList);
    for (size_t i = 0; i < chars.size(); ++i) {
      const exe_ui::MainMenu::Character& c = chars[i];
      std::string cls = c.class_tag.empty() ? std::string() : gameapi::localize(c.class_tag);
      std::string value;
      { MessageBuilder m; strings::push_character_summary(m, c.level, cls, c.hardcore); value = m.build(); }
      bool sel = (int)i == selected;
      auto v = std::make_shared<NodeVtable>();
      v->control_type = &kPickerRowType;
      v->announcements = {NodeAnnouncement([name = c.name] { return name; }, false, announcement_kinds::kLabel),
                          NodeAnnouncement([value] { return value; }, false, announcement_kinds::kValue),
                          NodeAnnouncement([sel] { return sel ? std::string(strings::kSelected) : std::string(); }, true, announcement_kinds::kSelected)};
      v->on_activate = [i] { MultiplayerScreen ms = exe_ui::multiplayer_screen(); if (ms) ms.select_character((int)i); };
      b.add_item(ControlId::structural("mp.character." + c.name), v);
    }
    b.pop_context();
  }

  EditSession edit_;
  Field field_ = Field::None;
  bool seat_ = false;
  int quiet_frames_ = 0;                 // consecutive frames the screen has been up with no popup
  uint64_t last_frame_ = 0;
  static constexpr int kSettleFrames = 15;
  std::optional<ControlId> return_to_;   // the focused control when a popup covered the screen
};

std::unique_ptr<Screen> make_multiplayer() { return std::make_unique<MultiplayerMenuScreen>(); }
}  // namespace gd::screens
