// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Window.h"
#include <memory>
#include <string>

struct MouseCoords;
class glFont;
class ctrlTextDeepening;
class iwControllerKeyboard;
struct KeyEvent;

enum class EditType
{
    Text,
    Number,
    Filename
};

enum class FileNameStatus
{
    Empty,
    Invalid,
    Valid
};
struct GetFileNameResult
{
    FileNameStatus status;
    std::string name; // only set when status == Valid
};

class ctrlEdit : public Window
{
public:
    ctrlEdit(Window* parent, unsigned id, const DrawPoint& pos, const Extent& size, TextureColor tc, const glFont* font,
             unsigned short maxlength = 0, bool password = false, bool disabled = false, bool notify = false);
    /// setzt den Text.
    void SetText(const std::string& text);
    void SetText(unsigned text);

    std::string GetText() const;
    /// Trims leading whitespace (and trailing whitespace only if ext is empty), appends a non-empty ext,
    /// validates; returns Empty/Invalid/Valid with filename. Requires EditType::Filename.
    GetFileNameResult GetFileName(const std::string& ext = "") const;
    void SetFocus(bool focus = true);
    bool HasFocus() const { return focus_; }
    void SetDisabled(bool disabled = true) { this->isDisabled_ = disabled; }
    void SetNotify(bool notify = true) { this->notify_ = notify; }
    void SetType(EditType type) { this->editType_ = type; }

    void Resize(const Extent& newSize) override;

    bool Msg_LeftDown(const MouseCoords& mc) override;
    bool Msg_KeyDown(const KeyEvent& ke) override;

    /// Legacy fields remain outside pad focus. New front-end callers opt in explicitly;
    /// focusing an opted-in field never sets the mouse/keyboard focus bit.
    void SetControllerKeyboardEnabled(bool enabled = true) { controllerKeyboardEnabled_ = enabled; }
    bool CanFocus() const override { return controllerKeyboardEnabled_ && !isDisabled_; }
    bool CanActivate() const override;
    bool Activate() override;
    bool WantsTextInput() const override { return focus_; }

protected:
    void Draw_() override;

private:
    friend class iwControllerKeyboard;
    // Created only when opening a keyboard. Lifetime plus revision invalidates a draft
    // after destruction or any external edit, including a change back to the initial text.
    std::shared_ptr<unsigned> KeyboardLifetime();
    std::shared_ptr<unsigned> controllerKeyboardLifetime_;
    bool controllerKeyboardEnabled_ = false;
    void AddChar(char32_t c);
    void RemoveChar();
    void Notify();
    void UpdateInternalText();

    void CursorLeft();
    void CursorRight();

    unsigned short maxLength_;
    ctrlTextDeepening* txtCtrl;
    bool isPassword_;
    bool isDisabled_;
    bool focus_ = false;
    bool notify_;

    std::u32string text_;
    /// Position of cursor in text (in UTF32 chars)
    unsigned cursorPos_ = 0;
    /// Offset of the cursor from the start of the text start position
    unsigned cursorOffsetX_ = 0;
    unsigned viewStart_ = 0;

    EditType editType_ = EditType::Text;
};
