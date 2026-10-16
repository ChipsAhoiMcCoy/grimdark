#include "screens/message_box.h"
#include <optional>
#include "app.h"
#include "core/graph_builder.h"
#include "core/message_builder.h"
#include "core/navigator.h"
#include "core/strings.h"
#include "exe_ui.h"
#include "screens/controls.h"
#include "screens/edit_field.h"
#include "speech.h"
#include "textcap.h"

namespace gd::screens {
using namespace gd::core;
using exe_ui::WidgetA;

static const ControlType kTextType{"text", {"value"}, [] { return std::vector<NodeAnnouncement>{}; }};

// The game's message boxes, from two structured sources:
//  - in the world, the exported DialogManager behind the prompt box (text, Okay or Yes/No type; answered by
//    posting the response the box's own buttons would post);
//  - in the menus, a popup window of the menu tree ("A character with that name already exists." with Ok):
//    its text widgets and its buttons, and the edit box when it has one (the join password prompt: Down from the
//    message to "Password, edit", Enter to type, Enter to stop, then Ok).
// One box can change in place: joining a game shows "Connecting to Server ..." and, for a passworded game, the SAME
// popup becomes the password prompt (user log 2026-10-16: silent until an arrow key). A new message is announced like
// a box that just opened: focus goes back to the message and the navigator's landing reads it.
class MessageBoxScreen : public Screen {
 public:
  std::string_view key() const override { return "message_box"; }
  bool is_active() override { return exe_ui::available() && (exe_ui::dialog_open() || (bool)exe_ui::popup()); }
  std::string screen_name() const override { return std::string(strings::kMessage); }
  int layer() const override { return 30; }
  bool exclusive() const override { return true; }
  bool captures_raw_input() const override { return edit_.active(); }
  bool allows_typeahead() const override { return !edit_.active(); }
  bool passes_key(int code) const override { return edit_.passes_key(code); }
  void on_push() override { edit_.stop(); shown_.reset(); settle_ = 0; }
  void on_unfocus() override { edit_.stop(); }
  void on_update() override {
    // A changed box is re-read once its content has held for a few frames (the text and the edit box need not
    // arrive in the same frame, and one change must not be read twice).
    std::string now = content_key();
    if (!shown_) shown_ = now;
    else if (*shown_ == now) settle_ = 0;
    else if (now != pending_) { pending_ = now; settle_ = 1; }
    else if (++settle_ >= kSettleFrames) {
      shown_ = now; settle_ = 0;
      edit_.stop();
      if (GraphNavigator* nav = app::navigator()) nav->blur();   // the next pull re-seats on the message and reads it
    }
    if (edit_.active()) { exe_ui::Popup p = exe_ui::popup(); edit_.tick(p ? p.edit() : WidgetA{}); }
  }
  std::vector<ScreenAction> actions() override {
    return {{std::string(action_ids::Back), [] {
      if (exe_ui::dialog_open()) { exe_ui::answer_dialog(false); return; }  // No, or just closes an Okay box
      exe_ui::Popup p = exe_ui::popup();
      std::vector<WidgetA> btns = p ? p.buttons() : std::vector<WidgetA>{};
      if (!btns.empty()) btns.back().activate();
    }}};
  }

  void build(GraphBuilder& b) override {
    exe_ui::Popup popup;
    bool dialog = exe_ui::dialog_open();
    if (!dialog) popup = exe_ui::popup();
    std::string text = message_text(dialog, popup);
    if (text.empty()) text = strings::kNoDetails;
    b.begin_stop("message");
    {
      auto v = std::make_shared<NodeVtable>();
      v->control_type = &kTextType;
      v->announcements = {NodeAnnouncement([text] { return text; }, false, announcement_kinds::kValue)};
      b.add_item(ControlId::structural("message_box.text"), v);
    }
    if (WidgetA box = popup ? popup.edit() : WidgetA{})
      b.add_item(ControlId::structural("message_box.edit"), edit_item(textcap::speakable(popup.edit_label().text()), box, edit_));
    b.begin_stop("buttons");
    b.start_row("buttons");
    if (dialog) {
      if (exe_ui::dialog_type() == 1) {
        b.add_item(ControlId::structural("message_box.Yes"), answer_button(strings::kYes, true));
        b.add_item(ControlId::structural("message_box.No"), answer_button(strings::kNo, false));
      } else {
        b.add_item(ControlId::structural("message_box.Okay"), answer_button(strings::kOkay, true));
      }
    } else if (popup) {
      int i = 0;
      for (WidgetA w : popup.buttons()) b.add_item(ControlId::structural(std::format("message_box.popup{}", i++)), widget_button(w));
    }
    b.end_row();
  }


 private:
  static std::string message_text(bool dialog, const exe_ui::Popup& popup) {
    return textcap::speakable(dialog ? exe_ui::dialog_text() : popup ? popup.text() : std::string());
  }
  // What makes this a different message: its text (dots ignored, so a "Connecting..." ellipsis that animates does not
  // re-read it) and whether it has an edit box.
  static std::string content_key() {
    exe_ui::Popup popup;
    bool dialog = exe_ui::dialog_open();
    if (!dialog) popup = exe_ui::popup();
    std::string key;
    for (char c : message_text(dialog, popup)) if (c != '.') key.push_back(c);
    if (popup && popup.edit()) key += "|edit";
    return key;
  }
  static NodeVtablePtr answer_button(std::string_view label, bool yes) {
    auto v = std::make_shared<NodeVtable>();
    v->control_type = &kButtonType;
    std::string l(label);
    v->announcements = {NodeAnnouncement([l] { return l; }, false, announcement_kinds::kLabel)};
    v->on_activate = [yes] { exe_ui::answer_dialog(yes); };
    return v;
  }
  EditSession edit_;
  std::optional<std::string> shown_;   // the content last read for this box
  std::string pending_;                // a different content, waiting to settle
  int settle_ = 0;
  static constexpr int kSettleFrames = 4;
};

std::unique_ptr<Screen> make_message_box() { return std::make_unique<MessageBoxScreen>(); }
}  // namespace gd::screens
