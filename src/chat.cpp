#include "chat.h"
#include <windows.h>
#include <atomic>
#include <format>
#include <mutex>
#include <string>
#include <vector>
#include "core/chat_log.h"
#include "core/message_builder.h"
#include "core/strings.h"
#include "gd_names.h"
#include "hooks.h"
#include "log.h"
#include "msvc_string.h"
#include "speech.h"
#include "textcap.h"

namespace gd::chat {
namespace {
using namespace gd::names;

std::vector<gd::hooks::Hook> g_hooks;
std::mutex g_mu;                 // the log is written on the game thread and read by /chat
core::ChatLog g_log;
std::atomic<uint64_t> g_lines{0}, g_calls{0};

// GAME::ChatMessage (GameEngine::AddChatMessage builds it at exe-side push_back Game.dll+0x2e7200, stride 0x50):
// +0x00 u16 sender name, +0x20 u16 text, +0x40 u8 type, +0x48 Entity* (the speech bubble's owner, may be null).
// Type (the window's colour switch at exe+0x16477e and the send paths at exe+0x162c90): 0 = everyone, 1 = the party
// channel, 2 = a whisper (the window keeps its sender as the /r target).
constexpr size_t kMsgStride = 0x50, kMsgName = 0x00, kMsgText = 0x20, kMsgType = 0x40;
struct Raw { char16_t name[64]; char16_t text[512]; unsigned char type; };

bool bad(const void* p, size_t n) { return IsBadReadPtr(p, n) != 0; }
bool read_u16(const void* mstr, char16_t* out, size_t cap) {
  __try {
    const MsvcStringW* s = (const MsvcStringW*)mstr;
    size_t n = s->size < cap - 1 ? s->size : cap - 1;
    const char16_t* d = s->data();
    if (n && bad(d, n * 2)) return false;
    memcpy(out, d, n * 2); out[n] = 0;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// The out vector after the game filled it: {begin, end, cap}. POD reads only (SEH frame).
int read_messages(const void* vec, Raw* out, int max) {
  int count = 0;
  __try {
    const char* const* v = (const char* const*)vec;
    const char* begin = v[0]; const char* end = v[1];
    if (!begin || end < begin) return 0;
    size_t n = (size_t)(end - begin) / kMsgStride;
    for (size_t i = 0; i < n && count < max; ++i) {
      const char* m = begin + i * kMsgStride;
      Raw& r = out[count];
      if (!read_u16(m + kMsgName, r.name, 64)) r.name[0] = 0;
      if (!read_u16(m + kMsgText, r.text, 512)) r.text[0] = 0;
      r.type = *(const unsigned char*)(m + kMsgType);
      ++count;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {}
  return count;
}

std::string_view kind_word(unsigned char type) {
  if (type == 1) return strings::kChatParty;
  if (type == 2) return strings::kChatWhisper;
  return {};
}

void on_line(const Raw& r) {
  std::string name = textcap::speakable(std::u16string_view(r.name));
  std::string text = textcap::speakable(std::u16string_view(r.text));
  if (text.empty()) return;
  core::MessageBuilder m;
  strings::push_chat_line(m, kind_word(r.type), name, text);
  std::string line = m.build();
  { std::lock_guard lk(g_mu); g_log.add(line); }
  ++g_lines;
  log::writef("chat: type {} '{}'", (int)r.type, line);
  speech::speak(line, false);   // the screen reader, never cutting what it is saying
}

// GameEngine::RemovePendingMessages(mem::vector<ChatMessage>& out): moves the engine's pending lines into `out`.
typedef void (*RemovePending_t)(void*, void*);
static RemovePending_t RemovePending_hook_orig;
static void RemovePending_hook(void* self, void* out) {
  RemovePending_hook_orig(self, out);   // the game's behaviour first, always
  ++g_calls;
  if (!out) return;
  static Raw raw[16];
  int n = read_messages(out, raw, 16);
  for (int i = 0; i < n; ++i) on_line(raw[i]);
}
}  // namespace

bool install() {
  g_hooks = {GD_HOOK(GameEngine_RemovePendingMessages, RemovePending_hook)};
  return gd::hooks::attach_hooks(g_hooks) == 0;
}
void remove() { gd::hooks::detach_hooks(g_hooks); }

void review(int dir) {
  std::string line;
  bool edge = false;
  { std::lock_guard lk(g_mu); line = g_log.step(dir, edge); }
  if (line.empty()) { speech::speak(strings::kNoChat, true); return; }
  if (edge) {
    core::MessageBuilder m; m.list_item().fragment(dir < 0 ? strings::kChatTop : strings::kChatBottom).list_item().fragment(line);
    speech::speak(m.build(), true);
    return;
  }
  speech::speak(line, true);
}

std::string status() {
  std::lock_guard lk(g_mu);
  std::string out = std::format("calls={} lines={} kept={} cursor={}\n", g_calls.load(), g_lines.load(), g_log.size(), g_log.cursor());
  for (const std::string& l : g_log.lines()) out += "  " + l + "\n";
  return out;
}

namespace {
typedef void (*AddChat_t)(void*, const MsvcStringW*, const MsvcStringW*, unsigned char, void*);
bool call_add(AddChat_t add, void* engine, const MsvcStringW* n, const MsvcStringW* t, unsigned char type) {
  __try { add(engine, n, t, type, nullptr); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::u16string to_u16(const std::string& s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::u16string out((size_t)(n > 0 ? n : 0), u'\0');
  if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), (wchar_t*)out.data(), n);
  return out;
}
}  // namespace
bool dev_add(const std::string& name, const std::string& text, int type) {
  HMODULE m = GetModuleHandleA(GameEngine_AddChatMessage_DLL);
  AddChat_t add = m ? (AddChat_t)GetProcAddress(m, GameEngine_AddChatMessage) : nullptr;
  void** ge = m ? (void**)GetProcAddress(m, gGameEngine) : nullptr;
  void* engine = ge ? *ge : nullptr;
  if (!add || !engine) return false;
  // Our own strings in the game's layout, heap form pointing at our buffers: the callee only copies them.
  std::u16string n16 = to_u16(name), t16 = to_u16(text);
  n16.reserve(16); t16.reserve(16);
  MsvcStringW ns{}, ts{};
  ns.u.ptr = n16.data(); ns.size = n16.size(); ns.capacity = n16.capacity();
  ts.u.ptr = t16.data(); ts.size = t16.size(); ts.capacity = t16.capacity();
  bool ok = call_add(add, engine, &ns, &ts, (unsigned char)type);
  log::writef("chat: dev add '{}' '{}' type {} ok={}", name, text, type, ok);
  return ok;
}
}  // namespace gd::chat
