// The trade window (docs/exe-ui-layout.md "Trade"): four Tab stops over the game's own trade state -- the other
// player's offer (their iron bits and whether they accepted, then their items), your offer (your iron bits: Enter types
// an amount, Backspace offers none; then your items: Enter takes one back), your bags (Enter offers an item, the game's
// own shift-click while trading), and the Accept / Cancel buttons pressed through the window's registry. Every change
// the other player makes is announced. Items read like every other item list; Space / Ctrl+Space = the tooltip,
// Backslash = compare with what you wear.
#include "screens/trade.h"
#include <algorithm>
#include <format>
#include <set>
#include <string>
#include "app.h"
#include "core/graph_builder.h"
#include "core/message_builder.h"
#include "core/navigator.h"
#include "core/strings.h"
#include "exe_ui.h"
#include "gameapi.h"
#include "screens/count_prompt.h"
#include "screens/window_base.h"
#include "speech.h"

namespace gd::screens {
using namespace gd::core;

namespace {
std::string offer_value(unsigned gold, bool accepted) {
  MessageBuilder m;
  strings::push_stat(m.list_item(), strings::kIronBits, std::format("{}", gold));
  m.list_item().fragment(accepted ? strings::kAccepted : strings::kNotAccepted);
  return m.build();
}
}  // namespace

class TradeScreen : public WindowScreen {
 public:
  TradeScreen() : WindowScreen("trade", std::string(strings::kTrade), exe_ui::ingame::kTrade, 14) {}   // above the inventory the game opens beside it

  void on_focus() override {
    view_.invalidate(); bags_.invalidate();
    const gameapi::TradeView& v = view();
    partner_name_ = gameapi::player_name(v.partner);
    MessageBuilder m; m.fragment(strings::kTradingWith).fragment(partner_name_);
    speech::speak(m.build(), false);
    remember(v);
    WindowScreen::on_focus();
  }
  // Escape = the game's Cancel (closing the window cancels the trade anyway, exe+0x260b30).
  std::vector<ScreenAction> actions() override { return {{std::string(action_ids::Back), [] { exe_ui::trade_press(false); }}}; }

  // The other player's changes, spoken as they happen (the game shows them only in its grid).
  void on_update() override {
    view_.invalidate();
    const gameapi::TradeView& v = view();
    if (!seen_) { remember(v); return; }
    std::string who = partner_name_.empty() ? gameapi::player_name(v.partner) : partner_name_;
    std::set<unsigned> now;
    for (const gameapi::BagItem& it : v.theirs) {
      now.insert(it.id);
      if (!their_items_.count(it.id)) say(who, strings::kAdded, item_label(it));
    }
    for (const auto& [id, label] : their_labels_) if (!now.count(id)) say(who, strings::kRemoved, label);
    if (v.their_gold != their_gold_) say(who, strings::kOffers, offer_gold(v.their_gold));
    if (v.their_accepted != their_accepted_) say(who, v.their_accepted ? strings::kAccepted : strings::kNotAccepted, {});
    remember(v);
  }

  void build(GraphBuilder& b) override {
    const gameapi::TradeView& v = view();
    std::string who = partner_name_.empty() ? gameapi::player_name(v.partner) : partner_name_;
    // Their offer.
    b.begin_stop("theirs");
    { MessageBuilder t; t.fragment(who.empty() ? std::string() : std::format("{}'s", who)).fragment(strings::kTheirOffer);
      b.add_item(ControlId::structural("trade.theirs"), row_item(t.build(), [this] { const gameapi::TradeView& c = view(); return offer_value(c.their_gold, c.their_accepted); })); }
    for (const gameapi::BagItem& it : v.theirs) item_row(b, "trade.their", it, {});
    // Your offer: the gold row, then your items (Enter takes one back).
    b.begin_stop("yours");
    b.add_item(ControlId::structural("trade.gold"),
               row_item(std::string(strings::kYourOffer), [this] { const gameapi::TradeView& c = view(); return offer_value(c.my_gold, c.my_accepted); },
                        [this] { ask_gold(); }, [] { speech::speak(strings::kGoldRowHint, true); },
                        [this] { set_gold(0); }));
    for (const gameapi::BagItem& it : v.mine) {
      unsigned id = it.id; std::string label = item_label(it);
      item_row(b, "trade.mine", it, [this, id, label] {
        bool ok = gameapi::trade_take_back(id);
        MessageBuilder m; m.fragment(ok ? label : std::string(strings::kCannot)).fragment(ok ? strings::kTakenBack : std::string_view{});
        speech::speak(m.build(), true);
        invalidate();
      });
    }
    // Your bags: Enter offers the item.
    b.begin_stop("bags");
    const std::vector<gameapi::Bag>& bags = bags_.get([] { return gameapi::bags(); }, 30);
    bool any = false;
    for (const gameapi::Bag& bag : bags)
      for (const gameapi::BagItem& it : bag.items) {
        any = true;
        unsigned id = it.id; std::string label = item_label(it);
        item_row(b, "trade.bag", it, [this, id, label] { offer(id, label); });
      }
    if (!any) b.add_item(ControlId::structural("trade.bags.empty"), line_item(std::string(strings::kEmpty)));
    // The window's own buttons, captions from the game.
    b.begin_stop("buttons");
    b.start_row("buttons");
    for (bool accept : {true, false}) {
      std::string caption = exe_ui::trade_button_caption(accept);
      if (caption.empty()) caption = accept ? "Accept" : "Cancel";
      b.add_item(ControlId::structural(accept ? "trade.accept" : "trade.cancel"), row_item(caption, {}, [accept] {
        if (accept && exe_ui::trade_gold_invalid()) { speech::speak(strings::kNotEnoughBits, true); return; }
        if (!exe_ui::trade_press(accept)) speech::speak(strings::kCannot, true);
      }));
    }
    b.end_row();
  }

 private:
  const gameapi::TradeView& view() { return view_.get([] { return gameapi::trade_view(); }, 0); }
  void invalidate() { view_.invalidate(); bags_.invalidate(); }
  void item_row(GraphBuilder& b, const char* prefix, const gameapi::BagItem& it, std::function<void()> activate) {
    auto v = row_item(item_label(it), {}, std::move(activate), item_tip(it.id, false), {}, item_tip(it.id, true));
    v->on_compare = item_compare(it.id);
    b.add_item(ControlId::structural(std::format("{}{}", prefix, it.id)), v);
  }
  static std::string offer_gold(unsigned gold) { MessageBuilder m; m.fragment(std::format("{}", gold)).fragment(strings::kIronBits); return m.build(); }
  static void say(const std::string& who, std::string_view what, const std::string& thing) {
    MessageBuilder m; m.fragment(who).fragment(what).fragment(thing);
    speech::speak(m.build(), false);   // the other player's doing: never cut what is being read
  }
  void remember(const gameapi::TradeView& v) {
    seen_ = true;
    their_items_.clear(); their_labels_.clear();
    for (const gameapi::BagItem& it : v.theirs) { their_items_.insert(it.id); their_labels_.emplace_back(it.id, item_label(it)); }
    their_gold_ = v.their_gold; their_accepted_ = v.their_accepted;
  }
  void offer(unsigned id, const std::string& label) {
    // The item's own gate first, as the bag's shift-click does (refused -> the game's tagTransferStashError).
    if (!gameapi::trade_item_allowed(id)) {
      std::string why = gameapi::localize("tagTransferStashError");
      speech::speak(why.empty() ? std::string(strings::kCannot) : why, true);
      return;
    }
    bool ok = exe_ui::trade_quick_add(id);
    MessageBuilder m; m.fragment(ok ? label : std::string(strings::kCannot)).fragment(ok ? strings::kAdded : std::string_view{});
    speech::speak(m.build(), true);
    invalidate();
  }
  void ask_gold() {
    unsigned have = gameapi::money();
    if (have == 0) { speech::speak(strings::kNoIronBits, true); return; }
    MessageBuilder t; t.fragment(strings::kOfferHowMuch).fragment(std::format("{}", have));
    open_count_prompt(t.build(), have, [this](unsigned n) { set_gold(n); });
  }
  void set_gold(unsigned n) {
    bool ok = exe_ui::trade_set_gold(n);
    speech::speak(ok ? offer_gold(n) : std::string(strings::kCannot), true);
    invalidate();
  }

  Snapshot<gameapi::TradeView> view_;
  Snapshot<std::vector<gameapi::Bag>> bags_;
  std::string partner_name_;
  bool seen_ = false;
  std::set<unsigned> their_items_;
  std::vector<std::pair<unsigned, std::string>> their_labels_;
  unsigned their_gold_ = 0;
  bool their_accepted_ = false;
};

std::unique_ptr<Screen> make_trade() { return std::make_unique<TradeScreen>(); }
}  // namespace gd::screens
