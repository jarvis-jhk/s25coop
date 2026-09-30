# Campaign controller return paths

The campaign chooser uses the same Back action for controller B and the Back button.
A local campaign returns to Singleplayer; a Direct-IP, LAN or lobby campaign returns
to the Create game map chooser with its original server configuration.

The chapter and campaign victory screens accept controller input. Continue is the
initial focus and A activates it; B and Start also return to the main menu through
`ShowMenu()`. Picking up a controller and moving the stick or D-pad leave the victory
screen visible. Mouse clicks and keyboard input retain their existing return path.
Completion indicators are cleared when the victory screen opens, as before; recorded
campaign progress is retained.

## Regression coverage

`MenuPadCampaignTests` drives events through `MockupVideoDriver` and
`WindowManager::Draw`, without directly invoking desktop handlers or focus setters.
The network regression enters the campaign chooser through its real Campaign
together button and checks Direct-IP, LAN and lobby contexts. Existing tests cover
the local chooser and mission return path. Victory coverage checks A, B and Start
for both kinds of completion, acquisition/navigation without dismissal, consumed
input events, preserved progress and the mouse/keyboard return paths.

The UI fixture creates its own two-mission campaign from repository test data;
original Settlers II assets and a physical controller are not required.

## Companion session status

Branch: `sol/campaign-controller-navigation`. This is an M3 controller-navigation
slice. It does not complete the wider main-menu/all-dialogs roadmap item. Primary
integration and player changelog remain with Opus. `NOTES.md` and `ROADMAP.md` were initially claimed by `opus/per-player-start-wares`.
After that task completed, their scopes were acquired and the corresponding session
notes/status added on this branch. The wider M3 controller-navigation item remains open.

Validation on the companion checkout: the old implementation fails the network Back
and controller-victory regressions, while the keyboard/mouse countercheck passes.
With the correction, all seven `MenuPadCampaignTests` cases pass. The target was
built with GCC 12, warnings as errors and at most two jobs. Clang-format 10 and
whitespace checks pass. A read-only gpt-6.1-sol review found no actionable issues in
the supplied diff (sandbox source reads were unavailable).
The full `Test_splitscreen` ctest passed in 55.16 seconds.
