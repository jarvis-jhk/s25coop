# Changelog

What changed for players, newest first. Each release's notes on GitHub are its section here,
and the game will show the new sections after an update.

## Unreleased

- Options on the start screen now open full-screen pages for Display / Steam Deck, Sound and
  music, Controls and Language, usable with controller, mouse or keyboard. The old settings
  stay under "Advanced settings".
- "Load game" on the start screen opens a full-screen list of your saves, newest first, with
  map, date, play time and players. "Continue" starts your newest save directly.
- Map selection no longer loses a whole map folder because of one unreadable file next to it.
- From Return to the Roots: new game options for how each kind of mine uses up its deposits, coins
  training only one soldier at a time, and how far stranded soldiers look for a way home. Hunters
  now search for animals in a circle around their hut.
- Fixed a crash when closing the game on systems that provide SDL2 through sdl2-compat.

## 0.1.13

- Controller players who joined on the title screen now get their local lobby seats without
  pressing A again. Campaigns and loaded saves share the host's tribe; maps use separate open
  tribes. When there are too few open seats, choose "Play one tribe together" to seat everyone.

- Addon settings have clearer categories, a “Changed only” filter and hints for options that
  make resource gathering or travel easier or harder. Use LB/RB to switch categories;
  developer options stay hidden until you choose to show them.
- Applying or saving addon settings from another controller keeps your accepted choices and
  leaves an unfinished dropdown preview out of the saved settings.

## 0.1.12

- New title screen: every player presses A once on their controller to join (up to four), and
  the players who joined are shown at the top of every menu afterwards. Press A again (or click
  Start) to continue; B takes a player out again.
- New start screen: after the title the game opens a full-screen home page with big tiles for
  resuming your last game, campaigns, maps, loading a game, online play, options and more. It works
  with a controller (D-pad, A to open, B to come back) as well as with the mouse, uses the whole
  screen on a Steam Deck instead of stretching, and the help line at the bottom shows which buttons
  do what. Coming back from any menu puts you on the tile you opened. The old main menu is still
  there under "Classic menus", and its "New menus" button leads back.
- When several controller players share one tribe, an economy window (distribution, transport,
  tools, military, building order) that one of them has open now shows the changes another player
  makes, instead of the old values.

## 0.1.11

- The lobby has a "Player cards" button: it shows every player and co-player with colour, nation
  and team, four per page, and stays up to date while others join, leave or change settings.
- With several controller players sharing one tribe, each lobby card shows the cursor colour
  that player will have in the game.
- In the local lobby every controller player gets their own card: move up/down to pick colour,
  nation, team or seat mode and left/right to change it, without waiting for the host.
- On a Steam Deck, a fresh installation starts with menus and buttons sized for the handheld
  screen (125%). Existing settings and a GUI scale you picked yourself stay as they are.

## 0.1.10

- Controller hints name the right stick for moving the camera and LT/RT for zoom, including
  while browsing a window or Just watch. A zoom hint disappears when it cannot zoom any further.

- In-game controller hints show coloured A/B/X/Y button symbols and labelled buttons for
  the other controls. Each player's hints wrap to fit their view without losing instructions.

- Click the left stick (L3) to turn building spots on or off, including while a menu,
  window or road preview is open. From Just watch, the click returns to play and shows the spots.

- Loading a splitscreen game brings the controller players back: players who had their own tribe
  get it again, and in a "one tribe together" game everyone just presses A again in the lobby.
- The shared-tribe checkbox in the local lobby stays below the controller seats, clear of the game settings.
- Outline map keeps a consistent map and window size when zooming and reopening it.
- Steam, the application menu and the game window use "The Settlers II: Coop". Reinstalling
  repairs old Steam names automatically; starting from the desktop repairs them when Steam is closed.
- Post office keeps your selected letter when older messages disappear and refreshes category
  filters when a full inbox receives new mail, so viewing and deleting stay on the right letter.
- The merchandise statistics now show correct totals for the 4-hour and 16-hour ranges;
  they were too low before.
- Distribution of goods stays read-only during replays, without a false discard warning
  after controller or mouse input. Help remains available.
- Military settings in replays keep showing the recorded values after controller or mouse input,
  and close without a spurious warning about unsaved changes.
- Tool settings stay read-only during replays, with no false discard warning when closing.
  The tool-production Help button also remains available during replay playback.
- Statistics show a sensible empty scale when every player is hidden, instead of an enormous
  minimum value.
- Campaign previews show a clear fallback when artwork is unavailable; one missing or damaged
  image no longer hides the previews of other campaigns.
- Building sequence: Default now keeps all buildings enabled by wine, leather and charcoal addons.
  Replay sequence settings remain read-only, while building previews follow the selected row and recorded changes.
- Moving tracks up or down in the music player now updates playback when you close the window.
  Cancelling Add Track or Add Directory keeps playback as it was.
- Cancel the network Create Game form with controller B or Escape. The window's close button also
  works, and cancelling keeps you in the network menu.
- Keep scrolling text within bounds when its content or visible area changes, avoiding a possible crash.

## 0.1.9

- Choose the goods each player starts with: every row in the game lobby has a "Goods" button. "Default"
  uses the game's setting; the other choices are Minimal, Minimal+, Very low, Low, Normal and A lot.
  The host can set it for everybody, every player for themselves.
- New start: "Minimal" — 2 wood, 2 boards, 4 stones, one tool each for a metalworker, builder, woodcutter
  and carpenter, a little iron, ore and coal, 2 generals and a donkey, and not a single helper.
  "Minimal+" adds 8 boards, 6 stones, a shovel, 8 iron and 5 soldiers, for a player who needs a head start.
  Both can also be picked for all players with the game's starting goods setting.
- Controller: the campaign victory screen can now be left with A, B or Start, and B in the campaign list of
  a network game goes back to where you came from instead of the single-player menu. B on the map selection, in the options and in the single- and multiplayer menus goes back too (options are
  saved as with the Back button), and A, B or Start skips the intro video.

## 0.1.8

- Play one tribe together on one screen: in the lobby, tick "Play one tribe together", and every
  player who presses A on their controller gets their own view of the SAME tribe. Campaign
  missions always work this way.
- The game is now fully in German when your system is set to German, including all co-op texts.

## 0.1.7

- Splitscreen: up to four players on one screen, each with their own gamepad and their own view.
  Plug in the controllers, start a single-player game, and in the lobby every player presses A
  to take a seat.
- The menus, the lobby and the campaign screens work with a gamepad; mouse and keyboard work as before.
- Build with a gamepad through a ring menu, with hints on screen that say what each button does.
- TV mode: a larger, readable interface for playing on a television.
- German texts for all of the above.

## 0.1.6

- Continue a campaign together: the host loads the saved game under "Create game" → "Load", and the co-players
  join with "Play the host's tribe together" again.
- A co-player who has to leave a game is told why (the host removed them, their player left, or their game got
  out of step with the others) instead of just "Lost connection".

## 0.1.5

- Play one tribe together: in a network game the host ticks "Allow co-players", and anyone in the
  lobby can pick a player and press Join to control that player's tribe together with them. The
  host sees who plays with whom and can send a co-player away.
- Join Game has a new box "Play the host's tribe together": tick it to join even a full game as
  the host's co-player.
- Campaigns together: under "Create game", pick "Campaign together..." and a mission; your friends
  join with "Play the host's tribe together" and you play the mission as one tribe.

## 0.1.4

- The installer finds Settlers II by itself: put the GOG installer
  (`setup_the_settlers_2_gold_….exe`) or a copy of the CD into your Downloads folder and it takes the
  game files from there. If nothing is found, it opens a page explaining what is needed and where to get it.
- The Intro button in the main menu works: it plays the original intro video, with sound.
  Any key or click skips it.
- The game starts filling the whole screen at your screen's own resolution.
- The Steam entry is called "The Settlers II: Coop" and shows the Settlers II box art in the
  library and in Game Mode.

## 0.1.3

- Campaigns remember your progress: finishing a mission unlocks the next one (in the Roman
  campaign the first two are open from the start, as in the original), finished missions and
  conquered continents are marked, and a victory screen greets you when you leave a won mission.
- World campaign: winning a continent opens the same neighbouring continents as in the original.

## 0.1.2

- After an update the game shows what is new, once, right in the main menu. The new "What's new"
  button in the main menu shows the whole list again at any time.

## 0.1.1

- When the installer, an update or the game fails on Linux, an anonymous fault report is sent so
  it can be fixed (details and how to switch it off: README, "Fault reports").

## 0.1.0

- First s25coop build: Return to the Roots as of September 2026, packaged for Linux
  (Steam Deck, Linux Mint) and Windows.
- Steam Deck / Linux: `Install-s25coop.desktop` installs the game, adds it to Steam (Game Mode)
  and to the menu, and every start updates to the newest release by itself.
- The original Settlers II Gold folders
  `DATA` and `GFX` go into `~/.local/share/s25coop/S2/`, so updates never touch them.
- Coop campaigns, controller support and the in-game changelog are coming next — see ROADMAP.md.
