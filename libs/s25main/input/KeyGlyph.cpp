// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "KeyGlyph.h"
#include "ogl/FontStyle.h"
#include "ogl/glFont.h"
#include <s25util/colors.h>
#include <algorithm>

namespace brief {

unsigned KeyBadgeColor(const KeyHint& input)
{
    if(input.input != KeyInput::Button)
        return neutralBadgeColor;
    switch(input.button)
    {
        case PadButton::A: return 0xFF287A35;
        case PadButton::B: return 0xFFA62F31;
        case PadButton::X: return 0xFF2864AA;
        case PadButton::Y: return 0xFFE5B832;
        default: return neutralBadgeColor;
    }
}

std::vector<KeyGlyphLine> LayoutKeyGlyphs(const std::vector<KeyHint>& keys, const glFont& font,
                                          const unsigned short width, const unsigned textColor)
{
    std::vector<KeyGlyphLine> lines;
    const std::string separator = "  -  ";
    const unsigned separatorWidth = font.getWidth(separator);
    unsigned usedWidth = 0;
    for(const auto& group : GroupKeys(keys))
    {
        std::vector<KeyRun> runs;
        unsigned groupWidth = 0;
        const auto append = [&](const std::string& text, const unsigned color, const unsigned badgeColor) {
            const unsigned runWidth = font.getWidth(text) + (badgeColor ? 2 * keyTextPadding : 0);
            runs.push_back(KeyRun{text, groupWidth, runWidth, color, badgeColor});
            groupWidth += runWidth;
        };
        for(const auto& input : group.inputs)
        {
            if(!runs.empty())
                append("/", textColor, 0);
            const unsigned color = KeyBadgeColor(input);
            append(KeyInputLabel(input), color == 0xFFE5B832 ? COLOR_BLACK : COLOR_WHITE, color);
        }
        append(std::string(" ") + KeyLabel(group.inputs.front().action), textColor, 0);

        if(groupWidth > width)
        {
            const auto text = group.text();
            for(auto& part : font.GetWrapInfo(text, width, width).CreateSingleStrings(text))
                lines.push_back(KeyGlyphLine{std::move(part), {}});
            // A fallback line may already occupy the entire available width.
            usedWidth = width;
            continue;
        }
        if(lines.empty() || usedWidth + separatorWidth + groupWidth > width)
        {
            lines.push_back(KeyGlyphLine{});
            usedWidth = 0;
        }
        auto& line = lines.back();
        if(usedWidth)
        {
            line.runs.push_back(KeyRun{separator, usedWidth, separatorWidth, textColor, 0});
            line.text += separator;
            usedWidth += separatorWidth;
        }
        for(auto& run : runs)
        {
            run.x += usedWidth;
            line.runs.push_back(std::move(run));
        }
        line.text += group.text();
        usedWidth += groupWidth;
    }
    return lines;
}

void EmitKeyBadge(const Rect& rect, const unsigned color, const std::function<void(const Rect&, unsigned)>& emit)
{
    const auto size = rect.getSize();
    const unsigned inset = std::min(2u, size.x / 2);
    // Inset top/bottom corners give the badge a button silhouette without bitmap assets.
    emit(Rect(rect.getOrigin() + DrawPoint(static_cast<int>(inset), 0), Extent(size.x - (2 * inset), size.y)), color);
    if(size.y > 2)
        emit(Rect(rect.getOrigin() + DrawPoint(0, 1), Extent(size.x, size.y - 2)), color);
}

void DrawKeyRuns(const DrawPoint& pos, const std::vector<KeyRun>& runs, const glFont& font,
                 const std::function<void(const Rect&, unsigned)>& drawRect)
{
    for(const auto& run : runs)
    {
        const DrawPoint origin = pos + DrawPoint(static_cast<int>(run.x), 0);
        if(run.badgeColor)
            EmitKeyBadge(Rect(origin, Extent(run.width, font.getHeight())), run.badgeColor, drawRect);
        font.Draw(origin + DrawPoint(run.badgeColor ? keyTextPadding : 0, 0), run.text, FontStyle{}, run.textColor);
    }
}

} // namespace brief
