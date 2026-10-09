// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "input/MenuPadInput.h"
#include "Window.h"
#include "desktops/Desktop.h"
#include "ingameWindows/IngameWindow.h"
#include "s25util/colors.h"
#include <algorithm>

MenuPadInput::MenuPadInput() = default;

PadDeviceId MenuPadInput::GetActingDevice() const
{
    return DeviceForSlot(actingSlot_);
}

PadDeviceId MenuPadInput::DeviceForSlot(const unsigned slot) const
{
    if(slot == NoSlot)
        return InvalidPadDevice;
    for(const PadDeviceId dev : router_.GetDevices())
    {
        if(router_.GetSlot(dev) == slot)
            return dev;
    }
    return InvalidPadDevice;
}

void MenuPadInput::Pump(const std::vector<PadEvent>& events, const unsigned elapsedMs, Desktop* desktop,
                        IngameWindow* topWnd)
{
    desktop_ = desktop;
    rootWnd_ = topWnd;
    // Im Menue ist das gerade bediente Ding oft ein IngameWindow und kein Desktop: iwConnecting
    // traegt den Uebergang von der Kartenauswahl in die Lobby, iwMsgbox jede Fehlermeldung.
    root_ = topWnd ? static_cast<Window*>(topWnd) : static_cast<Window*>(desktop);
    // Erst die Slotzahl, dann die Ereignisse: ein in DIESEM Frame angestecktes und sofort
    // benutztes Pad soll seinen Slot noch in diesem Frame bekommen. Dieselbe Reihenfolge wie in
    // dskGameInterface::UpdateInput.
    router_.SetNumSlots(desktop ? std::min(desktop->GetNumPadSlots(), MaxSlots) : 1u);
    router_.OnEvents(events);
    party_.Retain([this](const PadDeviceId device) noexcept { return router_.HasDevice(device); });
    if(rootWnd_)
        rootWnd_->ReconcileMenuPads(router_);

    // Die Wurzel kann sich zwischen zwei Frames geaendert haben, ohne dass ein Padereignis
    // daran beteiligt war: ein Desktopwechsel, ein geoeffnetes Fenster, eine Nachrichtenbox.
    // Verglichen wird nur der ZEIGER, dereferenziert wird die alte Wurzel nie - und beim
    // Desktopwechsel raeumt der WindowManager sie ohnehin ausdruecklich weg.
    for(unsigned slot = 0; slot < MaxSlots; ++slot)
    {
        if(hasDevice_[slot] && focus_[slot].GetRoot() != root_)
            ResetFocus(slot);
    }

    // Der Einstiegspunkt zieht nach, solange der Spieler noch nicht selbst navigiert hat -
    // siehe focusUntouched_ in MenuPadInput.h.
    if(root_)
    {
        for(unsigned slot = 0; slot < MaxSlots; ++slot)
        {
            if(!hasDevice_[slot] || !focusUntouched_[slot])
                continue;
            Window* entry = root_->GetPadEntryCtrl(slot);
            if(entry && focus_[slot].GetFocused() != entry)
                focus_[slot].FocusCtrl(entry);
        }
    }

    stepMs_ = elapsedMs;
    // Erst Zuordnung und Bewegung, dann die Knopfflanken - genau die Reihenfolge, die
    // PadRouter zusichert und die dskGameInterface fuer den Weltzeiger braucht.
    router_.UpdateMotion(elapsedMs, *this);
    router_.DispatchButtons(*this);
    swallowFrame_.fill(false);
}

void MenuPadInput::ResetFocus(const unsigned slot)
{
    FocusPath& focus = focus_[slot];
    focusUntouched_[slot] = true;
    focus.SetRoot(root_);
    // Die Wurzel darf sagen, wo ein Pad anfangen soll. Die Lobby setzt damit den Fokus eines
    // neu hinzukommenden Spielers auf die erste freie Sitzkarte - er sieht seinen Beitritt
    // also dort, wo er ihn erwartet, statt auf "Spiel starten". Eine Rueckfrage nennt hier
    // ihre harmlose Vorgabeantwort (iwMsgbox::GetPadEntryCtrl).
    if(root_)
    {
        if(Window* entry = root_->GetPadEntryCtrl(slot))
            focus.FocusCtrl(entry);
    }
}

void MenuPadInput::OnPadAssigned(const unsigned slot, const bool assigned)
{
    if(slot >= MaxSlots)
        return;
    hasDevice_[slot] = assigned;
    if(assigned)
    {
        swallowFrame_[slot] = true;
        ResetFocus(slot);
    } else
        focus_[slot].Clear();
}

void MenuPadInput::OnPadMove(const unsigned slot, const Position& delta)
{
    if(slot >= MaxSlots)
        return;
    if(rootWnd_)
    {
        const auto modalPolicy = rootWnd_->AllowsMenuPadInput(DeviceForSlot(slot));
        if(modalPolicy ? !*modalPolicy : desktop_ && !desktop_->AllowsPadWindowInput(slot))
            return;
    }
    if(delta != Position(0, 0))
        focusUntouched_[slot] = false;
    focus_[slot].OnPadMove(delta, stepMs_);
}

void MenuPadInput::OnPadCamera(unsigned /*slot*/, const Position& /*delta*/)
{
    // Es gibt im Menue keine Welt, die sich verschieben liesse.
}

void MenuPadInput::OnPadZoom(unsigned /*slot*/, float /*step*/)
{
    // dito
}

void MenuPadInput::OnPadButton(const unsigned slot, const PadButton button, const bool down)
{
    if(slot >= MaxSlots)
        return;
    if(swallowFrame_[slot])
        return; // Aufnahmedruck, siehe MenuPadInput.h
    if(!down)
        return;

    if(rootWnd_)
    {
        const auto modalPolicy = rootWnd_->AllowsMenuPadInput(DeviceForSlot(slot));
        if(modalPolicy ? !*modalPolicy : desktop_ && !desktop_->AllowsPadWindowInput(slot))
            return;
    }
    focusUntouched_[slot] = false;
    actingSlot_ = slot;
    const auto deviceMap = [this] {
        std::array<PadDeviceId, MaxSlots> devices{};
        for(const PadDeviceId device : router_.GetDevices())
        {
            const unsigned assignedSlot = router_.GetSlot(device);
            if(assignedSlot < MaxSlots)
                devices[assignedSlot] = device;
        }
        return devices;
    };
    const auto slotsBefore = deviceMap();
    // B und Start erreichen FocusPath::OnPadButton bewusst NICHT:
    //  - FocusPath::OnPadButton macht aus B ein Clear(), also "raus aus dem Fenster". Ingame ist
    //    das richtig (dahinter liegt die Welt), im Menue waere es eine Sackgasse: dahinter liegt
    //    nichts, und der Spieler haette keinen Weg zurueck. GEFRAGT wird das fokussierte Control
    //    trotzdem, aber nur nach dem einen: hast du eine offene Eingabe zu verwerfen?
    //  - Start ist ingame der Knopf, mit dem ein Spieler sein Pad in die Hand nimmt, und dort
    //    bewusst wirkungslos. Im Menue ist er die Vorgabeaktion des Bildschirms.
    // Beides beantwortet der Desktop; sagt er nichts dazu, passiert nichts.
    const bool modalHandled = rootWnd_ && rootWnd_->HandleMenuPadButton(GetActingDevice(), button);
    if(modalHandled || (button == PadButton::B && focus_[slot].Cancel()))
    {
        // The modal consumed its command, or B discarded an open dropdown. Neither may
        // also close a window or activate the desktop underneath.
    } else if(button == PadButton::B && rootWnd_)
    {
        // Steht der Spieler in einem Fenster, ist B das Fenster zu - dieselbe Wirkung, die die
        // Tastatur mit ESC hat, samt derselben Ausnahmen (WindowManager::RelayKeyboardMessage).
        if(!rootWnd_->IsPinned() && rootWnd_->getCloseBehavior() != CloseBehavior::Custom)
            rootWnd_->Close();
    } else if(button == PadButton::B
              || button == PadButton::Start
              // Videos have no controls. A can confirm the desktop itself only when no window owns input.
              || (button == PadButton::A && !rootWnd_ && !focus_[slot].GetFocused()))
    {
        if(desktop_ && !rootWnd_)
            desktop_->Msg_PadCommand(slot, button);
    } else
    {
        // Multi-seat desktops can keep card input with its owner before generic navigation.
        if(rootWnd_ || !desktop_ || !desktop_->HandlesPadControlCommands() || !desktop_->Msg_PadCommand(slot, button))
            focus_[slot].OnPadButton(button, down);
    }
    // PadRouter's queued edges name the slots before dispatch. A join/leave may reassign them;
    // consume residual edges on every changed slot so they cannot act for its new controller.
    const auto slotsAfter = deviceMap();
    for(unsigned i = 0; i < MaxSlots; ++i)
    {
        if(slotsBefore[i] != slotsAfter[i])
            swallowFrame_[i] = true;
    }
    actingSlot_ = NoSlot;
}

void MenuPadInput::DrawRings() const
{
    for(unsigned slot = 0; slot < MaxSlots; ++slot)
    {
        if(!hasDevice_[slot])
            continue;
        // Dieselben Farben wie die Spielerfarben der Partie, damit ein Sitzplatz im Menue und
        // im Spiel dieselbe Farbe hat. Kein Bezug auf eine Partie - es gibt hier noch keine.
        focus_[slot].DrawRing(PLAYER_COLORS[slot % PLAYER_COLORS.size()]);
    }
}

void MenuPadInput::OnRootDestroyed(const Window* wnd)
{
    for(auto& focus : focus_)
        focus.OnRootDestroyed(wnd);
    if(root_ == wnd)
        root_ = nullptr;
    if(static_cast<const Window*>(rootWnd_) == wnd)
        rootWnd_ = nullptr;
    if(static_cast<const Window*>(desktop_) == wnd)
        desktop_ = nullptr;
}

void MenuPadInput::ClearFocus()
{
    for(auto& focus : focus_)
        focus.ClearSilently();
    root_ = nullptr;
    rootWnd_ = nullptr;
    desktop_ = nullptr;
}

void MenuPadInput::Reset()
{
    ClearFocus();
    router_.Clear();
    party_.Clear();
    hasDevice_.fill(false);
    swallowFrame_.fill(false);
    focusUntouched_.fill(false);
    desktop_ = nullptr;
}
