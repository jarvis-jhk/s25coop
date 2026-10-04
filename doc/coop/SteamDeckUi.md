# Steam Deck first-run display profile

A fresh profile on a detected Steam Deck uses automatic GUI scaling against a
640-unit logical height. At the native 1280×800 handheld resolution this is 125%,
with a 1024×640 GUI surface. The existing driver caps automatic scaling against
both axes so that at least 800×600 GUI units remain after a resize, including a
small or portrait display. Borderless desktop-resolution startup stays in place.

Detection accepts Steam's exact `SteamDeck=1` or `SteamDeck=0` launch environment
values. They override hardware detection. With neither valid override, the Linux
DMI files `/sys/class/dmi/id/sys_vendor` and `product_name` must identify `Valve`
and exactly `Jupiter` (LCD) or `Galileo` (OLED). Missing or unreadable files are an
ordinary non-match; resolution, operating-system branding, a connected controller
or a partial product name alone never identifies a Deck. The environment flag can
also deliberately opt another platform into the first-run profile.

## Settings precedence

The selected profile is saved as `steam_deck_ui` in the existing `[video]` section.
It is a first-run default, rather than a hardware rule applied on every launch.

- Existing valid configurations without this key retain the old automatic GUI
  scale, including when opened on a Deck. They load `steam_deck_ui=false`.
- A stored true or false is preserved even when the device or launch flag changes.
- An explicitly chosen GUI percentage wins over the automatic recommendation.
- TV mode uses its existing 1080-unit reference height. Turning it off restores
  the stored Deck profile, or ordinary desktop automatic scaling when disabled.
  Enabling TV mode still selects Auto; disabling it retains a chosen fixed scale.

Players can use the existing Options → Graphics → GUI Scale selection to change
or override the recommendation. Advanced configuration may set `steam_deck_ui`
explicitly; this slice does not add another options control.

## Acceptance boundary

Tests cover synthetic LCD/OLED hardware identities, explicit environment flags,
missing IDs and false positives; actual singleton settings load/save and old-key
migration; real mock-driver resize/scale conversion; and mouse clicks on the
actual Options TV controls. Test state and environment are restored explicitly
on normal and deliberately exercised exceptional exits.

This is detection and display scaling only. It does not implement MenuRedesign
player cards, a controller panel, physical Deck device probing, controller
remapping, or packaged hardware acceptance. The existing game/CI fault-report
path remains responsible for real faults; absent DMI files are not faults.
