// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "stateMachine.hpp"
#include "InputStateMachineEngine.hpp"
#include "ascii.hpp"
#include "../input/terminalInput.hpp"
#include "../../inc/unicode.hpp"
#include "../../interactivity/inc/EventSynthesis.hpp"

#include <vector>
#include <functional>
#include <sstream>
#include <string>
#include <algorithm>

#include "../../../interactivity/inc/VtApiRedirection.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class InputEngineTest;
            class TestInteractDispatch;
        };
    };
};
using namespace Microsoft::Console::VirtualTerminal;

bool IsShiftPressed(const DWORD modifierState)
{
    return WI_IsFlagSet(modifierState, SHIFT_PRESSED);
}

bool IsAltPressed(const DWORD modifierState)
{
    return WI_IsAnyFlagSet(modifierState, LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);
}

bool IsCtrlPressed(const DWORD modifierState)
{
    return WI_IsAnyFlagSet(modifierState, LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
}

bool ModifiersEquivalent(DWORD a, DWORD b)
{
    auto fShift = IsShiftPressed(a) == IsShiftPressed(b);
    auto fAlt = IsAltPressed(a) == IsAltPressed(b);
    auto fCtrl = IsCtrlPressed(a) == IsCtrlPressed(b);
    auto fEnhanced = WI_IsFlagSet(a, ENHANCED_KEY) == WI_IsFlagSet(b, ENHANCED_KEY);
    return fShift && fCtrl && fAlt && fEnhanced;
}

class TestState
{
public:
    TestState() :
        vExpectedInput{},
        _stateMachine{ nullptr },
        _expectedToCallWindowManipulation{ false },
        _expectSendCtrlC{ false },
        _expectCursorPosition{ false },
        _expectedCursor{ -1, -1 },
        _expectedWindowManipulation{ DispatchTypes::WindowManipulationType::Invalid }
    {
    }

    void RoundtripTerminalInputCallback(_In_ std::deque<std::unique_ptr<IInputEvent>>& inEvents)
    {
        // Take all the characters out of the input records here, and put them into
        //  the input state machine.
        auto inputRecords = IInputEvent::ToInputRecords(inEvents);
        std::wstring vtseq = L"";
        for (auto& inRec : inputRecords)
        {
            VERIFY_ARE_EQUAL(KEY_EVENT, inRec.EventType);
            if (inRec.Event.KeyEvent.bKeyDown)
            {
                vtseq += &inRec.Event.KeyEvent.uChar.UnicodeChar;
            }
        }
        Log::Comment(
            NoThrowString().Format(L"\tvtseq: \"%s\"(%zu)", vtseq.c_str(), vtseq.length()));

        _stateMachine->ProcessString(vtseq);
        Log::Comment(L"String processed");
    }

    void TestInputCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents)
    {
        auto records = IInputEvent::ToInputRecords(inEvents);

        // This callback doesn't work super well for the Ctrl+C iteration of the
        // C0Test. For ^C, we always send a keydown and a key up event, however,
        // both calls to WriteCtrlKey happen in one single call to
        // ProcessString, and the test doesn't have a chance to load each key
        // into this callback individually. Instead, we'll just skip these
        // checks for the second call to WriteInput for this test.
        if (_expectSendCtrlC && vExpectedInput.size() == 0)
        {
            return;
        }
        VERIFY_ARE_EQUAL(1u, vExpectedInput.size());

        auto foundEqual = false;
        auto irExpected = vExpectedInput.back();

        Log::Comment(
            NoThrowString().Format(L"\texpected:\t") +
            VerifyOutputTraits<INPUT_RECORD>::ToString(irExpected));

        // Look for an equivalent input record.
        // Differences between left and right modifiers are ignored, as long as one is pressed.
        // There may be other keypresses, eg. modifier keypresses, those are ignored.
        for (auto& inRec : records)
        {
            Log::Comment(
                NoThrowString().Format(L"\tActual  :\t") +
                VerifyOutputTraits<INPUT_RECORD>::ToString(inRec));

            auto areEqual =
                (irExpected.EventType == inRec.EventType) &&
                (irExpected.Event.KeyEvent.bKeyDown == inRec.Event.KeyEvent.bKeyDown) &&
                (irExpected.Event.KeyEvent.wRepeatCount == inRec.Event.KeyEvent.wRepeatCount) &&
                (irExpected.Event.KeyEvent.uChar.UnicodeChar == inRec.Event.KeyEvent.uChar.UnicodeChar) &&
                ModifiersEquivalent(irExpected.Event.KeyEvent.dwControlKeyState, inRec.Event.KeyEvent.dwControlKeyState);

            foundEqual |= areEqual;
            if (areEqual)
            {
                Log::Comment(L"\t\tFound Match");
            }
        }

        VERIFY_IS_TRUE(foundEqual);
        vExpectedInput.clear();
    }

    void TestInputStringCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents)
    {
        auto records = IInputEvent::ToInputRecords(inEvents);

        for (auto expected : vExpectedInput)
        {
            Log::Comment(
                NoThrowString().Format(L"\texpected:\t") +
                VerifyOutputTraits<INPUT_RECORD>::ToString(expected));
        }

        auto irExpected = vExpectedInput.front();
        Log::Comment(
            NoThrowString().Format(L"\tLooking for:\t") +
            VerifyOutputTraits<INPUT_RECORD>::ToString(irExpected));

        // Look for an equivalent input record.
        // Differences between left and right modifiers are ignored, as long as one is pressed.
        // There may be other keypresses, eg. modifier keypresses, those are ignored.
        for (auto& inRec : records)
        {
            Log::Comment(
                NoThrowString().Format(L"\tActual  :\t") +
                VerifyOutputTraits<INPUT_RECORD>::ToString(inRec));

            auto areEqual =
                (irExpected.EventType == inRec.EventType) &&
                (irExpected.Event.KeyEvent.bKeyDown == inRec.Event.KeyEvent.bKeyDown) &&
                (irExpected.Event.KeyEvent.wRepeatCount == inRec.Event.KeyEvent.wRepeatCount) &&
                (irExpected.Event.KeyEvent.uChar.UnicodeChar == inRec.Event.KeyEvent.uChar.UnicodeChar) &&
                ModifiersEquivalent(irExpected.Event.KeyEvent.dwControlKeyState, inRec.Event.KeyEvent.dwControlKeyState);

            if (areEqual)
            {
                Log::Comment(L"\t\tFound Match");
                vExpectedInput.pop_front();
                if (vExpectedInput.size() > 0)
                {
                    irExpected = vExpectedInput.front();
                    Log::Comment(
                        NoThrowString().Format(L"\tLooking for:\t") +
                        VerifyOutputTraits<INPUT_RECORD>::ToString(irExpected));
                }
            }
        }
        VERIFY_ARE_EQUAL(static_cast<size_t>(0), vExpectedInput.size(), L"Verify we found all the inputs we were expecting");
        vExpectedInput.clear();
    }

    std::deque<INPUT_RECORD> vExpectedInput;
    StateMachine* _stateMachine;
    bool _expectedToCallWindowManipulation;
    bool _expectSendCtrlC;
    bool _expectCursorPosition;
    til::point _expectedCursor;
    DispatchTypes::WindowManipulationType _expectedWindowManipulation;
    std::array<unsigned short, 16> _expectedParams{};
};

class Microsoft::Console::VirtualTerminal::InputEngineTest
{
    TEST_CLASS(InputEngineTest);

    TestState testState;

    void RoundtripTerminalInputCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents);
    void TestInputCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents);
    void TestInputStringCallback(std::deque<std::unique_ptr<IInputEvent>>& inEvents);
    std::wstring GenerateSgrMouseSequence(const CsiMouseButtonCodes button,
                                          const unsigned short modifiers,
                                          const til::point position,
                                          const VTID direction);

    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiActionMouseCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)
    typedef std::tuple<CsiMouseButtonCodes, unsigned short, til::point, CsiActionCodes> SGR_PARAMS;

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags
    typedef std::tuple<DWORD, DWORD, til::point, DWORD> MOUSE_EVENT_PARAMS;

    void VerifySGRMouseData(const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData);

    // We need to manually call this at the end of the tests so that we know _which_ tests failed, rather than that the method cleanup failed
    void VerifyExpectedInputDrained();

    TEST_CLASS_SETUP(ClassSetup)
    {
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        return true;
    }

    TEST_METHOD(C0Test);
    TEST_METHOD(AlphanumericTest);
    TEST_METHOD(RoundTripTest);
    TEST_METHOD(WindowManipulationTest);
    TEST_METHOD(NonAsciiTest);
    TEST_METHOD(CursorPositioningTest);
    TEST_METHOD(CSICursorBackTabTest);
    TEST_METHOD(EnhancedKeysTest);
    TEST_METHOD(SS3CursorKeyTest);
    TEST_METHOD(AltBackspaceTest);
    TEST_METHOD(AltCtrlDTest);
    TEST_METHOD(AltIntermediateTest);
    TEST_METHOD(AltBackspaceEnterTest);
    TEST_METHOD(SGRMouseTest_ButtonClick);
    TEST_METHOD(SGRMouseTest_Modifiers);
    TEST_METHOD(SGRMouseTest_Movement);
    TEST_METHOD(SGRMouseTest_Scroll);
    TEST_METHOD(SGRMouseTest_DoubleClick);
    TEST_METHOD(SGRMouseTest_Hover);
    TEST_METHOD(CtrlAltZCtrlAltXTest);
    TEST_METHOD(TestSs3Entry);
    TEST_METHOD(TestSs3Immediate);
    TEST_METHOD(TestSs3Param);

    TEST_METHOD(TestWin32InputParsing);
    TEST_METHOD(TestWin32InputOptionals);

    friend class TestInteractDispatch;
};

void InputEngineTest::VerifyExpectedInputDrained()
{
    if (!testState.vExpectedInput.empty())
    {
        for (const auto& exp : testState.vExpectedInput)
        {
            switch (exp.EventType)
            {
            case KEY_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: KEY_EVENT");
                break;
            case MOUSE_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: MOUSE_EVENT");
                break;
            case WINDOW_BUFFER_SIZE_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: WINDOW_BUFFER_SIZE_EVENT");
                break;
            case MENU_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: MENU_EVENT");
                break;
            case FOCUS_EVENT:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: FOCUS_EVENT");
                break;
            default:
                Log::Error(L"EXPECTED INPUT NEVER RECEIVED: UNKNOWN TYPE");
                break;
            }
        }
        VERIFY_FAIL(L"there should be no remaining un-drained expected input");
        testState.vExpectedInput.clear();
    }
}

class Microsoft::Console::VirtualTerminal::TestInteractDispatch final : public IInteractDispatch
{
public:
    TestInteractDispatch(_In_ std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> pfn,
                         _In_ TestState* testState);
    virtual bool WriteInput(_In_ std::deque<std::unique_ptr<IInputEvent>>& inputEvents) override;

    virtual bool WriteCtrlKey(const KeyEvent& event) override;
    virtual bool WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                    const VTParameter parameter1,
                                    const VTParameter parameter2) override; // DTTERM_WindowManipulation
    virtual bool WriteString(const std::wstring_view string) override;

    virtual bool MoveCursor(const VTInt row,
                            const VTInt col) override;

    virtual bool IsVtInputEnabled() const override;

    virtual bool FocusChanged(const bool focused) const override;

private:
    std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> _pfnWriteInputCallback;
    TestState* _testState; // non-ownership pointer
};

TestInteractDispatch::TestInteractDispatch(_In_ std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> pfn,
                                           _In_ TestState* testState) :
    _pfnWriteInputCallback(pfn),
    _testState(testState)
{
}

bool TestInteractDispatch::WriteInput(_In_ std::deque<std::unique_ptr<IInputEvent>>& inputEvents)
{
    _pfnWriteInputCallback(inputEvents);
    return true;
}

bool TestInteractDispatch::WriteCtrlKey(const KeyEvent& event)
{
    VERIFY_IS_TRUE(_testState->_expectSendCtrlC);
    std::deque<std::unique_ptr<IInputEvent>> inputEvents;
    inputEvents.push_back(std::make_unique<KeyEvent>(event));
    return WriteInput(inputEvents);
}

bool TestInteractDispatch::WindowManipulation(const DispatchTypes::WindowManipulationType function,
                                              const VTParameter parameter1,
                                              const VTParameter parameter2)
{
    VERIFY_ARE_EQUAL(true, _testState->_expectedToCallWindowManipulation);
    VERIFY_ARE_EQUAL(_testState->_expectedWindowManipulation, function);
    VERIFY_ARE_EQUAL(_testState->_expectedParams[0], parameter1.value_or(0));
    VERIFY_ARE_EQUAL(_testState->_expectedParams[1], parameter2.value_or(0));
    return true;
}

bool TestInteractDispatch::WriteString(const std::wstring_view string)
{
    std::deque<std::unique_ptr<IInputEvent>> keyEvents;

    for (const auto& wch : string)
    {
        // We're forcing the translation to CP_USA, so that it'll be constant
        //  regardless of the CP the test is running in
        auto convertedEvents = Microsoft::Console::Interactivity::CharToKeyEvents(wch, CP_USA);
        std::move(convertedEvents.begin(),
                  convertedEvents.end(),
                  std::back_inserter(keyEvents));
    }

    return WriteInput(keyEvents);
}

bool TestInteractDispatch::MoveCursor(const VTInt row, const VTInt col)
{
    VERIFY_IS_TRUE(_testState->_expectCursorPosition);
    til::point received{ col, row };
    VERIFY_ARE_EQUAL(_testState->_expectedCursor, received);
    return true;
}

bool TestInteractDispatch::IsVtInputEnabled() const
{
    return true;
}

bool TestInteractDispatch::FocusChanged(const bool /*focused*/) const
{
    return false;
}

void InputEngineTest::C0Test()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);

    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    Log::Comment(L"Sending 0x0-0x19 to parser to make sure they're translated correctly back to C-key");

    for (wchar_t wch = '\x0'; wch < '\x20'; wch++)
    {
        auto inputSeq = std::wstring(&wch, 1);
        // In general, he actual key that we're going to generate for a C0 char
        //      is char+0x40 and with ctrl pressed.
        wchar_t sentWch = wch;
        wchar_t expectedWch = wch;
        auto writeCtrl = true;

        // Exceptional cases.
        switch (wch)
        {
        case L'\r': // Enter
            writeCtrl = false;
            break;
        case L'\x1b': // Escape
            writeCtrl = false;
            break;
        case L'\t': // Tab
            writeCtrl = false;
            break;
        case L'\b': // backspace
            sentWch = '\x7f';
            break;
        }

        auto keyscan = OneCoreSafeVkKeyScanW(expectedWch);
        short vkey = keyscan & 0xff;
        short keyscanModifiers = (keyscan >> 8) & 0xff;
        auto scanCode = (WORD)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        DWORD dwModifierState = 0;
        if (writeCtrl)
        {
            dwModifierState = WI_SetFlag(dwModifierState, LEFT_CTRL_PRESSED);
        }
        // If we need to press shift for this key, but not on alphabetical chars
        //  Eg simulating C-z, not C-S-z.
        if (WI_IsFlagSet(keyscanModifiers, 1) && (expectedWch < L'A' || expectedWch > L'Z'))
        {
            dwModifierState = WI_SetFlag(dwModifierState, SHIFT_PRESSED);
        }

        // Just make sure we write the same thing telnetd did:
        if (sentWch == UNICODE_ETX)
        {
            Log::Comment(NoThrowString().Format(
                L"We used to expect 0x%x, 0x%x, 0x%x, 0x%x here",
                vkey,
                scanCode,
                sentWch,
                dwModifierState));
            vkey = 'C';
            scanCode = 0;
            sentWch = UNICODE_ETX;
            dwModifierState = LEFT_CTRL_PRESSED;
            Log::Comment(NoThrowString().Format(
                L"Now we expect 0x%x, 0x%x, 0x%x, 0x%x here",
                vkey,
                scanCode,
                sentWch,
                dwModifierState));
            testState._expectSendCtrlC = true;
        }
        else
        {
            testState._expectSendCtrlC = false;
        }

        Log::Comment(NoThrowString().Format(L"Testing char 0x%x", sentWch));
        Log::Comment(NoThrowString().Format(L"Input Sequence=\"%s\"", inputSeq.c_str()));

        INPUT_RECORD inputRec;

        inputRec.EventType = KEY_EVENT;
        inputRec.Event.KeyEvent.bKeyDown = TRUE;
        inputRec.Event.KeyEvent.dwControlKeyState = dwModifierState;
        inputRec.Event.KeyEvent.wRepeatCount = 1;
        inputRec.Event.KeyEvent.wVirtualKeyCode = vkey;
        inputRec.Event.KeyEvent.wVirtualScanCode = scanCode;
        inputRec.Event.KeyEvent.uChar.UnicodeChar = sentWch;

        testState.vExpectedInput.push_back(inputRec);

        _stateMachine->ProcessString(inputSeq);
    }
    VerifyExpectedInputDrained();
}

void InputEngineTest::AlphanumericTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    Log::Comment(L"Sending every printable ASCII character");
    DisableVerifyExceptions disable;
    for (wchar_t wch = '\x20'; wch < '\x7f'; wch++)
    {
        auto inputSeq = std::wstring(&wch, 1);

        auto keyscan = OneCoreSafeVkKeyScanW(wch);
        short vkey = keyscan & 0xff;
        WORD scanCode = (wchar_t)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        short keyscanModifiers = (keyscan >> 8) & 0xff;
        // Because of course, these are not the same flags.
        DWORD dwModifierState = 0 |
                                (WI_IsFlagSet(keyscanModifiers, 1) ? SHIFT_PRESSED : 0) |
                                (WI_IsFlagSet(keyscanModifiers, 2) ? LEFT_CTRL_PRESSED : 0) |
                                (WI_IsFlagSet(keyscanModifiers, 4) ? LEFT_ALT_PRESSED : 0);

        Log::Comment(NoThrowString().Format(L"Testing char 0x%x", wch));
        Log::Comment(NoThrowString().Format(L"Input Sequence=\"%s\"", inputSeq.c_str()));

        INPUT_RECORD inputRec;
        inputRec.EventType = KEY_EVENT;
        inputRec.Event.KeyEvent.bKeyDown = TRUE;
        inputRec.Event.KeyEvent.dwControlKeyState = dwModifierState;
        inputRec.Event.KeyEvent.wRepeatCount = 1;
        inputRec.Event.KeyEvent.wVirtualKeyCode = vkey;
        inputRec.Event.KeyEvent.wVirtualScanCode = scanCode;
        inputRec.Event.KeyEvent.uChar.UnicodeChar = wch;

        testState.vExpectedInput.push_back(inputRec);

        _stateMachine->ProcessString(inputSeq);
    }
    VerifyExpectedInputDrained();
}

void InputEngineTest::RoundTripTest()
{
    // TODO GH #4405: This test fails.
    Log::Result(WEX::Logging::TestResults::Skipped);
    return;

    /*
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    // Send Every VKEY through the TerminalInput module, then take the char's
    //   from the generated INPUT_RECORDs and put them through the InputEngine.
    // The VKEY sequence it writes out should be the same as the original.

    auto pfn2 = std::bind(&TestState::RoundtripTerminalInputCallback, &testState, std::placeholders::_1);
    TerminalInput terminalInput{ pfn2 };

    for (BYTE vkey = 0; vkey < BYTE_MAX; vkey++)
    {
        wchar_t wch = (wchar_t)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR);
        WORD scanCode = (wchar_t)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        unsigned int uiActualKeystate = 0;

        // Couple of exceptional cases here:
        if (vkey >= 'A' && vkey <= 'Z')
        {
            // A-Z need shift pressed in addition to the 'a'-'z' chars.
            uiActualKeystate = WI_SetFlag(uiActualKeystate, SHIFT_PRESSED);
        }
        else if (vkey == VK_CANCEL || vkey == VK_PAUSE)
        {
            uiActualKeystate = WI_SetFlag(uiActualKeystate, LEFT_CTRL_PRESSED);
        }

        if (vkey == UNICODE_ETX)
        {
            testState._expectSendCtrlC = true;
        }

        INPUT_RECORD irTest = { 0 };
        irTest.EventType = KEY_EVENT;
        irTest.Event.KeyEvent.dwControlKeyState = uiActualKeystate;
        irTest.Event.KeyEvent.wRepeatCount = 1;
        irTest.Event.KeyEvent.wVirtualKeyCode = vkey;
        irTest.Event.KeyEvent.bKeyDown = TRUE;
        irTest.Event.KeyEvent.uChar.UnicodeChar = wch;
        irTest.Event.KeyEvent.wVirtualScanCode = scanCode;

        Log::Comment(
            NoThrowString().Format(L"Expecting::   ") +
            VerifyOutputTraits<INPUT_RECORD>::ToString(irTest));

        testState.vExpectedInput.clear();
        testState.vExpectedInput.push_back(irTest);

        auto inputKey = IInputEvent::Create(irTest);
        terminalInput.HandleKey(inputKey.get());
    }

    VerifyExpectedInputDrained();
    */
}

void InputEngineTest::WindowManipulationTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine.get());
    testState._stateMachine = _stateMachine.get();

    Log::Comment(NoThrowString().Format(
        L"Try sending a bunch of Window Manipulation sequences. "
        L"Only the valid ones should call the "
        L"TestInteractDispatch::WindowManipulation callback."));

    const auto param1 = 123;
    const auto param2 = 456;
    const auto wszParam1 = L"123";
    const auto wszParam2 = L"456";

    for (unsigned int i = 0; i < static_cast<unsigned int>(BYTE_MAX); i++)
    {
        std::wstringstream seqBuilder;
        seqBuilder << L"\x1b[" << i;

        if (i == DispatchTypes::WindowManipulationType::ResizeWindowInCharacters)
        {
            // We need to build the string with the params as strings for some reason -
            //      x86 would implicitly convert them to chars (eg 123 -> '{')
            //      before appending them to the string
            seqBuilder << L";" << wszParam1 << L";" << wszParam2;

            testState._expectedToCallWindowManipulation = true;
            testState._expectedParams[0] = param1;
            testState._expectedParams[1] = param2;
            testState._expectedWindowManipulation = static_cast<DispatchTypes::WindowManipulationType>(i);
        }
        else
        {
            // other operations don't expect any params.

            testState._expectedToCallWindowManipulation = true;
            testState._expectedParams[0] = 0;
            testState._expectedParams[1] = 0;
            testState._expectedWindowManipulation = static_cast<DispatchTypes::WindowManipulationType>(i);
        }
        seqBuilder << L"t";
        auto seq = seqBuilder.str();
        Log::Comment(NoThrowString().Format(
            L"Processing \"%s\"", seq.c_str()));
        _stateMachine->ProcessString(seq);
    }
    VerifyExpectedInputDrained();
}

void InputEngineTest::NonAsciiTest()
{
    auto pfn = std::bind(&TestState::TestInputStringCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine.get());
    testState._stateMachine = _stateMachine.get();
    Log::Comment(L"Sending various non-ascii strings, and seeing what we get out");

    INPUT_RECORD proto = { 0 };
    proto.EventType = KEY_EVENT;
    proto.Event.KeyEvent.dwControlKeyState = 0;
    proto.Event.KeyEvent.wRepeatCount = 1;
    proto.Event.KeyEvent.wVirtualKeyCode = 0;
    proto.Event.KeyEvent.wVirtualScanCode = 0;
    // Fill these in for each char
    proto.Event.KeyEvent.bKeyDown = TRUE;
    proto.Event.KeyEvent.uChar.UnicodeChar = UNICODE_NULL;

    Log::Comment(NoThrowString().Format(
        L"We're sending utf-16 characters here, because the VtInputThread has "
        L"already converted the ut8 input to utf16 by the time it calls the state machine."));

    // "Л", UTF-16: 0x041B, utf8: "\xd09b"
    std::wstring utf8Input = L"\x041B";
    auto test = proto;
    test.Event.KeyEvent.uChar.UnicodeChar = utf8Input[0];

    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", utf8Input.c_str()));

    testState.vExpectedInput.clear();
    testState.vExpectedInput.push_back(test);
    test.Event.KeyEvent.bKeyDown = FALSE;
    testState.vExpectedInput.push_back(test);
    _stateMachine->ProcessString(utf8Input);

    // "旅", UTF-16: 0x65C5, utf8: "0xE6 0x97 0x85"
    utf8Input = L"\u65C5";
    test = proto;
    test.Event.KeyEvent.uChar.UnicodeChar = utf8Input[0];

    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", utf8Input.c_str()));

    testState.vExpectedInput.clear();
    testState.vExpectedInput.push_back(test);
    test.Event.KeyEvent.bKeyDown = FALSE;
    testState.vExpectedInput.push_back(test);
    _stateMachine->ProcessString(utf8Input);
    VerifyExpectedInputDrained();
}

void InputEngineTest::CursorPositioningTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);

    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    VERIFY_IS_NOT_NULL(dispatch.get());
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch), true);
    VERIFY_IS_NOT_NULL(inputEngine.get());
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    Log::Comment(NoThrowString().Format(
        L"Try sending a cursor position response, then send it again. "
        L"The first time, it should be interpreted as a cursor position. "
        L"The state machine engine should reset itself to normal operation "
        L"after that, and treat the second as an F3."));

    std::wstring seq = L"\x1b[1;4R";
    testState._expectCursorPosition = true;
    testState._expectedCursor = { 4, 1 };

    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", seq.c_str()));
    _stateMachine->ProcessString(seq);

    testState._expectCursorPosition = false;

    INPUT_RECORD inputRec;
    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED | SHIFT_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_F3;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_F3, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\0';

    testState.vExpectedInput.push_back(inputRec);
    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", seq.c_str()));
    _stateMachine->ProcessString(seq);
    VerifyExpectedInputDrained();
}

void InputEngineTest::CSICursorBackTabTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    INPUT_RECORD inputRec;

    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = SHIFT_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_TAB;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_TAB, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\t';

    testState.vExpectedInput.push_back(inputRec);

    const std::wstring seq = L"\x1b[Z";
    Log::Comment(NoThrowString().Format(
        L"Processing \"%s\"", seq.c_str()));
    _stateMachine->ProcessString(seq);
    VerifyExpectedInputDrained();
}

void InputEngineTest::EnhancedKeysTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    // The following vkeys should be handled as enhanced keys
    // Reference: https://docs.microsoft.com/en-us/windows/console/key-event-record-str
    // clang-format off
    const std::map<int, std::wstring> enhancedKeys{
        { VK_PRIOR,  L"\x1b[5~"},
        { VK_NEXT,   L"\x1b[6~"},
        { VK_END,    L"\x1b[F"},
        { VK_HOME,   L"\x1b[H"},
        { VK_LEFT,   L"\x1b[D"},
        { VK_UP,     L"\x1b[A"},
        { VK_RIGHT,  L"\x1b[C"},
        { VK_DOWN,   L"\x1b[B"},
        { VK_INSERT, L"\x1b[2~"},
        { VK_DELETE, L"\x1b[3~"}
    };
    // clang-format on

    for (const auto& [vkey, seq] : enhancedKeys)
    {
        INPUT_RECORD inputRec;

        const auto wch = (wchar_t)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR);
        const auto scanCode = (WORD)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        inputRec.EventType = KEY_EVENT;
        inputRec.Event.KeyEvent.bKeyDown = TRUE;
        inputRec.Event.KeyEvent.dwControlKeyState = ENHANCED_KEY;
        inputRec.Event.KeyEvent.wRepeatCount = 1;
        inputRec.Event.KeyEvent.wVirtualKeyCode = static_cast<WORD>(vkey);
        inputRec.Event.KeyEvent.wVirtualScanCode = scanCode;
        inputRec.Event.KeyEvent.uChar.UnicodeChar = wch;

        testState.vExpectedInput.push_back(inputRec);

        Log::Comment(NoThrowString().Format(
            L"Processing \"%s\"", seq.c_str()));
        _stateMachine->ProcessString(seq);
    }
    VerifyExpectedInputDrained();
}

void InputEngineTest::SS3CursorKeyTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    // clang-format off
    const std::map<int, std::wstring> cursorKeys{
        { VK_UP,    L"\x1bOA" },
        { VK_DOWN,  L"\x1bOB" },
        { VK_RIGHT, L"\x1bOC" },
        { VK_LEFT,  L"\x1bOD" },
        { VK_HOME,  L"\x1bOH" },
        { VK_END,   L"\x1bOF" },
    };
    // clang-format on

    for (const auto& [vkey, seq] : cursorKeys)
    {
        INPUT_RECORD inputRec;

        const auto wch = (wchar_t)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR);
        const auto scanCode = (WORD)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        inputRec.EventType = KEY_EVENT;
        inputRec.Event.KeyEvent.bKeyDown = TRUE;
        inputRec.Event.KeyEvent.dwControlKeyState = 0;
        inputRec.Event.KeyEvent.wRepeatCount = 1;
        inputRec.Event.KeyEvent.wVirtualKeyCode = static_cast<WORD>(vkey);
        inputRec.Event.KeyEvent.wVirtualScanCode = scanCode;
        inputRec.Event.KeyEvent.uChar.UnicodeChar = wch;

        testState.vExpectedInput.push_back(inputRec);

        Log::Comment(NoThrowString().Format(
            L"Processing \"%s\"", seq.c_str()));
        _stateMachine->ProcessString(seq);
    }
    VerifyExpectedInputDrained();
}

void InputEngineTest::AltBackspaceTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    INPUT_RECORD inputRec;

    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_BACK;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_BACK, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\x08';

    testState.vExpectedInput.push_back(inputRec);

    const std::wstring seq = L"\x1b\x7f";
    Log::Comment(NoThrowString().Format(L"Processing \"\\x1b\\x7f\""));
    _stateMachine->ProcessString(seq);

    VerifyExpectedInputDrained();
}

void InputEngineTest::AltCtrlDTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    INPUT_RECORD inputRec;

    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = 0x44; // D key
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(OneCoreSafeMapVirtualKeyW(0x44, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\x04';

    testState.vExpectedInput.push_back(inputRec);

    const std::wstring seq = L"\x1b\x04";
    Log::Comment(NoThrowString().Format(L"Processing \"\\x1b\\x04\""));
    _stateMachine->ProcessString(seq);

    VerifyExpectedInputDrained();
}

void InputEngineTest::AltIntermediateTest()
{
    // Tests GH#1209. When we process a alt+key combination where the key just
    // so happens to be an intermediate character, we should make sure that an
    // immediately subsequent ctrl character is handled correctly.

    // We'll test this by creating both a TerminalInput and an
    // InputStateMachine, and piping the KeyEvents generated by the
    // InputStateMachine into the TerminalInput.
    std::wstring expectedTranslation{};

    // First create the callback TerminalInput will call - this will be
    // triggered second, after both the state machine and the TerminalInput have
    // translated the characters.
    auto pfnTerminalInputCallback = [&](std::deque<std::unique_ptr<IInputEvent>>& inEvents) {
        // Get all the characters:
        std::wstring wstr = L"";
        for (auto& ev : inEvents)
        {
            if (ev->EventType() == InputEventType::KeyEvent)
            {
                auto& k = static_cast<KeyEvent&>(*ev);
                auto wch = k.GetCharData();
                wstr += wch;
            }
        }

        VERIFY_ARE_EQUAL(expectedTranslation, wstr);
    };
    TerminalInput terminalInput{ pfnTerminalInputCallback };

    // Create the callback that's fired when the state machine wants to write
    // input. We'll take the events and put them straight into the
    // TerminalInput.
    auto pfnInputStateMachineCallback = [&](std::deque<std::unique_ptr<IInputEvent>>& inEvents) {
        for (auto& ev : inEvents)
        {
            terminalInput.HandleKey(ev.get());
        }
    };
    auto dispatch = std::make_unique<TestInteractDispatch>(pfnInputStateMachineCallback, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(stateMachine);
    testState._stateMachine = stateMachine.get();

    // Write a Alt+/, Ctrl+e pair to the input engine, then take its output and
    // run it through the terminalInput translator. We should get ^[/^E back
    // out.
    std::wstring seq = L"\x1b/";
    expectedTranslation = seq;
    Log::Comment(NoThrowString().Format(L"Processing \"\\x1b/\""));
    stateMachine->ProcessString(seq);

    seq = L"\x05"; // 0x05 is ^E
    expectedTranslation = seq;
    Log::Comment(NoThrowString().Format(L"Processing \"\\x05\""));
    stateMachine->ProcessString(seq);

    VerifyExpectedInputDrained();
}

void InputEngineTest::AltBackspaceEnterTest()
{
    // Created as a test for microsoft/terminal#2746. See that issue for mode
    // details. We're going to send an Alt+Backspace to conpty, followed by an
    // enter. The enter should be processed as just a single VK_ENTER, not a
    // alt+enter.

    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    INPUT_RECORD inputRec;

    inputRec.EventType = KEY_EVENT;
    inputRec.Event.KeyEvent.bKeyDown = TRUE;
    inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED;
    inputRec.Event.KeyEvent.wRepeatCount = 1;
    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_BACK;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_BACK, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\x08';

    // First, expect a alt+backspace.
    testState.vExpectedInput.push_back(inputRec);

    std::wstring seq = L"\x1b\x7f";
    Log::Comment(NoThrowString().Format(L"Processing \"\\x1b\\x7f\""));
    _stateMachine->ProcessString(seq);

    // Ensure the state machine has correctly returned to the ground state
    VERIFY_ARE_EQUAL(StateMachine::VTStates::Ground, _stateMachine->_state);

    inputRec.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    inputRec.Event.KeyEvent.dwControlKeyState = 0;
    inputRec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(OneCoreSafeMapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC));
    inputRec.Event.KeyEvent.uChar.UnicodeChar = L'\x0d'; //maybe \xa

    // Then, expect a enter
    testState.vExpectedInput.push_back(inputRec);

    seq = L"\x0d";
    Log::Comment(NoThrowString().Format(L"Processing \"\\x0d\""));
    _stateMachine->ProcessString(seq);

    // Ensure the state machine has correctly returned to the ground state
    VERIFY_ARE_EQUAL(StateMachine::VTStates::Ground, _stateMachine->_state);

    VerifyExpectedInputDrained();
}

// Method Description:
// - Writes an SGR VT sequence based on the necessary parameters
// Arguments:
// - button - the state of the buttons (constructed via InputStateMachineEngine::CsiActionMouseCodes)
// - modifiers - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
// - position - the {x,y} position of the event on the viewport where the top-left is {1,1}
// - direction - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)
// Return Value:
// - the SGR VT sequence
std::wstring InputEngineTest::GenerateSgrMouseSequence(const CsiMouseButtonCodes button,
                                                       const unsigned short modifiers,
                                                       const til::point position,
                                                       const VTID direction)
{
    // we first need to convert "button" and "modifiers" into an 8 bit sequence
    unsigned int actionCode = 0;

    // button represents the top 2 and bottom 2 bits
    actionCode |= (button & 0b1100);
    actionCode = actionCode << 4;
    actionCode |= (button & 0b0011);

    // modifiers represents the middle 4 bits
    actionCode |= modifiers;

    // mouse sequence identifiers consist of a private parameter prefix and a final character
    const wchar_t prefixChar = direction[0];
    const wchar_t finalChar = direction[1];

    return wil::str_printf_failfast<std::wstring>(L"\x1b[%c%d;%d;%d%c", prefixChar, static_cast<int>(actionCode), position.x, position.y, finalChar);
}

void InputEngineTest::VerifySGRMouseData(const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData)
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);

    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    // The tests may be running somewhere that doesn't report anything for GetDoubleClickTime.
    // Let's force it to a high value to make the double click tests pass.
    inputEngine->_doubleClickTime = std::chrono::milliseconds(1000);
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    SGR_PARAMS input;
    MOUSE_EVENT_PARAMS expected;
    INPUT_RECORD inputRec;
    for (size_t i = 0; i < testData.size(); i++)
    {
        // construct test input
        input = std::get<0>(testData[i]);
        const auto seq = GenerateSgrMouseSequence(std::get<0>(input), std::get<1>(input), std::get<2>(input), std::get<3>(input));

        // construct expected result
        expected = std::get<1>(testData[i]);
        inputRec.EventType = MOUSE_EVENT;
        inputRec.Event.MouseEvent.dwButtonState = std::get<0>(expected);
        inputRec.Event.MouseEvent.dwControlKeyState = std::get<1>(expected);
        inputRec.Event.MouseEvent.dwMousePosition = til::unwrap_coord(std::get<2>(expected));
        inputRec.Event.MouseEvent.dwEventFlags = std::get<3>(expected);

        testState.vExpectedInput.push_back(inputRec);

        Log::Comment(NoThrowString().Format(L"Processing \"%s\"", seq.c_str()));
        _stateMachine->ProcessString(seq);
    }

    VerifyExpectedInputDrained();
}

void InputEngineTest::SGRMouseTest_ButtonClick()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                     EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseDown },         { FROM_LEFT_1ST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseUp },           { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseDown },       { FROM_LEFT_2ND_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseUp },         { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseUp },          { 0, 0, { 0, 0 }, 0 } },
    };
    // clang-format on

    VerifySGRMouseData(testData);
}

void InputEngineTest::SGRMouseTest_Modifiers()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                                               EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::Left, CsiMouseModifierCodes::Shift, { 1, 1 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED, SHIFT_PRESSED, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Left, CsiMouseModifierCodes::Shift, { 1, 1 }, CsiActionCodes::MouseUp },          { 0, SHIFT_PRESSED, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Middle, CsiMouseModifierCodes::Meta, { 1, 1 }, CsiActionCodes::MouseDown },       { FROM_LEFT_2ND_BUTTON_PRESSED, LEFT_ALT_PRESSED, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Middle, CsiMouseModifierCodes::Meta, { 1, 1 }, CsiActionCodes::MouseUp },         { 0, LEFT_ALT_PRESSED, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Right, CsiMouseModifierCodes::Ctrl, { 1, 1 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, LEFT_CTRL_PRESSED, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Right, CsiMouseModifierCodes::Ctrl, { 1, 1 }, CsiActionCodes::MouseUp },          { 0, LEFT_CTRL_PRESSED, { 0, 0 }, 0 } },
    };
    // clang-format on

    VerifySGRMouseData(testData);
}

void InputEngineTest::SGRMouseTest_Movement()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                                               EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::Right, 0,                           { 1, 1 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Right, CsiMouseModifierCodes::Drag, { 1, 2 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, 0, { 0, 1 }, MOUSE_MOVED } },
        {   { CsiMouseButtonCodes::Right, CsiMouseModifierCodes::Drag, { 2, 2 }, CsiActionCodes::MouseDown },        { RIGHTMOST_BUTTON_PRESSED, 0, { 1, 1 }, MOUSE_MOVED } },
        {   { CsiMouseButtonCodes::Right, 0,                           { 2, 2 }, CsiActionCodes::MouseUp },          { 0, 0, { 1, 1 }, 0 } },

        {   { CsiMouseButtonCodes::Left,  0,                           { 2, 2 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED, 0, { 1, 1 }, 0 } },
        {   { CsiMouseButtonCodes::Right, 0,                           { 2, 2 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED, 0, { 1, 1 }, 0 } },
        {   { CsiMouseButtonCodes::Left, CsiMouseModifierCodes::Drag,  { 2, 3 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED, 0, { 1, 2 }, MOUSE_MOVED } },
        {   { CsiMouseButtonCodes::Left, CsiMouseModifierCodes::Drag,  { 3, 3 }, CsiActionCodes::MouseDown },        { FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED, 0, { 2, 2 }, MOUSE_MOVED } },
        {   { CsiMouseButtonCodes::Left, 0,                            { 3, 3 }, CsiActionCodes::MouseUp },          { RIGHTMOST_BUTTON_PRESSED, 0, { 2, 2 }, 0 } },
        {   { CsiMouseButtonCodes::Right, 0,                           { 3, 3 }, CsiActionCodes::MouseUp },          { 0, 0, { 2, 2 }, 0 } },
    };
    // clang-format on

    VerifySGRMouseData(testData);
}

void InputEngineTest::SGRMouseTest_Scroll()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    // NOTE: scrolling events do NOT send a mouse up event
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                             EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::ScrollForward, 0, { 1, 1 }, CsiActionCodes::MouseDown },        { SCROLL_DELTA_FORWARD,  0, { 0, 0 }, MOUSE_WHEELED } },
        {   { CsiMouseButtonCodes::ScrollBack,    0, { 1, 1 }, CsiActionCodes::MouseDown },        { SCROLL_DELTA_BACKWARD, 0, { 0, 0 }, MOUSE_WHEELED } },
    };
    // clang-format on
    VerifySGRMouseData(testData);
}

void InputEngineTest::SGRMouseTest_DoubleClick()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                     EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseDown },         { FROM_LEFT_1ST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseUp },           { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseDown },         { FROM_LEFT_1ST_BUTTON_PRESSED, 0, { 0, 0 }, DOUBLE_CLICK } },
        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseUp },           { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseDown },         { FROM_LEFT_1ST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Left, 0, { 1, 1 }, CsiActionCodes::MouseUp },           { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseDown },       { FROM_LEFT_2ND_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseUp },         { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseDown },       { FROM_LEFT_2ND_BUTTON_PRESSED, 0, { 0, 0 }, DOUBLE_CLICK } },
        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseUp },         { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseDown },       { FROM_LEFT_2ND_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Middle, 0, { 1, 1 }, CsiActionCodes::MouseUp },         { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseDown },         { RIGHTMOST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseUp },           { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseDown },         { RIGHTMOST_BUTTON_PRESSED, 0, { 0, 0 }, DOUBLE_CLICK } },
        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseUp },           { 0, 0, { 0, 0 }, 0 } },

        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseDown },         { RIGHTMOST_BUTTON_PRESSED, 0, { 0, 0 }, 0 } },
        {   { CsiMouseButtonCodes::Right, 0, { 1, 1 }, CsiActionCodes::MouseUp },           { 0, 0, { 0, 0 }, 0 } },
    };
    // clang-format on

    VerifySGRMouseData(testData);
}

void InputEngineTest::SGRMouseTest_Hover()
{
    // SGR_PARAMS serves as test input
    // - the state of the buttons (constructed via InputStateMachineEngine::CsiMouseButtonCodes)
    // - the modifiers for the mouse event (constructed via InputStateMachineEngine::CsiMouseModifierCodes)
    // - the {x,y} position of the event on the viewport where the top-left is {1,1}
    // - the direction of the mouse press (constructed via InputStateMachineEngine::CsiActionCodes)

    // MOUSE_EVENT_PARAMS serves as expected output
    // - buttonState
    // - controlKeyState
    // - mousePosition
    // - eventFlags

    // clang-format off
    const std::vector<std::tuple<SGR_PARAMS, MOUSE_EVENT_PARAMS>> testData = {
        //  TEST INPUT                                                                                                 EXPECTED OUTPUT
        {   { CsiMouseButtonCodes::Released, CsiMouseModifierCodes::Drag, { 1, 1 }, CsiActionCodes::MouseUp },         { 0, 0, { 0, 0 }, MOUSE_MOVED } },
        {   { CsiMouseButtonCodes::Released, CsiMouseModifierCodes::Drag, { 2, 2 }, CsiActionCodes::MouseUp },         { 0, 0, { 1, 1 }, MOUSE_MOVED } },
    };
    // clang-format on

    VerifySGRMouseData(testData);
}

void InputEngineTest::CtrlAltZCtrlAltXTest()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);

    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto inputEngine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    auto _stateMachine = std::make_unique<StateMachine>(std::move(inputEngine));
    VERIFY_IS_NOT_NULL(_stateMachine);
    testState._stateMachine = _stateMachine.get();

    // This is a test for GH#4201. See that issue for more details.
    Log::Comment(L"Test Ctrl+Alt+Z and Ctrl+Alt+X, which execute from anywhere "
                 L"in the output engine, but should be Escape-Executed in the "
                 L"input engine.");

    DisableVerifyExceptions disable;

    {
        auto inputSeq = L"\x1b\x1a"; // ^[^Z

        auto expectedWch = L'Z';
        auto keyscan = OneCoreSafeVkKeyScanW(expectedWch);
        short vkey = keyscan & 0xff;
        auto scanCode = (WORD)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        INPUT_RECORD inputRec;

        inputRec.EventType = KEY_EVENT;
        inputRec.Event.KeyEvent.bKeyDown = TRUE;
        inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED;
        inputRec.Event.KeyEvent.wRepeatCount = 1;
        inputRec.Event.KeyEvent.wVirtualKeyCode = vkey;
        inputRec.Event.KeyEvent.wVirtualScanCode = scanCode;
        inputRec.Event.KeyEvent.uChar.UnicodeChar = expectedWch - 0x40;

        testState.vExpectedInput.push_back(inputRec);

        _stateMachine->ProcessString(inputSeq);
    }
    {
        auto inputSeq = L"\x1b\x18"; // ^[^X

        auto expectedWch = L'X';
        auto keyscan = OneCoreSafeVkKeyScanW(expectedWch);
        short vkey = keyscan & 0xff;
        auto scanCode = (WORD)OneCoreSafeMapVirtualKeyW(vkey, MAPVK_VK_TO_VSC);

        INPUT_RECORD inputRec;

        inputRec.EventType = KEY_EVENT;
        inputRec.Event.KeyEvent.bKeyDown = TRUE;
        inputRec.Event.KeyEvent.dwControlKeyState = LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED;
        inputRec.Event.KeyEvent.wRepeatCount = 1;
        inputRec.Event.KeyEvent.wVirtualKeyCode = vkey;
        inputRec.Event.KeyEvent.wVirtualScanCode = scanCode;
        inputRec.Event.KeyEvent.uChar.UnicodeChar = expectedWch - 0x40;

        testState.vExpectedInput.push_back(inputRec);

        _stateMachine->ProcessString(inputSeq);
    }

    VerifyExpectedInputDrained();
}

void InputEngineTest::TestSs3Entry()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto engine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    StateMachine mach(std::move(engine));

    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    mach.ProcessCharacter(AsciiChars::ESC);
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
    mach.ProcessCharacter(L'O');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
    mach.ProcessCharacter(L'm');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
}

void InputEngineTest::TestSs3Immediate()
{
    // Intermediates aren't supported by Ss3 - they just get dispatched
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto engine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    StateMachine mach(std::move(engine));

    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    mach.ProcessCharacter(AsciiChars::ESC);
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
    mach.ProcessCharacter(L'O');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
    mach.ProcessCharacter(L'$');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

    mach.ProcessCharacter(AsciiChars::ESC);
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
    mach.ProcessCharacter(L'O');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
    mach.ProcessCharacter(L'#');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

    mach.ProcessCharacter(AsciiChars::ESC);
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
    mach.ProcessCharacter(L'O');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
    mach.ProcessCharacter(L'%');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);

    mach.ProcessCharacter(AsciiChars::ESC);
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
    mach.ProcessCharacter(L'O');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
    mach.ProcessCharacter(L'?');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
}

void InputEngineTest::TestSs3Param()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto engine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));
    StateMachine mach(std::move(engine));

    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
    mach.ProcessCharacter(AsciiChars::ESC);
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Escape);
    mach.ProcessCharacter(L'O');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Entry);
    mach.ProcessCharacter(L';');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
    mach.ProcessCharacter(L'3');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
    mach.ProcessCharacter(L'2');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
    mach.ProcessCharacter(L'4');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
    mach.ProcessCharacter(L';');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
    mach.ProcessCharacter(L';');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
    mach.ProcessCharacter(L'8');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ss3Param);
    mach.ProcessCharacter(L'J');
    VERIFY_ARE_EQUAL(mach._state, StateMachine::VTStates::Ground);
}

void InputEngineTest::TestWin32InputParsing()
{
    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto engine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));

    {
        std::vector<VTParameter> params{ 1 };
        auto key = engine->_GenerateWin32Key({ params.data(), params.size() });
        VERIFY_ARE_EQUAL(1, key.GetVirtualKeyCode());
        VERIFY_ARE_EQUAL(0, key.GetVirtualScanCode());
        VERIFY_ARE_EQUAL(L'\0', key.GetCharData());
        VERIFY_ARE_EQUAL(false, key.IsKeyDown());
        VERIFY_ARE_EQUAL(0u, key.GetActiveModifierKeys());
        VERIFY_ARE_EQUAL(1, key.GetRepeatCount());
    }
    {
        std::vector<VTParameter> params{ 1, 2 };
        auto key = engine->_GenerateWin32Key({ params.data(), params.size() });
        VERIFY_ARE_EQUAL(1, key.GetVirtualKeyCode());
        VERIFY_ARE_EQUAL(2, key.GetVirtualScanCode());
        VERIFY_ARE_EQUAL(L'\0', key.GetCharData());
        VERIFY_ARE_EQUAL(false, key.IsKeyDown());
        VERIFY_ARE_EQUAL(0u, key.GetActiveModifierKeys());
        VERIFY_ARE_EQUAL(1, key.GetRepeatCount());
    }
    {
        std::vector<VTParameter> params{ 1, 2, 3 };
        auto key = engine->_GenerateWin32Key({ params.data(), params.size() });
        VERIFY_ARE_EQUAL(1, key.GetVirtualKeyCode());
        VERIFY_ARE_EQUAL(2, key.GetVirtualScanCode());
        VERIFY_ARE_EQUAL(L'\x03', key.GetCharData());
        VERIFY_ARE_EQUAL(false, key.IsKeyDown());
        VERIFY_ARE_EQUAL(0u, key.GetActiveModifierKeys());
        VERIFY_ARE_EQUAL(1, key.GetRepeatCount());
    }
    {
        std::vector<VTParameter> params{ 1, 2, 3, 4 };
        auto key = engine->_GenerateWin32Key({ params.data(), params.size() });
        VERIFY_ARE_EQUAL(1, key.GetVirtualKeyCode());
        VERIFY_ARE_EQUAL(2, key.GetVirtualScanCode());
        VERIFY_ARE_EQUAL(L'\x03', key.GetCharData());
        VERIFY_ARE_EQUAL(true, key.IsKeyDown());
        VERIFY_ARE_EQUAL(0u, key.GetActiveModifierKeys());
        VERIFY_ARE_EQUAL(1, key.GetRepeatCount());
    }
    {
        std::vector<VTParameter> params{ 1, 2, 3, 1 };
        auto key = engine->_GenerateWin32Key({ params.data(), params.size() });
        VERIFY_ARE_EQUAL(1, key.GetVirtualKeyCode());
        VERIFY_ARE_EQUAL(2, key.GetVirtualScanCode());
        VERIFY_ARE_EQUAL(L'\x03', key.GetCharData());
        VERIFY_ARE_EQUAL(true, key.IsKeyDown());
        VERIFY_ARE_EQUAL(0u, key.GetActiveModifierKeys());
        VERIFY_ARE_EQUAL(1, key.GetRepeatCount());
    }
    {
        std::vector<VTParameter> params{ 1, 2, 3, 4, 5 };
        auto key = engine->_GenerateWin32Key({ params.data(), params.size() });
        VERIFY_ARE_EQUAL(1, key.GetVirtualKeyCode());
        VERIFY_ARE_EQUAL(2, key.GetVirtualScanCode());
        VERIFY_ARE_EQUAL(L'\x03', key.GetCharData());
        VERIFY_ARE_EQUAL(true, key.IsKeyDown());
        VERIFY_ARE_EQUAL(0x5u, key.GetActiveModifierKeys());
        VERIFY_ARE_EQUAL(1, key.GetRepeatCount());
    }
    {
        std::vector<VTParameter> params{ 1, 2, 3, 4, 5, 6 };
        auto key = engine->_GenerateWin32Key({ params.data(), params.size() });
        VERIFY_ARE_EQUAL(1, key.GetVirtualKeyCode());
        VERIFY_ARE_EQUAL(2, key.GetVirtualScanCode());
        VERIFY_ARE_EQUAL(L'\x03', key.GetCharData());
        VERIFY_ARE_EQUAL(true, key.IsKeyDown());
        VERIFY_ARE_EQUAL(0x5u, key.GetActiveModifierKeys());
        VERIFY_ARE_EQUAL(6, key.GetRepeatCount());
    }
}

void InputEngineTest::TestWin32InputOptionals()
{
    // Send a bunch of possible sets of parameters, to see if they all parse correctly.

    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:provideVirtualKeyCode", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:provideVirtualScanCode", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:provideCharData", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:provideKeyDown", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:provideActiveModifierKeys", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:provideRepeatCount", L"{false, true}")
        TEST_METHOD_PROPERTY(L"Data:numParams", L"{0, 1, 2, 3, 4, 5, 6}")
    END_TEST_METHOD_PROPERTIES();

    INIT_TEST_PROPERTY(bool, provideVirtualKeyCode, L"If true, pass the VirtualKeyCode param in the list of params. Otherwise, leave it as the default param value (0)");
    INIT_TEST_PROPERTY(bool, provideVirtualScanCode, L"If true, pass the VirtualScanCode param in the list of params. Otherwise, leave it as the default param value (0)");
    INIT_TEST_PROPERTY(bool, provideCharData, L"If true, pass the CharData param in the list of params. Otherwise, leave it as the default param value (0)");
    INIT_TEST_PROPERTY(bool, provideKeyDown, L"If true, pass the KeyDown param in the list of params. Otherwise, leave it as the default param value (0)");
    INIT_TEST_PROPERTY(bool, provideActiveModifierKeys, L"If true, pass the ActiveModifierKeys param in the list of params. Otherwise, leave it as the default param value (0)");
    INIT_TEST_PROPERTY(bool, provideRepeatCount, L"If true, pass the RepeatCount param in the list of params. Otherwise, leave it as the default param value (0)");
    INIT_TEST_PROPERTY(size_t, numParams, L"Control how many of the params we send");

    auto pfn = std::bind(&TestState::TestInputCallback, &testState, std::placeholders::_1);
    auto dispatch = std::make_unique<TestInteractDispatch>(pfn, &testState);
    auto engine = std::make_unique<InputStateMachineEngine>(std::move(dispatch));

    {
        std::vector<VTParameter> params{
            provideVirtualKeyCode ? 1 : 0,
            provideVirtualScanCode ? 2 : 0,
            provideCharData ? 3 : 0,
            provideKeyDown ? 4 : 0,
            provideActiveModifierKeys ? 5 : 0,
            provideRepeatCount ? 6 : 0,
        };

        auto key = engine->_GenerateWin32Key({ params.data(), numParams });
        VERIFY_ARE_EQUAL((provideVirtualKeyCode && numParams > 0) ? 1 : 0,
                         key.GetVirtualKeyCode());
        VERIFY_ARE_EQUAL((provideVirtualScanCode && numParams > 1) ? 2 : 0,
                         key.GetVirtualScanCode());
        VERIFY_ARE_EQUAL((provideCharData && numParams > 2) ? L'\x03' : L'\0',
                         key.GetCharData());
        VERIFY_ARE_EQUAL((provideKeyDown && numParams > 3) ? true : false,
                         key.IsKeyDown());
        VERIFY_ARE_EQUAL((provideActiveModifierKeys && numParams > 4) ? 5u : 0u,
                         key.GetActiveModifierKeys());
        if (numParams == 6)
        {
            VERIFY_ARE_EQUAL((provideRepeatCount) ? 6 : 0,
                             key.GetRepeatCount());
        }
        else
        {
            VERIFY_ARE_EQUAL((provideRepeatCount && numParams > 5) ? 6 : 1,
                             key.GetRepeatCount());
        }
    }
}
