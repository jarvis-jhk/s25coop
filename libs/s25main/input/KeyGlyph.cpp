// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "KeyGlyph.h"
#include "Loader.h"
#include "ogl/FontStyle.h"
#include "ogl/glArchivItem_Bitmap.h"
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

unsigned DpadBit(const KeyHint& input)
{
    if(input.input != KeyInput::Button)
        return 0;
    switch(input.button)
    {
        case PadButton::DpadUp: return 1;
        case PadButton::DpadDown: return 2;
        case PadButton::DpadLeft: return 4;
        case PadButton::DpadRight: return 8;
        default: return 0;
    }
}

unsigned PadGlyphIndex(const KeyHint& input)
{
    switch(input.input)
    {
        case KeyInput::LeftStickAxis: return padGlyphLeftStick;
        case KeyInput::RightStickAxis: return padGlyphRightStick;
        case KeyInput::LeftTriggerAxis: return padGlyphLT;
        case KeyInput::RightTriggerAxis: return padGlyphRT;
        case KeyInput::Button: break;
    }
    if(const unsigned bit = DpadBit(input))
        return padGlyphDpad - 1 + bit;
    switch(input.button)
    {
        case PadButton::A: return padGlyphA;
        case PadButton::B: return padGlyphB;
        case PadButton::X: return padGlyphX;
        case PadButton::Y: return padGlyphY;
        case PadButton::Back: return padGlyphView;
        case PadButton::Guide: return padGlyphGuide;
        case PadButton::Start: return padGlyphMenu;
        case PadButton::LeftStick: return padGlyphL3;
        case PadButton::RightStick: return padGlyphR3;
        case PadButton::LeftShoulder: return padGlyphLB;
        case PadButton::RightShoulder: return padGlyphRB;
        default: return padGlyphA;
    }
}

glArchivItem_Bitmap* LoaderGlyph(const unsigned index)
{
    // An install without the folder (or a test) leaves the archive empty: every input keeps its badge.
    return LOADER.GetImageN("pad_glyphs", index);
}

std::vector<KeyGlyphLine> LayoutKeyGlyphs(const std::vector<KeyHint>& keys, const glFont& font,
                                          const unsigned short width, const unsigned textColor,
                                          const GlyphLookup& glyphs)
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
        // All D-pad directions of a group become one image showing exactly those arms lit, but
        // only if that image exists; otherwise each direction keeps its own badge.
        unsigned dpadMask = 0;
        for(const auto& input : group.inputs)
            dpadMask |= DpadBit(input);
        glArchivItem_Bitmap* dpadGlyph = dpadMask ? glyphs(padGlyphDpad - 1 + dpadMask) : nullptr;
        bool dpadDrawn = false;
        for(const auto& input : group.inputs)
        {
            if(dpadGlyph && DpadBit(input))
            {
                if(dpadDrawn)
                    continue;
                dpadDrawn = true;
            }
            if(!runs.empty())
                append("/", textColor, 0);
            glArchivItem_Bitmap* glyph = (dpadGlyph && DpadBit(input)) ? dpadGlyph : glyphs(PadGlyphIndex(input));
            if(glyph)
            {
                runs.push_back(KeyRun{KeyInputLabel(input), groupWidth, glyph->getWidth(), textColor, 0, glyph});
                groupWidth += glyph->getWidth();
                continue;
            }
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
        if(run.glyph)
        {
            // Centred on the text line; a glyph a pixel taller than the font overhangs evenly.
            const int dy = (static_cast<int>(font.getHeight()) - static_cast<int>(run.glyph->getHeight())) / 2;
            run.glyph->DrawFull(origin + DrawPoint(0, dy));
            continue;
        }
        if(run.badgeColor)
            EmitKeyBadge(Rect(origin, Extent(run.width, font.getHeight())), run.badgeColor, drawRect);
        font.Draw(origin + DrawPoint(run.badgeColor ? keyTextPadding : 0, 0), run.text, FontStyle{}, run.textColor);
    }
}

} // namespace brief
