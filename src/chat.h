#pragma once
// Multiplayer chat (static RE 2026-10-05, docs/exe-ui-layout.md "Chat"). Every line other players send reaches the
// game's chat window through GameEngine::RemovePendingMessages, which the window calls once per frame (exe+0x1644cd)
// to take the engine's pending list. Hooking that export gives each line exactly once, in order: it is spoken
// (screen reader, never interrupting) and kept in a history the player reviews with Ctrl+Up / Ctrl+Down in the
// world and Up / Down while typing. Game-thread only except status().
#include <string>
namespace gd::chat {
bool install();
void remove();
// Review the history: dir < 0 older, > 0 newer. Speaks the line (or "no chat messages" / the edge word).
void review(int dir);
std::string status();                               // dev: /chat
bool dev_add(const std::string& name, const std::string& text, int type);   // dev: GameEngine::AddChatMessage, as an inbound line
}  // namespace gd::chat
