<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Reusable controller keyboard

MenuRedesign4b supplies an opt-in menu modal for the new front-end's name, IP and chat fields.
Existing fields retain their original mouse/keyboard behavior and remain outside pad focus.
F7/F9 callers must explicitly call `ctrlEdit::SetControllerKeyboardEnabled()` on an intended field.
This slice provides and exercises the reusable entry API; it does not claim adoption by those pages,
in-game input routing, Steam Deck rendering or a packaged release.

## Entry and editing

A focuses an enabled field's keyboard. Traversing to the field does not set its keyboard-focus bit:
a mouse user can keep typing into a different field until the controller explicitly opens the modal.
The modal owns a separate draft with the original maximum length, edit type and password masking.
Pad and mouse keys enter through that draft's existing `ctrlEdit::Msg_KeyDown`, sharing its character
availability, numeric/filename filtering, cursor and Unicode behavior with the physical keyboard.

- D-pad/stick navigates the key grid; A types the selected character.
- B removes one Unicode character before the draft cursor; X inserts a space; Y toggles letter case.
- Menu/Start, Enter or the Confirm button accepts the draft.
- The Cancel button, Escape or window close discards the draft.
- Mouse users can select every key and action; a physical keyboard edits the same draft.

Confirm calls the original field's `SetText` once, after marking the modal closed. Its configured
change notification runs on confirmation only. No original `Msg_EditEnter` is synthesized: accepting
text must not silently send a chat message, connect to a host or start a game. A future caller owns
that separate action. A notification may replace its own parent; the modal never touches the target
or itself after notifying it.

## Ownership and lifetime

The modal's owner is a `PadDeviceId`, not a router slot. A changed slot therefore does not change
who may edit. Foreign controllers cannot navigate, type, shift, delete, confirm or close the modal.
The default `IngameWindow` input hooks defer to desktop policy; only the keyboard opts in to
explicit device ownership, which also survives a restrictive policy on a newly assigned slot. The menu router
reconciles device inventory before dispatch and gives the opted-in modal a chance to consume
Back/Start before generic focus or desktop fallback.

Disconnecting the owner cancels. The original edit owns a weakly observed lifetime token: deleting
or replacing the target closes the modal without dereferencing it or delivering a callback. Desktop
switches destroy pending windows and also invalidate their targets. Disabling or externally replacing
the source text cancels instead of overwriting newer input. Its edit revision also detects a change
back to the initial value. A closing ancestor cancels before destruction. Opening the modal
drops queued routes into the original screen so a trailing Menu press cannot start it behind the keyboard. State is transient; no keyboard/password
contents are persisted or reported. Existing runtime and failed-CI fault reporting remains in place;
this modal introduces no background service or new external endpoint.

## Validation gate

Physical regressions use `MenuPadFixture` and the real driver queue, WindowManager and menu router.
Legacy `EditIsNoPadFocusStop` and `EditIsNotCollectedByFocusPath` contracts remain unchanged.
Debug UI/splitscreen suites, executed omission controls and all17 checks plus both complete exact-head
workflows are required before handoff. Local tests use mock drivers; real hardware acceptance is separate.
