#include "screens/message_box.h"
#include "core/graph_builder.h"
#include "core/message_builder.h"
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
  void on_push() override { edit_.stop(); }
  void on_unfocus() override { edit_.stop(); }
  void on_update() override { if (edit_.active()) { exe_ui::Popup p = exe_ui::popup(); edit_.tick(p ? p.edit() : WidgetA{}); } }
  std::vector<ScreenAction> actions() override {
    return {{std::string(action_ids::Back), [] {
      if (exe_ui::dialog_open()) { exe_ui::answer_dialog(false); return; }  // No, or just closes an Okay box
      exe_ui::Popup p = exe_ui::popup();
      std::vector<WidgetA> btns = p ? p.buttons() : std::vector<WidgetA>{};
      if (!btns.empty()) btns.back().activate();
    }}};
  }

  void build(GraphBuilder& b) override {
    std::string text;
    exe_ui::Popup popup;
    bool dialog = exe_ui::dialog_open();
    if (dialog) text = exe_ui::dialog_text();
    else { popup = exe_ui::popup(); if (popup) text = popup.text(); }
    text = textcap::speakable(text);
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
  static NodeVtablePtr answer_button(std::string_view label, bool yes) {
    auto v = std::make_shared<NodeVtable>();
    v->control_type = &kButtonType;
    std::string l(label);
    v->announcements = {NodeAnnouncement([l] { return l; }, false, announcement_kinds::kLabel)};
    v->on_activate = [yes] { exe_ui::answer_dialog(yes); };
    return v;
  }
  EditSession edit_;
};

std::unique_ptr<Screen> make_message_box() { return std::make_unique<MessageBoxScreen>(); }
}  // namespace gd::screens
