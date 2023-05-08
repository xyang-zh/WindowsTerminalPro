/*++
Copyright (c) Microsoft Corporation

Module Name:
- IInputEvent.hpp

Abstract:
- Internal representation of public INPUT_RECORD struct.

Author:
- Austin Diviness (AustDi) 18-Aug-2017
--*/

#pragma once

#include <wil/common.h>
#include <wil/resource.h>

#ifndef ALTNUMPAD_BIT
// from winconp.h
#define ALTNUMPAD_BIT 0x04000000 // AltNumpad OEM char (copied from ntuser/inc/kbd.h)
#endif

#include <wtypes.h>

#include <unordered_set>
#include <memory>
#include <deque>
#include <ostream>

enum class InputEventType
{
    KeyEvent,
    MouseEvent,
    WindowBufferSizeEvent,
    MenuEvent,
    FocusEvent
};

class IInputEvent
{
public:
    static std::unique_ptr<IInputEvent> Create(const INPUT_RECORD& record);
    static std::deque<std::unique_ptr<IInputEvent>> Create(std::span<const INPUT_RECORD> records);
    static std::deque<std::unique_ptr<IInputEvent>> Create(const std::deque<INPUT_RECORD>& records);

    static std::vector<INPUT_RECORD> ToInputRecords(const std::deque<std::unique_ptr<IInputEvent>>& events);

    virtual ~IInputEvent() = 0;
    IInputEvent() = default;
    IInputEvent(const IInputEvent&) = default;
    IInputEvent(IInputEvent&&) = default;
    IInputEvent& operator=(const IInputEvent&) & = default;
    IInputEvent& operator=(IInputEvent&&) & = default;

    virtual INPUT_RECORD ToInputRecord() const noexcept = 0;

    virtual InputEventType EventType() const noexcept = 0;

#ifdef UNIT_TESTING
    friend std::wostream& operator<<(std::wostream& stream, const IInputEvent* const pEvent);
#endif
};

inline IInputEvent::~IInputEvent() = default;

#ifdef UNIT_TESTING
std::wostream& operator<<(std::wostream& stream, const IInputEvent* pEvent);
#endif

#define ALT_PRESSED (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)
#define CTRL_PRESSED (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)
#define MOD_PRESSED (SHIFT_PRESSED | ALT_PRESSED | CTRL_PRESSED)

// Note taken from VkKeyScan docs (https://msdn.microsoft.com/en-us/library/windows/desktop/ms646329(v=vs.85).aspx):
// For keyboard layouts that use the right-hand ALT key as a shift key
// (for example, the French keyboard layout), the shift state is
// represented by the value 6, because the right-hand ALT key is
// converted internally into CTRL+ALT.
struct VkKeyScanModState
{
    static const byte None = 0;
    static const byte ShiftPressed = 1;
    static const byte CtrlPressed = 2;
    static const byte ShiftAndCtrlPressed = ShiftPressed | CtrlPressed;
    static const byte AltPressed = 4;
    static const byte ShiftAndAltPressed = ShiftPressed | AltPressed;
    static const byte CtrlAndAltPressed = CtrlPressed | AltPressed;
    static const byte ModPressed = ShiftPressed | CtrlPressed | AltPressed;
};

enum class ModifierKeyState
{
    RightAlt,
    LeftAlt,
    RightCtrl,
    LeftCtrl,
    Shift,
    NumLock,
    ScrollLock,
    CapsLock,
    EnhancedKey,
    NlsDbcsChar,
    NlsAlphanumeric,
    NlsKatakana,
    NlsHiragana,
    NlsRoman,
    NlsImeConversion,
    AltNumpad,
    NlsImeDisable,
    ENUM_COUNT // must be the last element in the enum class
};

std::unordered_set<ModifierKeyState> FromVkKeyScan(const short vkKeyScanFlags);
std::unordered_set<ModifierKeyState> FromConsoleControlKeyFlags(const DWORD flags);
DWORD ToConsoleControlKeyFlag(const ModifierKeyState modifierKey) noexcept;

class KeyEvent : public IInputEvent
{
public:
    enum class Modifiers : DWORD
    {
        None = 0,
        RightAlt = RIGHT_ALT_PRESSED,
        LeftAlt = LEFT_ALT_PRESSED,
        RightCtrl = RIGHT_CTRL_PRESSED,
        LeftCtrl = LEFT_CTRL_PRESSED,
        Shift = SHIFT_PRESSED,
        NumLock = NUMLOCK_ON,
        ScrollLock = SCROLLLOCK_ON,
        CapsLock = CAPSLOCK_ON,
        EnhancedKey = ENHANCED_KEY,
        DbcsChar = NLS_DBCSCHAR,
        Alphanumeric = NLS_ALPHANUMERIC,
        Katakana = NLS_KATAKANA,
        Hiragana = NLS_HIRAGANA,
        Roman = NLS_ROMAN,
        ImeConvert = NLS_IME_CONVERSION,
        AltNumpad = ALTNUMPAD_BIT,
        ImeDisable = NLS_IME_DISABLE
    };

    constexpr KeyEvent(const KEY_EVENT_RECORD& record) :
        _keyDown{ !!record.bKeyDown },
        _repeatCount{ record.wRepeatCount },
        _virtualKeyCode{ record.wVirtualKeyCode },
        _virtualScanCode{ record.wVirtualScanCode },
        _charData{ record.uChar.UnicodeChar },
        _activeModifierKeys{ record.dwControlKeyState }
    {
    }

    constexpr KeyEvent(const bool keyDown,
                       const WORD repeatCount,
                       const WORD virtualKeyCode,
                       const WORD virtualScanCode,
                       const wchar_t charData,
                       const DWORD activeModifierKeys) :
        _keyDown{ keyDown },
        _repeatCount{ repeatCount },
        _virtualKeyCode{ virtualKeyCode },
        _virtualScanCode{ virtualScanCode },
        _charData{ charData },
        _activeModifierKeys{ activeModifierKeys }
    {
    }

    constexpr KeyEvent() noexcept :
        _keyDown{ 0 },
        _repeatCount{ 0 },
        _virtualKeyCode{ 0 },
        _virtualScanCode{ 0 },
        _charData{ 0 },
        _activeModifierKeys{ 0 }
    {
    }

    static std::pair<KeyEvent, KeyEvent> MakePair(
        const WORD repeatCount,
        const WORD virtualKeyCode,
        const WORD virtualScanCode,
        const wchar_t charData,
        const DWORD activeModifierKeys)
    {
        return std::make_pair<KeyEvent, KeyEvent>(
            { true,
              repeatCount,
              virtualKeyCode,
              virtualScanCode,
              charData,
              activeModifierKeys },
            { false,
              repeatCount,
              virtualKeyCode,
              virtualScanCode,
              charData,
              activeModifierKeys });
    }

    ~KeyEvent();
    KeyEvent(const KeyEvent&) = default;
    KeyEvent(KeyEvent&&) = default;
// For these two operators, there seems to be a bug in the compiler:
// See https://stackoverflow.com/a/60206505/1481137
//   > C.128 applies only to virtual member functions, but operator= is not
//   > virtual in your base class and neither does it have the same signature as
//   > in the derived class, so there is no reason for it to apply.
#pragma warning(push)
#pragma warning(disable : 26456)
    KeyEvent& operator=(const KeyEvent&) & = default;
    KeyEvent& operator=(KeyEvent&&) & = default;
#pragma warning(pop)

    INPUT_RECORD ToInputRecord() const noexcept override;
    InputEventType EventType() const noexcept override;

    constexpr bool IsShiftPressed() const noexcept
    {
        return WI_IsFlagSet(GetActiveModifierKeys(), SHIFT_PRESSED);
    }

    constexpr bool IsAltPressed() const noexcept
    {
        return WI_IsAnyFlagSet(GetActiveModifierKeys(), ALT_PRESSED);
    }

    constexpr bool IsCtrlPressed() const noexcept
    {
        return WI_IsAnyFlagSet(GetActiveModifierKeys(), CTRL_PRESSED);
    }

    constexpr bool IsAltGrPressed() const noexcept
    {
        return WI_IsFlagSet(GetActiveModifierKeys(), LEFT_CTRL_PRESSED) &&
               WI_IsFlagSet(GetActiveModifierKeys(), RIGHT_ALT_PRESSED);
    }

    constexpr bool IsModifierPressed() const noexcept
    {
        return WI_IsAnyFlagSet(GetActiveModifierKeys(), MOD_PRESSED);
    }

    constexpr bool IsCursorKey() const noexcept
    {
        // true iff vk in [End, Home, Left, Up, Right, Down]
        return (_virtualKeyCode >= VK_END) && (_virtualKeyCode <= VK_DOWN);
    }

    constexpr bool IsAltNumpadSet() const noexcept
    {
        return WI_IsFlagSet(GetActiveModifierKeys(), ALTNUMPAD_BIT);
    }

    constexpr bool IsKeyDown() const noexcept
    {
        return _keyDown;
    }

    constexpr bool IsPauseKey() const noexcept
    {
        return (_virtualKeyCode == VK_PAUSE);
    }

    constexpr WORD GetRepeatCount() const noexcept
    {
        return _repeatCount;
    }

    constexpr WORD GetVirtualKeyCode() const noexcept
    {
        return _virtualKeyCode;
    }

    constexpr WORD GetVirtualScanCode() const noexcept
    {
        return _virtualScanCode;
    }

    constexpr wchar_t GetCharData() const noexcept
    {
        return _charData;
    }

    constexpr DWORD GetActiveModifierKeys() const noexcept
    {
        return static_cast<DWORD>(_activeModifierKeys);
    }

    void SetKeyDown(const bool keyDown) noexcept;
    void SetRepeatCount(const WORD repeatCount) noexcept;
    void SetVirtualKeyCode(const WORD virtualKeyCode) noexcept;
    void SetVirtualScanCode(const WORD virtualScanCode) noexcept;
    void SetCharData(const char character) noexcept;
    void SetCharData(const wchar_t character) noexcept;

    void SetActiveModifierKeys(const DWORD activeModifierKeys) noexcept;
    void DeactivateModifierKey(const ModifierKeyState modifierKey) noexcept;
    void ActivateModifierKey(const ModifierKeyState modifierKey) noexcept;
    bool DoActiveModifierKeysMatch(const std::unordered_set<ModifierKeyState>& consoleModifiers) const noexcept;
    bool IsCommandLineEditingKey() const noexcept;
    bool IsPopupKey() const noexcept;

    // Function Description:
    // - Returns true if the given VKey represents a modifier key - shift, alt,
    //   control or the Win key.
    // Arguments:
    // - vkey: the VKEY to check
    // Return Value:
    // - true iff the key is a modifier key.
    constexpr static bool IsModifierKey(const WORD vkey)
    {
        return (vkey == VK_CONTROL) ||
               (vkey == VK_LCONTROL) ||
               (vkey == VK_RCONTROL) ||
               (vkey == VK_MENU) ||
               (vkey == VK_LMENU) ||
               (vkey == VK_RMENU) ||
               (vkey == VK_SHIFT) ||
               (vkey == VK_LSHIFT) ||
               (vkey == VK_RSHIFT) ||
               // There is no VK_WIN
               (vkey == VK_LWIN) ||
               (vkey == VK_RWIN);
    };

private:
    bool _keyDown;
    WORD _repeatCount;
    WORD _virtualKeyCode;
    WORD _virtualScanCode;
    wchar_t _charData;
    Modifiers _activeModifierKeys;

    friend constexpr bool operator==(const KeyEvent& a, const KeyEvent& b) noexcept;
#ifdef UNIT_TESTING
    friend std::wostream& operator<<(std::wostream& stream, const KeyEvent* const pKeyEvent);
#endif
};

constexpr bool operator==(const KeyEvent& a, const KeyEvent& b) noexcept
{
    return (a._keyDown == b._keyDown &&
            a._repeatCount == b._repeatCount &&
            a._virtualKeyCode == b._virtualKeyCode &&
            a._virtualScanCode == b._virtualScanCode &&
            a._charData == b._charData &&
            a._activeModifierKeys == b._activeModifierKeys);
}

#ifdef UNIT_TESTING
std::wostream& operator<<(std::wostream& stream, const KeyEvent* const pKeyEvent);
#endif

class MouseEvent : public IInputEvent
{
public:
    constexpr MouseEvent(const MOUSE_EVENT_RECORD& record) :
        _position{ til::wrap_coord(record.dwMousePosition) },
        _buttonState{ record.dwButtonState },
        _activeModifierKeys{ record.dwControlKeyState },
        _eventFlags{ record.dwEventFlags }
    {
    }

    constexpr MouseEvent(const til::point position,
                         const DWORD buttonState,
                         const DWORD activeModifierKeys,
                         const DWORD eventFlags) :
        _position{ position },
        _buttonState{ buttonState },
        _activeModifierKeys{ activeModifierKeys },
        _eventFlags{ eventFlags }
    {
    }

    ~MouseEvent();
    MouseEvent(const MouseEvent&) = default;
    MouseEvent(MouseEvent&&) = default;
    MouseEvent& operator=(const MouseEvent&) & = default;
    MouseEvent& operator=(MouseEvent&&) & = default;

    INPUT_RECORD ToInputRecord() const noexcept override;
    InputEventType EventType() const noexcept override;

    constexpr bool IsMouseMoveEvent() const noexcept
    {
        return _eventFlags == MOUSE_MOVED;
    }

    constexpr til::point GetPosition() const noexcept
    {
        return _position;
    }

    constexpr DWORD GetButtonState() const noexcept
    {
        return _buttonState;
    }

    constexpr DWORD GetActiveModifierKeys() const noexcept
    {
        return _activeModifierKeys;
    }

    constexpr DWORD GetEventFlags() const noexcept
    {
        return _eventFlags;
    }

    void SetPosition(const til::point position) noexcept;
    void SetButtonState(const DWORD buttonState) noexcept;
    void SetActiveModifierKeys(const DWORD activeModifierKeys) noexcept;
    void SetEventFlags(const DWORD eventFlags) noexcept;

private:
    til::point _position;
    DWORD _buttonState;
    DWORD _activeModifierKeys;
    DWORD _eventFlags;

#ifdef UNIT_TESTING
    friend std::wostream& operator<<(std::wostream& stream, const MouseEvent* const pMouseEvent);
#endif
};

#ifdef UNIT_TESTING
std::wostream& operator<<(std::wostream& stream, const MouseEvent* const pMouseEvent);
#endif

class WindowBufferSizeEvent : public IInputEvent
{
public:
    constexpr WindowBufferSizeEvent(const WINDOW_BUFFER_SIZE_RECORD& record) :
        _size{ til::wrap_coord_size(record.dwSize) }
    {
    }

    constexpr WindowBufferSizeEvent(const til::size size) :
        _size{ size }
    {
    }

    ~WindowBufferSizeEvent();
    WindowBufferSizeEvent(const WindowBufferSizeEvent&) = default;
    WindowBufferSizeEvent(WindowBufferSizeEvent&&) = default;
    WindowBufferSizeEvent& operator=(const WindowBufferSizeEvent&) & = default;
    WindowBufferSizeEvent& operator=(WindowBufferSizeEvent&&) & = default;

    INPUT_RECORD ToInputRecord() const noexcept override;
    InputEventType EventType() const noexcept override;

    constexpr til::size GetSize() const noexcept
    {
        return _size;
    }

    void SetSize(const til::size size) noexcept;

private:
    til::size _size;

#ifdef UNIT_TESTING
    friend std::wostream& operator<<(std::wostream& stream, const WindowBufferSizeEvent* const pEvent);
#endif
};

#ifdef UNIT_TESTING
std::wostream& operator<<(std::wostream& stream, const WindowBufferSizeEvent* const pEvent);
#endif

class MenuEvent : public IInputEvent
{
public:
    constexpr MenuEvent(const MENU_EVENT_RECORD& record) :
        _commandId{ record.dwCommandId }
    {
    }

    constexpr MenuEvent(const UINT commandId) :
        _commandId{ commandId }
    {
    }

    ~MenuEvent();
    MenuEvent(const MenuEvent&) = default;
    MenuEvent(MenuEvent&&) = default;
    MenuEvent& operator=(const MenuEvent&) & = default;
    MenuEvent& operator=(MenuEvent&&) & = default;

    INPUT_RECORD ToInputRecord() const noexcept override;
    InputEventType EventType() const noexcept override;

    constexpr UINT GetCommandId() const noexcept
    {
        return _commandId;
    }

    void SetCommandId(const UINT commandId) noexcept;

private:
    UINT _commandId;

#ifdef UNIT_TESTING
    friend std::wostream& operator<<(std::wostream& stream, const MenuEvent* const pMenuEvent);
#endif
};

#ifdef UNIT_TESTING
std::wostream& operator<<(std::wostream& stream, const MenuEvent* const pMenuEvent);
#endif

class FocusEvent : public IInputEvent
{
public:
    constexpr FocusEvent(const FOCUS_EVENT_RECORD& record) :
        _focus{ !!record.bSetFocus },
        _cameFromApi{ true }
    {
    }

    constexpr FocusEvent(const bool focus) :
        _focus{ focus },
        _cameFromApi{ false }
    {
    }

    ~FocusEvent();
    FocusEvent(const FocusEvent&) = default;
    FocusEvent(FocusEvent&&) = default;
    FocusEvent& operator=(const FocusEvent&) & = default;
    FocusEvent& operator=(FocusEvent&&) & = default;

    INPUT_RECORD ToInputRecord() const noexcept override;
    InputEventType EventType() const noexcept override;

    constexpr bool GetFocus() const noexcept
    {
        return _focus;
    }

    void SetFocus(const bool focus) noexcept;

    // BODGY - see FocusEvent.cpp for details.
    constexpr bool CameFromApi() const noexcept
    {
        return _cameFromApi;
    }

private:
    bool _focus;
    bool _cameFromApi;

#ifdef UNIT_TESTING
    friend std::wostream& operator<<(std::wostream& stream, const FocusEvent* const pFocusEvent);
#endif
};

#ifdef UNIT_TESTING
std::wostream& operator<<(std::wostream& stream, const FocusEvent* const pFocusEvent);
#endif
