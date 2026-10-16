#include "notify.h"
#include <windows.h>
#include <atomic>
#include <deque>
#include <format>
#include <mutex>
#include <string>
#include <vector>
#include "app.h"
#include "gd_names.h"
#include "hooks.h"
#include "log.h"
#include "msvc_string.h"
#include "speech.h"
#include "textcap.h"

namespace gd::notify {
namespace {
using namespace gd::names;

std::vector<gd::hooks::Hook> g_hooks;
std::atomic<uint64_t> g_banners{0}, g_popups{0}, g_deduped{0};
std::mutex g_recent_mu;
std::deque<std::string> g_recent;      // last spoken lines, for /notify
std::string g_last_text;               // dedupe: the immediately-preceding line
double g_last_t = 0;
constexpr double kDedupeWindow = 1.5;  // seconds; the game re-sets the popup while it is showing

// Speak a banner/popup line once (game thread): non-interrupting, deduped against the last identical line.
void announce(std::string text) {
  if (text.empty()) return;
  double now = app::now();
  if (text == g_last_text && now - g_last_t < kDedupeWindow) { ++g_deduped; g_last_t = now; return; }
  g_last_text = text; g_last_t = now;
  { std::lock_guard lk(g_recent_mu); g_recent.push_back(text); while (g_recent.size() > 20) g_recent.pop_front(); }
  speech::speak(text, false);   // screen reader, do not cut what it is already saying
}

bool bad(const void* p, size_t n) { return IsBadReadPtr(p, n) != 0; }
// POD reads (SEH): copy a MsvcStringW's UTF-16 into a fixed buffer (no C++ objects in the __try body).
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
bool read_charstr(const void* mstr, char* out, size_t cap) {
  __try {
    const MsvcStringA* s = (const MsvcStringA*)mstr;
    size_t n = s->size < cap - 1 ? s->size : cap - 1;
    const char* d = s->data();
    if (n && bad(d, n)) return false;
    memcpy(out, d, n); out[n] = 0;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Read up to `maxlines` u16 strings out of a mem::vector<basic_string<u16>> (stride 0x20). Returns the count.
int read_u16_vec(const void* vec, char16_t lines[][256], int maxlines) {
  int count = 0;
  __try {
    const char* const* v = (const char* const*)vec;   // {begin, end, cap}
    const char* begin = v[0]; const char* end = v[1];
    if (!begin || end < begin) return 0;
    size_t n = (size_t)(end - begin) / 0x20;
    for (size_t i = 0; i < n && count < maxlines; ++i) {
      const MsvcStringW* s = (const MsvcStringW*)(begin + i * 0x20);
      size_t m = s->size < 255 ? s->size : 255;
      const char16_t* d = s->data();
      if (m && bad(d, m * 2)) { lines[count][0] = 0; }
      else { memcpy(lines[count], d, m * 2); lines[count][m] = 0; }
      ++count;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {}
  return count;
}

void check_queue_layout(const char16_t* pushed);   // below: the inline-banner reader, checked against a real push

// GameEngine::AddUINotification(Type, u16string const&, bool) -- one localized banner line.
typedef void (*AddUINotifStr_t)(void*, int, const void*, bool);
static AddUINotifStr_t AddUINotifStr_hook_orig;
static void AddUINotifStr_hook(void* self, int type, const void* str, bool b) {
  AddUINotifStr_hook_orig(self, type, str, b);
  ++g_banners;
  char16_t buf[256];
  if (str && read_u16(str, buf, 256) && buf[0]) { check_queue_layout(buf); announce(log::utf8(buf)); }
}
// GameEngine::AddUINotification(Type, mem::vector<u16string> const&, bool) -- a multi-line banner.
typedef void (*AddUINotifVec_t)(void*, int, const void*, bool);
static AddUINotifVec_t AddUINotifVec_hook_orig;
static void AddUINotifVec_hook(void* self, int type, const void* vec, bool b) {
  AddUINotifVec_hook_orig(self, type, vec, b);
  ++g_banners;
  if (!vec) return;
  char16_t lines[8][256];
  int n = read_u16_vec(vec, lines, 8);
  for (int i = 0; i < n; ++i) if (lines[i][0]) announce(log::utf8(lines[i]));
}
// ControllerPlayer::SetUserText(std::string const& tag, int ms) -- the red action-failed popup; localize the tag.
typedef void (*SetUserText_t)(void*, const void*, int);
static SetUserText_t SetUserText_hook_orig;
static void SetUserText_hook(void* self, const void* tag, int ms) {
  SetUserText_hook_orig(self, tag, ms);
  ++g_popups;
  char t[128];
  if (tag && read_charstr(tag, t, 128) && t[0]) announce(gd::hooks::localize(t));
}

// ---- banners queued inline ----
// Several Game.dll functions push a UINotification straight into GameEngine's list instead of calling
// AddUINotification (the compiler inlined it): the multiplayer "busy" answers (tagShrineBusy "That shrine is being
// used by another player.", tagNpcConversationBusy, tagTroveBusy, tagAltarBusy, tagShrineHostActivated,
// tagShrineLocked), the gravestone's "Experience Reclaimed", remote server messages and the party kick. Static RE
// 2026-10-16: every one ends in the same vector push (Game.dll+0x2fbd20 on gGameEngine+0x68), the exported
// AddUINotification does the same, and GameEngine::TakeTopNotification reads it back: mem::vector of 0x20-byte
// entries {int type, mem::vector<u16string> lines}. The exe never calls TakeTopNotification, so the consumer is no hook
// point; each producer is hooked instead and what it left at the back of the list is spoken.
constexpr size_t kGE_Notifications = 0x68, kNotif_Size = 0x20, kNotif_Lines = 0x08;
void* game_engine() {
  static void** ge = [] {
    HMODULE m = GetModuleHandleA(gGameEngine_DLL);
    return m ? (void**)GetProcAddress(m, gGameEngine) : nullptr;
  }();
  return ge ? *ge : nullptr;
}
// The back of the list before a producer runs, to tell its push from what was already queued.
struct QueueMark { size_t count = 0; const char* back_entry = nullptr; const void* back_lines = nullptr; char16_t back[256] = {}; };
bool read_mark(QueueMark& m) {
  __try {
    char* ge = (char*)game_engine();
    if (!ge || bad(ge + kGE_Notifications, 0x18)) return false;
    char* const* v = (char* const*)(ge + kGE_Notifications);   // {begin, end, cap}
    char* begin = v[0]; char* end = v[1];
    if (!begin || end <= begin) { m.count = 0; return true; }
    m.count = (size_t)(end - begin) / kNotif_Size;
    char* back = end - kNotif_Size;
    if (bad(back, kNotif_Size)) return false;
    m.back_entry = back;
    char* const* lines = (char* const*)(back + kNotif_Lines);
    m.back_lines = lines[0];
    if (lines[0] && lines[1] > lines[0]) read_u16(lines[0], m.back, 256);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
QueueMark mark_queue() { QueueMark m; read_mark(m); return m; }
void announce_pushed(const QueueMark& before) {
  QueueMark after;
  if (!read_mark(after) || after.count == 0) return;
  bool same = after.count == before.count && after.back_lines == before.back_lines &&
              std::u16string_view(after.back) == std::u16string_view(before.back);
  if (same) return;
  ++g_banners;
  char16_t lines[8][256];
  int n = read_u16_vec(after.back_entry + kNotif_Lines, lines, 8);
  for (int i = 0; i < n; ++i) if (lines[i][0]) announce(textcap::speakable(std::u16string_view(lines[i])));
}
// Once per load: the list's back after a real AddUINotification must hold the line just pushed (the layout above is
// static RE; a game patch that moved it would otherwise fail silently).
void check_queue_layout(const char16_t* pushed) {
  static bool done = false;
  if (done) return;
  done = true;
  QueueMark m;
  bool ok = read_mark(m) && m.count > 0 && std::u16string_view(m.back) == std::u16string_view(pushed);
  log::writef("notify: inline banner queue layout {} (count {})", ok ? "ok" : "MISMATCH", m.count);
}
// One producer: its exact signature (a passthrough must forward stack arguments too), the mark, the call, the read.
#define GD_QUEUE_HOOK(NAME, PARAMS, ARGS) \
  typedef void (*NAME##_t) PARAMS;        \
  static NAME##_t NAME##_hook_orig;       \
  static void NAME##_hook PARAMS {        \
    QueueMark m = mark_queue();           \
    NAME##_hook_orig ARGS;                \
    announce_pushed(m);                   \
  }
GD_QUEUE_HOOK(EngageNpc, (void* s, unsigned a, unsigned b, bool c, bool d), (s, a, b, c, d))
GD_QUEUE_HOOK(EngageShrine, (void* s, unsigned a, unsigned b, unsigned c), (s, a, b, c))
GD_QUEUE_HOOK(EngageContainer, (void* s, unsigned a, unsigned b), (s, a, b))
GD_QUEUE_HOOK(EngageAltar, (void* s, unsigned a, unsigned b), (s, a, b))
GD_QUEUE_HOOK(EngageEndlessShrine, (void* s, unsigned a, unsigned b, unsigned c), (s, a, b, c))
GD_QUEUE_HOOK(XpRedemption, (void* s, unsigned a, bool b), (s, a, b))
GD_QUEUE_HOOK(MessageRemote, (void* s, unsigned a, unsigned b, bool c, const void* d), (s, a, b, c, d))
GD_QUEUE_HOOK(WMessageRemote, (void* s, unsigned a, bool b, const void* c), (s, a, b, c))
GD_QUEUE_HOOK(KickFromParty, (void* s, unsigned a), (s, a))
#undef GD_QUEUE_HOOK
}  // namespace

bool install() {
  g_hooks = {GD_HOOK(GameEngine_AddUINotification_Str, AddUINotifStr_hook),
             GD_HOOK(GameEngine_AddUINotification_Vec, AddUINotifVec_hook),
             GD_HOOK(ControllerPlayer_SetUserText, SetUserText_hook),
             GD_HOOK(GameEngine_EngageNpcResponse, EngageNpc_hook),
             GD_HOOK(GameEngine_EngageShrineResponse, EngageShrine_hook),
             GD_HOOK(GameEngine_EngageContainerResponse, EngageContainer_hook),
             GD_HOOK(GameEngine_EngageAltarResponse, EngageAltar_hook),
             GD_HOOK(GameEngine_EngageEndlessShrineResponse, EngageEndlessShrine_hook),
             GD_HOOK(GameEngine_PlayerExperienceRedemption, XpRedemption_hook),
             GD_HOOK(GameEngineInbound_DisplayMessageRemote, MessageRemote_hook),
             GD_HOOK(GameEngineInbound_DisplayWMessageRemote, WMessageRemote_hook),
             GD_HOOK(PartyManager_AttemptToKickFromLocalParty, KickFromParty_hook)};
  return gd::hooks::attach_hooks(g_hooks) == 0;
}
void remove() { gd::hooks::detach_hooks(g_hooks); }

std::string status() {
  std::string out = std::format("banners={} popups={} deduped={}\n", g_banners.load(), g_popups.load(), g_deduped.load());
  std::lock_guard lk(g_recent_mu);
  for (const std::string& l : g_recent) out += "  " + l + "\n";
  return out;
}
}  // namespace gd::notify
