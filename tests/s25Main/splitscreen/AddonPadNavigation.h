// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "MenuPadFixture.h"
#include <map>
#include <queue>
#include <stdexcept>
#include <vector>

namespace rttr::test {

/// Find a physical D-pad route. Addon shoulders belong to categories, so they cannot double
/// as a test-only linear focus iterator. Reconnect resets the real router to the same entry.
inline void FocusAddonControl(MenuPadFixture& fixture, PadDeviceId pad, const Window* target, unsigned slot = 0)
{
    if(fixture.focused(slot) == target)
        return;
    const auto restart = [&] {
        fixture.disconnect(pad);
        fixture.frame();
        fixture.pickUp(pad);
    };
    restart();
    std::map<const Window*, std::vector<PadButton>> routes;
    std::queue<const Window*> pending;
    routes.emplace(fixture.focused(slot), std::vector<PadButton>{});
    pending.push(fixture.focused(slot));
    while(!pending.empty())
    {
        const auto& route = routes.at(pending.front());
        pending.pop();
        for(const auto direction : {PadButton::DpadUp, PadButton::DpadDown, PadButton::DpadLeft, PadButton::DpadRight})
        {
            restart();
            for(const auto step : route)
                fixture.press(pad, step);
            if(fixture.focused(slot) == target)
                return;
            const Position delta = direction == PadButton::DpadUp ?
                                     Position(0, -1) :
                                     direction == PadButton::DpadDown ?
                                     Position(0, 1) :
                                     direction == PadButton::DpadLeft ? Position(-1, 0) : Position(1, 0);
            if(fixture.focused(slot) && fixture.focused(slot)->CanStepValue(delta))
                continue;
            fixture.press(pad, direction);
            const auto* reached = fixture.focused(slot);
            if(reached == target)
                return;
            if(routes.count(reached) == 0)
            {
                auto extended = route;
                extended.push_back(direction);
                routes.emplace(reached, std::move(extended));
                pending.push(reached);
            }
        }
    }
    throw std::runtime_error("Addon control is not physically reachable");
}
} // namespace rttr::test
