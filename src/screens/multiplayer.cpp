#include "screens/multiplayer.h"
#include <format>
#include <iterator>
#include <utility>
#include "core/graph_builder.h"
#include "core/message_builder.h"
#include "core/server_row.h"
#include "core/strings.h"
#include "exe_ui.h"
#include "gameapi.h"
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

NodeVtablePtr tab_item(WidgetA t) {
  std::string label = t.caption();
  bool selected = t.pressed();
  auto v = std::make_shared<NodeVtable>();
  v->control_type = &kTabType;
  v->announcements = {NodeAnnouncement([label] { return label; }, false, announcement_kinds::kLabel),
                      NodeAnnouncement([selected] { return selected ? std::string(strings::kSelected) : std::string(); }, true, announcement_kinds::kSelected)};
  v->on_activate = [t, selected] { if (!selected) t.activate(); };   // a pressed mode toggle would flip off: the game keeps one of the pair on
  v->state_text = [t] { return std::string(t.pressed() ? strings::kSelected : strings::kNotSelected); };
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
  bool is_active() override { return exe_ui::available() && (bool)exe_ui::multiplayer_screen() && !exe_ui::popup(); }
  std::string screen_name() const override { return std::string(strings::kMultiplayerScreen); }
  int layer() const override { return 0; }
  bool captures_raw_input() const override { return edit_.active(); }
  bool allows_typeahead() const override { return !edit_.active(); }
  bool passes_key(int code) const override { return edit_.passes_key(code); }
  std::vector<ScreenAction> actions() override {
    return {{std::string(action_ids::Back), [] { MultiplayerScreen s = exe_ui::multiplayer_screen(); if (s) press(s.member(MultiplayerScreen::kBack)); }}};
  }
  // Ctrl+Tab: Join <-> Host (the pair's other tab; the game keeps exactly one pressed).
  bool switch_tab(int) override {
    MultiplayerScreen s = exe_ui::multiplayer_screen();
    if (!s) return false;
    bool to_host = !s.hosting();
    WidgetA other = s.member(to_host ? MultiplayerScreen::kHostTab : MultiplayerScreen::kJoinTab);
    if (!other) return false;
    other.activate();
    // Land on the new tab (focus could otherwise sit on Back, which both tabs share, and nothing would be spoken).
    if (core::GraphNavigator* nav = app::navigator()) nav->focus_node(ControlId::structural(to_host ? "mp.host" : "mp.join"), false);
    MessageBuilder m;
    strings::push_control(m, other.caption(), strings::kTab, false, false);
    speech::speak(m.build(), true);
    return true;
  }
  void on_push() override { edit_.stop(); }
  void on_unfocus() override { edit_.stop(); }
  void on_update() override { if (edit_.active()) edit_.tick(field_box(field_)); }

  void build(GraphBuilder& b) override {
    MultiplayerScreen s = exe_ui::multiplayer_screen();
    if (!s) return;
    bool hosting = s.hosting();
    b.begin_stop("modes");
    b.start_row("mode");
    b.add_item(ControlId::structural("mp.join"), tab_item(s.member(MultiplayerScreen::kJoinTab)));
    b.add_item(ControlId::structural("mp.host"), tab_item(s.member(MultiplayerScreen::kHostTab)));
    b.end_row();
    b.start_row("network");
    b.add_item(ControlId::structural("mp.internet"), tab_item(s.member(MultiplayerScreen::kInternet)));
    b.add_item(ControlId::structural("mp.lan"), tab_item(s.member(MultiplayerScreen::kLan)));
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

  void build_filters(GraphBuilder& b, const MultiplayerScreen& s) {
    Filters f = filters(s);
    b.begin_stop("filters");
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
};

std::unique_ptr<Screen> make_multiplayer() { return std::make_unique<MultiplayerMenuScreen>(); }
}  // namespace gd::screens
