# Unreleased

- The multiplayer features (the Multiplayer screen, chat, `ctrl k` players, trading) now work with Grim Dawn 1.3.1.1 (the October 8 hotfix), together with everything from release 0.5.4.
- The Multiplayer screen now works. Choose Join Multiplayer or Host Multiplayer (`ctrl tab` switches between them), browse the list of games with the arrow keys, hear each game's players, difficulty, expansion, level range, password, hardcore, PvP, loot and ping, and press `enter` on a game to join it. The filters, your Steam friends list, the host settings and the character list are all there too.
  - Joining and hosting have not been tried yet; browsing, the filters, the host settings and switching tabs have.
- The Multiplayer screen's tabs now work like the tabs in the game's windows: Join Multiplayer and Host Multiplayer sit on one row, and arrowing onto a tab opens it. Internet or LAN is now one choice called network, changed with `left` and `right`, at the top of each tab.
- After the server list refreshes, or after a message box on the Multiplayer screen closes, you are back on the control you were on instead of the first tab.
- Fix: the password box that appears when you join a password-protected game is now read. Press `down` from the message to reach the password, `enter` to start typing, `enter` to stop, then choose Ok. `escape` cancels.
  - Joining with the right password has not been tried yet.
- Fix: the message the game shows when it refuses to let you join a game (for example "This game is using an incompatible game mode") is now read, with its Ok button.
- Chat in multiplayer: chat messages from other players are read as they arrive, with "whisper" or "party" in front when they are not for everyone. `ctrl up` and `ctrl down` go back through the chat messages.
- New: `ctrl enter` opens the chat box. Type your message (each letter is read, `backspace` reads back what is left), `enter` sends it, `escape` closes the box. `up` and `down` read the chat messages while you type.
- New key `delete` in the inventory: drops the item you are on to the ground at your feet. `g` or `m` finds it again; an item your loot filter hides needs `o` first.
- New key `ctrl k` in multiplayer: the other players in the game, with their level, class and whether they are in your party. `space` says where they are, `enter` lists what you can do: invite to party, remove from party, inspect, trade, whisper and ban, whichever the game allows for that player. `ctrl k` used to open the game's own group window, which the mod did not read.
  - Only whisper has been tried, and only alone. Inviting, removing, inspecting, trading and banning need another player and have not been tried. The inspect window is not read yet.
- The trade window is now read. Tab moves between the other player's offer, your offer, your bags and the Accept and Cancel buttons. `enter` on an item in your bags offers it, `enter` on an item in your offer takes it back, and `enter` on your offer's first line lets you type how many iron bits to offer (`backspace` offers none). Everything the other player does is read as it happens: what they add or take away, their iron bits, and when they accept. `escape` cancels the trade.
  - Not tried in the game yet.
- Fix: leaving the game with Exit to Main Menu no longer says "this screen is not supported" on the way out. If leaving takes a while, you hear "leaving the game".
- Fix: joining a multiplayer game is no longer "this screen is not supported" while the game connects; the connecting message and a password prompt that comes after it are read.
  - Not tried in the game yet: joining needs someone else's game.

# 0.5.4 (2026-10-09)

- Works with Grim Dawn 1.3.1.1 (the October 8 hotfix). The previous game version is no longer supported.

- Fix: `l` walks to places that sit just off the walkable ground, such as a mine or cave entrance picked from the map. Before, the game refused the walk and nothing happened.
- Fix: skill tooltips at a spirit guide (reclaiming skill points) are the full ones again, including what the next level gives, followed by the reclaim cost.

# 0.5.3 (2026-10-05)

- Fix: equipping a two-handed weapon while holding a weapon and a shield no longer loses the two-handed weapon. Before, it disappeared, and the old weapon and shield showed up in your bag while still being listed in your hands. Now both go to your bag and the two-handed weapon goes in your hands, the same as in the game. This works both with `enter` in the bag and from the Right Hand slot.
- Fix: equipping a two-handed weapon from the Right Hand slot no longer leaves your shield in the other hand.
- If your bag can't fit what you take off, equipping now says "no room in your bags" and nothing moves. Before, the item that didn't fit could be lost.

# 0.5.2 (2026-10-02)

- Fix: the update announcement now waits until the main menu has been up for a few seconds, so the game's own startup speech no longer talks over it.

# 0.5.1 (2026-10-02)

- Nothing new for players: this release exists so the update check added in 0.5.0 has something newer to find.

# 0.5.0 (2026-10-02)

- Grimdark now checks for updates when it starts and tells you if a newer version is out, for example "Grimdark update available v0.5.0, you have v0.4.0, run the Grimdark installer to update". If you installed a CI build, it tells you about newer CI builds instead. It stays quiet if you are offline. Turn it off in `f1`, mod options, check for updates.
- At a spirit guide, the constellations tab has a new "clear all constellations" row that says what it costs. Clicking it asks yes or no, then gives back every devotion point at once for the same price as reclaiming them one by one. This gets you out of constellations that hold themselves up, which the game otherwise only lets you undo with a Tonic of Clarity. Celestial powers need binding again afterwards.
- New key `ctrl apostrophe`: follow whatever you are reviewing instead of a map marker. After that, `apostrophe` puts your review cursor back on it and pings it like `semicolon`, even if it moved or you have reviewed other things since, so you can keep targeting one enemy in a boss fight. It says "gone" once the thing no longer exists. Picking a map marker or pressing `p` replaces it.
- The stats tab now tells you how to spend attribute points. The attribute points line always says to click Physique, Cunning or Spirit, and while you have points left each of those says "click to assign an attribute point".
- Fix: Vire's Might, Nullification and the medal runes that charge, leap or teleport to a spot now show up in the hotbar manager. They fire at your target, or at the spot you have selected.
- Fix: attaching a component or augment only offers items in your bags or equipped, no longer items in your personal and shared stashes.
- Items now say "with component", "with augment" or "with component augment" in every list, not just the inventory: the vendor's buy and sell tabs, both stash lists, the equip and attach pickers, and the inventor.
- `ctrl p` pauses the game in single player. It always worked, but it wasn't written down anywhere. The README has a new Pausing section on what you can and can't do while paused (short version: no acting in the world and no menus or windows, but the mod's own keys work).
- `l` now walks to whatever you selected last: the thing you found with the finding keys, a map marker you picked or followed with `apostrophe`, or the unexplored area from `p`. Targets beyond about 200 units may not walk until you get closer.
- New key `p`: find the nearest unexplored area you can walk to, within about 200 units. `apostrophe` pings it and `l` walks there; it says "explored" once you've seen it. Meant for when you're stuck and don't know where to go.
- Fix: the hotbar manager's skill list now uses the game's own rules, so it no longer offers skills the game won't put on a bar, such as auto-toggled skills or skills from the weapon set you aren't holding.
- Fix: skill tooltips in the skills screen no longer claim you can add points when you can't. They now say why, just like the game: no points, mastery too low, skill maxed, needs another skill first, and so on. Tooltips in the hotbar and celestial power pickers no longer talk about skill points at all.

# 0.4.0 (2026-09-27)

- Wall tones are now smoother.  When hugging a wall the wall will seem flatter/straighter.
- A number of critical but rare incorrect wall tone cases have been fixed.
- With `alt dot` and the other enemy targeting keys, the mod now prefers to target the nearest enemy you can fire at when enemies are bunched together, rather than grabbing one behind a wall.  Turn this off in `t`'s new targeting section.
- We now explicitly support classic casting.  This mode, enabled in the game's options menu, walks you into range of enemies before using ranged abilities. Note that the options menu requires tabbing to Apply and pressing it; note also that this mode does not give you control over how you'll move.
  - Hold shift in classic casting if you want to avoid moving (vanilla behavior, not something we added).
- `l` is the "move to" key. Anything you can target with the review cursor that is reachable can be moved to.  This explicitly excludes map icons.  Notably it includes adjacent room entrances, in which case it'll move you to the approximate center of the target room.
  - IMPORTANT: sometimes, rarely, a room is divided by a wall and the path to it thus goes through other rooms, possibly lots of them. You'll walk it, but maybe into a crowd of enemies.
- `j`, `i`, and the other skill use/clicking keys no longer care about the game's HUD or camera.
- When enemies are in range of ranged abilities, their sonar ping goes up in pitch. When they are in melee range, it goes up in pitch again. Turn this off in `ctrl t` if you don't want it.
- If you have trouble telling if enemies and other objects are north or south of you, a new option in `ctrl t`, "north south echo on route pings", may help by changing the ping you hear when using `semicolon`, `apostrophe`, and the other targeting keys.
- Fix: automatic loot pickups are now announced through Zira.
- Untested fix: Unicode in the map list may now render properly, and Unicode when using type-ahead may now work.
- Fix: do not play mod-provided sounds for shrines that are not enabled on your difficulty.
- Fix/improvement: rework how the 3 pings you get when cycling through entities work out reachability, to better match both how you use them and what's really going on.
- Fix/improvement: the stats screen now shows armor breakdown
- Fix: `alt comma` works again, jumping to the nearest of the highest-rarity enemies like `alt` with the other review keys.
