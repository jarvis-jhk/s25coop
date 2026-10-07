// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Desktop.h"
#include "frontend/PageLayout.h"
#include "input/PlayerBrief.h"
#include "gameTypes/TextureColor.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class FocusPath;
class ctrlButton;

/// One full-screen page of the new front end (doc/coop/FrontEnd.md, slice F1).
///
/// Every page has the same frame: a header (back button, title, joined-player strip), content laid out as
/// tiles or a list, and a footer help line drawn with the in-game brief's key badges. Layout is in actual
/// render units and is redone on every resize, so the Deck's 1024×640 gets its own arrangement instead of
/// a stretched 800×600 one. Content items are ordinary buttons: a mouse click and controller A both arrive
/// in Msg_ButtonClick, so keyboard and mouse keep working on every page.
///
/// THE BACK STACK. A desktop switch destroys the old desktop (WindowManager::DoDesktopSwitch), so a page
/// cannot keep a pointer to where it came from. Instead each page carries its trail: one factory per page
/// below it plus the item that was chosen there. B, Escape and the back button rebuild the previous page
/// from its factory and put focus back on that item. The trail lives in the page, not in a global, so a
/// test or a classic desktop that creates a page directly gets a page without a way back and nothing else.
class dskFrontEndPage : public Desktop
{
public:
    /// Called long after the page that stored it is gone: capture values only, never a page's `this`.
    using Factory = std::function<std::unique_ptr<Desktop>()>;
    struct TrailEntry
    {
        Factory factory;
        /// Item to focus when coming back; 0 = the page's normal entry point.
        unsigned focusId = 0;
    };

    explicit dskFrontEndPage(std::string title);
    ~dskFrontEndPage() override;

    /// The page `factory` creates, as the root of a new trail (nothing to go back to from there).
    static std::unique_ptr<Desktop> Create(const Factory& factory);
    /// Switch to Create(factory).
    static Desktop* Show(const Factory& factory);

    /// Open the next page on top of this one; B there comes back here.
    Desktop* Open(const Factory& next);
    /// Back to the previous page. false = there is none and OnBackAtRoot did not handle it either.
    bool GoBack();
    /// Is there a previous page?
    bool CanGoBack() const;
    /// Does B do anything here? A previous page, or a root page that handles it (OnBackAtRoot).
    virtual bool HasBackAction() const { return CanGoBack(); }

    const std::vector<TrailEntry>& GetTrail() const { return trail_; }
    const frontend::PageFrame& GetFrame() const { return frame_; }
    /// The ids of the content items in display order.
    const std::vector<unsigned>& GetItems() const { return items_; }
    /// The footer help line as last drawn.
    const std::vector<brief::KeyHint>& GetFooterKeys() const { return footerKeys_; }
    /// Rects of the joined-player badges as last drawn, in slot order.
    const std::vector<Rect>& GetPlayerStrip() const { return stripRects_; }

    bool WantsPadInput() const override { return true; }
    bool Msg_PadCommand(unsigned slot, PadButton button) override;
    bool Msg_KeyDown(const KeyEvent& ke) override;
    void Msg_ButtonClick(unsigned ctrl_id) final;
    void Msg_ScreenResize(const ScreenResizeEvent& sr) override;
    /// Catches up with a resize that happened while this page waited for its switch: WindowManager sends
    /// Msg_ScreenResize to the current desktop only (seen at startup, where the window is resized while
    /// the first page is queued).
    void SetActive(bool activate = true) override;
    Window* GetPadEntryCtrl(unsigned slot) override;

    enum ControlIds
    {
        ID_btBack = 0,
        ID_txtTitle,
        /// First free ID for the page's own items and controls
        ID_FIRST_FREE
    };

protected:
    enum class ContentLayout
    {
        Tiles,
        List
    };
    /// Tiles keep `aspect` and never exceed `maxTile`.
    void UseTiles(const Extent& aspect, const Extent& maxTile);
    /// Rows of `rowHeight`, at most `maxWidth` wide.
    void UseList(unsigned rowHeight, unsigned maxWidth);

    /// A content item: a button laid out by the page. Order of calls = display and focus order, so ids
    /// must ascend (FocusPath walks in id order).
    ctrlButton* AddItem(unsigned id, const std::string& label, TextureColor tc = TextureColor::Green2);
    void SetTitle(const std::string& title);
    /// Where a controller starts on this page, unless coming back restores another item.
    void SetEntryFocus(unsigned ctrl_id) { restoreFocus_ = ctrl_id; }
    /// Recompute positions of everything; call after adding or hiding items.
    void Layout();

    /// After every Layout(): place the page's own controls (those not added with AddItem) in GetFrame().
    virtual void OnLayout() {}
    /// The footer help line for the controller in slot 0. Default: frontend::PageKeys.
    virtual std::vector<brief::KeyHint> FooterKeys(const FocusPath& focus) const;

    /// A content item (or any other button of the page) was clicked or activated with A.
    virtual void OnChoose(unsigned /*ctrl_id*/) {}
    /// B on a page without a previous page. true = handled (e.g. the home page asks whether to quit).
    /// Override HasBackAction with it, so the footer names B.
    virtual bool OnBackAtRoot() { return false; }

    void Draw_() override;

private:
    /// A queued desktop switch: this page is on its way out, and a second press in the same frame
    /// (B, B or A, B) must not queue another switch over the first one.
    static bool Leaving();
    void DrawFooter();
    void DrawPlayerStrip();

    std::vector<TrailEntry> trail_;
    /// How this page was created, so a page opened from here can come back. Empty for a page that was
    /// created directly instead of through Show/Open.
    Factory self_;
    /// Focus to restore after coming back (see TrailEntry::focusId).
    unsigned restoreFocus_ = 0;
    /// The item chosen last; it becomes the focus target when the next page comes back here.
    unsigned lastChosen_ = 0;

    std::vector<unsigned> items_;
    ContentLayout layout_ = ContentLayout::Tiles;
    Extent tileAspect_{4, 3};
    Extent maxTile_{320, 200};
    unsigned rowHeight_ = 40;
    unsigned listWidth_ = 480;

    frontend::PageFrame frame_;
    std::vector<brief::KeyHint> footerKeys_;
    std::vector<Rect> stripRects_;
    /// The menu background, drawn under the header and footer bars (Desktop itself gets none, so its
    /// Draw_ can keep the fps counter running without painting over the bars).
    glArchivItem_Bitmap* pageBackground_;
};
