// Other players in a multiplayer game, through Game.dll / Engine.dll exports only (static RE 2026-10-05).
// The actions are the party portrait's right-click menu (its handler exe+0x252600, built at exe+0x254120) and the
// group window's invite / leave (exe+0x203748, exe+0x203830), each behind the same gate the exe applies:
//   Remove  (tagPortraitRemove): not auto-party and you lead your party (Player::GetPartyId == your player id)
//                                -> PartyManager::AttemptToKickFromLocalParty(id)
//   Inspect (tagInspectOption):  within 14 units (exe+0x31eab0) -> GameEngine::InspectPlayer(id), else the game's
//                                tagInspectWarning box
//   Trade   (tagPortraitTrade):  TradeManager::AbleToTrade, within 14 units -> HandleInitiateTradeOutbound(id, true),
//                                else the game's tagTradeDistance box
//   Ban     (tagMPBan):          the host only (GameEngine::IsServerOrSingle) -> GameEngine::BanPlayer(id, true)
//   Invite  (group window):      not you, not already in your party -> GameEngine::InvitePlayerToParty(id, you)
//   Leave   (group window):      GameEngine::AddPlayerToParty(you, 0)
// The distance refusals here say "too far away" instead of raising the game's dialog box.
#include "gameapi.h"
#include "gameapi_internal.h"
#include <algorithm>
#include <cmath>
#include <format>
#include "world.h"

namespace gd::gameapi {
using namespace gd::names;
using namespace gd::gameapi::detail;
namespace {
struct Api {
  void* (*GetPlayerManagerClient)(const void*) = nullptr;
  const MemVec* (*GetAllPlayersInGame)(const void*) = nullptr;
  MsvcStringW* (*GetPlayerName)(const void*, MsvcStringW*, unsigned) = nullptr;     // hidden return
  MsvcStringA* (*GetPlayerClass)(const void*, MsvcStringA*, unsigned) = nullptr;    // a class tag; hidden return
  MsvcStringW* (*GetPlayerLevel)(const void*, MsvcStringW*, unsigned) = nullptr;    // the level as text; hidden return
  void* (*GetPlayerLocation)(const void*, void*, unsigned) = nullptr;               // WorldVec3 by hidden return
  unsigned (*GetPlayerId)(const void*) = nullptr;
  bool (*IsServerOrSingle)(const void*) = nullptr;
  void* (*GetPartyManager)(void*) = nullptr;
  void (*GetPartyMembers)(const void*, unsigned, MemVec*) = nullptr;
  void (*AttemptToKick)(void*, unsigned) = nullptr;
  int (*GetPartyId)(const void*) = nullptr;
  void** gEngine = nullptr;
  void* (*GetGameInfo)(void*) = nullptr;
  bool (*GetAutoParty)(const void*) = nullptr;
  void* (*GetTradeManager)(void*) = nullptr;
  bool (*AbleToTrade)(const void*) = nullptr;
  void (*InitiateTrade)(void*, unsigned, bool) = nullptr;
  void (*InspectPlayer)(void*, unsigned) = nullptr;
  void (*InvitePlayerToParty)(void*, unsigned, int) = nullptr;
  void (*AddPlayerToParty)(void*, unsigned, int) = nullptr;
  void (*BanPlayer)(void*, unsigned, bool) = nullptr;
  void* (*EventManager_Get)() = nullptr;
  void (*EventManager_Send)(void*, const void*, unsigned) = nullptr;
  bool loaded = false;
} g;
void load_players() {
  if (g.loaded) return;
  g.loaded = true;
  GAPI_LOAD(g, GetPlayerManagerClient, GameEngine_GetPlayerManagerClient);
  GAPI_LOAD(g, GetAllPlayersInGame, PlayerManagerClient_GetAllPlayersInGame);
  GAPI_LOAD(g, GetPlayerName, PlayerManagerClient_GetPlayerName);
  GAPI_LOAD(g, GetPlayerClass, PlayerManagerClient_GetPlayerClass);
  GAPI_LOAD(g, GetPlayerLevel, PlayerManagerClient_GetPlayerLevel);
  GAPI_LOAD(g, GetPlayerLocation, PlayerManagerClient_GetPlayerLocation);
  GAPI_LOAD(g, GetPlayerId, GameEngine_GetPlayerId);
  GAPI_LOAD(g, IsServerOrSingle, GameEngine_IsServerOrSingle);
  GAPI_LOAD(g, GetPartyManager, GameEngine_GetPartyManager);
  GAPI_LOAD(g, GetPartyMembers, PartyManager_GetPartyMembers);
  GAPI_LOAD(g, AttemptToKick, PartyManager_AttemptToKickFromLocalParty);
  GAPI_LOAD(g, GetPartyId, Player_GetPartyId);
  GAPI_LOAD(g, gEngine, gEngine);
  GAPI_LOAD(g, GetGameInfo, Engine_GetGameInfo);
  GAPI_LOAD(g, GetAutoParty, GameInfo_GetAutoParty);
  GAPI_LOAD(g, GetTradeManager, GameEngine_GetTradeManager);
  GAPI_LOAD(g, AbleToTrade, TradeManager_AbleToTrade);
  GAPI_LOAD(g, InitiateTrade, TradeManager_HandleInitiateTradeOutbound);
  GAPI_LOAD(g, InspectPlayer, GameEngine_InspectPlayer);
  GAPI_LOAD(g, InvitePlayerToParty, GameEngine_InvitePlayerToParty);
  GAPI_LOAD(g, AddPlayerToParty, GameEngine_AddPlayerToParty);
  GAPI_LOAD(g, BanPlayer, GameEngine_BanPlayer);
  GAPI_LOAD(g, EventManager_Get, EventManager_Get);
  GAPI_LOAD(g, EventManager_Send, EventManager_Send);
}
constexpr float kReach = 14.0f;   // trade / inspect distance (exe+0x31eab0, compared in both branches of exe+0x252600)

void* pmc() {
  load_players();
  void* e = engine();
  void* m = nullptr;
  if (e && g.GetPlayerManagerClient) guarded("GetPlayerManagerClient", [&] { m = g.GetPlayerManagerClient(e); });
  return m;
}
unsigned my_id() {
  void* e = engine();
  unsigned id = 0;
  if (e && g.GetPlayerId) guarded("GetPlayerId", [&] { id = g.GetPlayerId(e); });
  return id;
}
std::vector<unsigned> party_of(unsigned id) {
  void* e = engine();
  std::vector<unsigned> out;
  if (!e || !g.GetPartyManager || !g.GetPartyMembers) return out;
  VecBuffer<unsigned> buf(16);
  bool ok = guarded("GetPartyMembers", [&] { void* pm = g.GetPartyManager(e); if (pm) g.GetPartyMembers(pm, id, buf.vec()); });
  if (ok) out = buf.take("GetPartyMembers");
  return out;
}
bool location_of(void* m, unsigned id, world::Vec3& out) {
  alignas(8) unsigned char wv[0x20] = {};
  bool ok = g.GetPlayerLocation && guarded("GetPlayerLocation", [&] { g.GetPlayerLocation(m, wv, id); });
  return ok && world::world_point(wv, out);
}
bool distance_to(void* m, unsigned id, float& out) {
  world::Vec3 them, me;
  if (!location_of(m, id, them) || !world::player_position(me)) return false;
  out = std::sqrt((them.x - me.x) * (them.x - me.x) + (them.y - me.y) * (them.y - me.y) + (them.z - me.z) * (them.z - me.z));
  return true;
}
}  // namespace

bool is_multiplayer() {
  load_players();
  void* e = engine();
  bool server_or_single = true;
  if (e && g.IsServerOrSingle) guarded("IsServerOrSingle", [&] { server_or_single = g.IsServerOrSingle(e); });
  return !server_or_single || !other_players().empty();
}

std::vector<OtherPlayer> other_players() {
  std::vector<OtherPlayer> out;
  void* m = pmc();
  if (!m || !g.GetAllPlayersInGame) return out;
  std::vector<unsigned> ids;
  guarded("GetAllPlayersInGame", [&] { ids = vec_items<unsigned>(g.GetAllPlayersInGame(m), 16); });
  unsigned me = my_id();
  std::vector<unsigned> mine = party_of(me);
  void* p = player();
  int my_party = 0;
  if (p && g.GetPartyId) guarded("GetPartyId", [&] { my_party = g.GetPartyId(p); });
  for (unsigned id : ids) {
    if (!id || id == me) continue;
    OtherPlayer o;
    o.id = id;
    if (g.GetPlayerName) guarded("GetPlayerName", [&] { MsvcStringW s; init_u16(s); g.GetPlayerName(m, &s, id); o.name = take_u16(s); });
    if (g.GetPlayerClass) guarded("GetPlayerClass", [&] { MsvcStringA s; init_a(s); g.GetPlayerClass(m, &s, id); o.class_tag = take_a(s); });
    if (g.GetPlayerLevel) guarded("GetPlayerLevel", [&] { MsvcStringW s; init_u16(s); g.GetPlayerLevel(m, &s, id); o.level = (unsigned)std::atoi(take_u16(s).c_str()); });
    if (!o.class_tag.empty()) o.class_name = localize(o.class_tag);
    o.in_party = std::find(mine.begin(), mine.end(), id) != mine.end();
    o.leader = o.in_party && my_party != 0 && (unsigned)my_party == id;
    world::Vec3 at;
    o.has_pos = location_of(m, id, o.pos);
    if (o.has_pos && world::player_position(at))
      o.distance = (int)std::lround(std::sqrt((o.pos.x - at.x) * (o.pos.x - at.x) + (o.pos.y - at.y) * (o.pos.y - at.y) + (o.pos.z - at.z) * (o.pos.z - at.z)));
    out.push_back(std::move(o));
  }
  return out;
}

std::string player_name(unsigned id) {
  void* m = pmc();
  std::string name;
  if (m && id && g.GetPlayerName) guarded("GetPlayerName", [&] { MsvcStringW s; init_u16(s); g.GetPlayerName(m, &s, id); name = take_u16(s); });
  return name;
}

PlayerActions player_actions(unsigned id) {
  load_players();
  PlayerActions a;
  void* e = engine(); void* p = player();
  unsigned me = my_id();
  if (!e || !p || !id || id == me) return a;
  bool auto_party = true;
  void* eng = g.gEngine ? rdp(g.gEngine, 0) : nullptr;
  if (eng && g.GetGameInfo && g.GetAutoParty) guarded("GetAutoParty", [&] { void* gi = g.GetGameInfo(eng); if (gi) auto_party = g.GetAutoParty(gi); });
  int my_party = 0;
  if (g.GetPartyId) guarded("GetPartyId", [&] { my_party = g.GetPartyId(p); });
  std::vector<unsigned> mine = party_of(me);
  bool together = std::find(mine.begin(), mine.end(), id) != mine.end();
  a.inspect = g.InspectPlayer != nullptr;
  a.trade = g.InitiateTrade != nullptr;
  a.whisper = true;
  a.remove = together && !auto_party && (unsigned)my_party == me && g.AttemptToKick;
  if (g.IsServerOrSingle) guarded("IsServerOrSingle", [&] { a.ban = g.IsServerOrSingle(e) && g.BanPlayer; });
  a.invite = !together && g.InvitePlayerToParty;
  a.leave = mine.size() > 1 && g.AddPlayerToParty;
  return a;
}

PlayerResult inspect_player(unsigned id) {
  void* m = pmc(); void* e = engine();
  if (!m || !e || !g.InspectPlayer) return PlayerResult::Failed;
  float d = 0;
  if (!distance_to(m, id, d)) return PlayerResult::Failed;
  if (d > kReach) return PlayerResult::TooFar;
  bool ok = guarded("InspectPlayer", [&] { g.InspectPlayer(e, id); });
  log::writef("gameapi: inspect player {} at {:.1f} ok={}", id, d, ok);
  return ok ? PlayerResult::Done : PlayerResult::Failed;
}
PlayerResult trade_with(unsigned id) {
  void* m = pmc(); void* e = engine();
  if (!m || !e || !g.GetTradeManager || !g.AbleToTrade || !g.InitiateTrade) return PlayerResult::Failed;
  void* tm = nullptr;
  bool able = false;
  guarded("AbleToTrade", [&] { tm = g.GetTradeManager(e); if (tm) able = g.AbleToTrade(tm); });
  if (!tm || !able) return PlayerResult::Refused;
  float d = 0;
  if (!distance_to(m, id, d)) return PlayerResult::Failed;
  if (d > kReach) return PlayerResult::TooFar;
  bool ok = guarded("HandleInitiateTradeOutbound", [&] { g.InitiateTrade(tm, id, true); });
  log::writef("gameapi: trade request to player {} at {:.1f} ok={}", id, d, ok);
  return ok ? PlayerResult::Done : PlayerResult::Failed;
}
bool invite_player(unsigned id) {
  void* e = engine();
  unsigned me = my_id();
  if (!e || !g.InvitePlayerToParty || !id || id == me) return false;
  bool ok = guarded("InvitePlayerToParty", [&] { g.InvitePlayerToParty(e, id, (int)me); });
  log::writef("gameapi: invite player {} ok={}", id, ok);
  return ok;
}
bool kick_player(unsigned id) {
  void* e = engine();
  if (!e || !g.GetPartyManager || !g.AttemptToKick) return false;
  bool ok = guarded("AttemptToKickFromLocalParty", [&] { void* pm = g.GetPartyManager(e); if (pm) g.AttemptToKick(pm, id); });
  log::writef("gameapi: kick player {} ok={}", id, ok);
  return ok;
}
bool ban_player(unsigned id) {
  void* e = engine();
  if (!e || !g.BanPlayer) return false;
  bool ok = guarded("BanPlayer", [&] { g.BanPlayer(e, id, true); });
  log::writef("gameapi: ban player {} ok={}", id, ok);
  return ok;
}
// The portrait menu's Whisper (tagPortraitChat, exe+0x2528fa): the player's name, quoted when it holds a space, in
// GameEvent 8 {int type = 8; u16string name}; the chat window's listener opens its box with "/w <name> " typed.
namespace {
struct WhisperEvent { int type; int pad; MsvcStringW name; };
bool send_event(void (*send)(void*, const void*, unsigned), void* em, const WhisperEvent* ev) {
  __try { send(em, ev, 8); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}  // namespace
bool whisper_player(unsigned id) {
  void* m = pmc();
  if (!m || !g.GetPlayerName || !g.EventManager_Get || !g.EventManager_Send) return false;
  std::u16string name;
  guarded("GetPlayerName", [&] {
    MsvcStringW s; init_u16(s); g.GetPlayerName(m, &s, id);
    name.assign(s.data(), s.size);
    if (s.capacity >= 8 && s.u.ptr) free(s.u.ptr);
  });
  if (name.empty()) return false;
  if (name.find(u' ') != std::u16string::npos) name = u"\"" + name + u"\"";
  name.reserve(16);   // heap form: the listener only copies the string out of the event
  WhisperEvent ev{8, 0, {}};
  ev.name.u.ptr = name.data(); ev.name.size = name.size(); ev.name.capacity = name.capacity();
  void* em = nullptr;
  guarded("EventManager::Get", [&] { em = g.EventManager_Get(); });
  bool ok = em && send_event(g.EventManager_Send, em, &ev);
  log::writef("gameapi: whisper player {} ok={}", id, ok);
  return ok;
}
bool leave_party() {
  void* e = engine();
  unsigned me = my_id();
  if (!e || !g.AddPlayerToParty || !me) return false;
  bool ok = guarded("AddPlayerToParty(leave)", [&] { g.AddPlayerToParty(e, me, 0); });
  log::writef("gameapi: leave party ok={}", ok);
  return ok;
}
std::string dump_players() {
  std::string out = std::format("me={} multiplayer={}\n", my_id(), is_multiplayer());
  for (const OtherPlayer& o : other_players()) {
    PlayerActions a = player_actions(o.id);
    out += std::format("  {} '{}' level {} class '{}' ({}) party={} leader={} dist={} | inspect={} trade={} invite={} remove={} ban={} leave={}\n",
                       o.id, o.name, o.level, o.class_name, o.class_tag, o.in_party, o.leader, o.distance, a.inspect, a.trade, a.invite, a.remove, a.ban, a.leave);
  }
  return out;
}
}  // namespace gd::gameapi
