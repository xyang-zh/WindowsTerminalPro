// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "_stream.h"
#include "stream.h"

#include "dbcs.h"
#include "handle.h"
#include "misc.h"
#include "readDataRaw.hpp"

#include "ApiRoutines.h"

#include "../types/inc/GlyphWidth.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using Microsoft::Console::Interactivity::ServiceLocator;

// Routine Description:
// - This routine is used in stream input.  It gets input and filters it for unicode characters.
// Arguments:
// - pInputBuffer - The InputBuffer to read from
// - pwchOut - On a successful read, the char data read
// - Wait - true if a waited read should be performed
// - pCommandLineEditingKeys - if present, arrow keys will be
// returned. on output, if true, pwchOut contains virtual key code for
// arrow key.
// - pPopupKeys - if present, arrow keys will be
// returned. on output, if true, pwchOut contains virtual key code for
// arrow key.
// Return Value:
// - STATUS_SUCCESS on success or a relevant error code on failure.
[[nodiscard]] NTSTATUS GetChar(_Inout_ InputBuffer* const pInputBuffer,
                               _Out_ wchar_t* const pwchOut,
                               const bool Wait,
                               _Out_opt_ bool* const pCommandLineEditingKeys,
                               _Out_opt_ bool* const pPopupKeys,
                               _Out_opt_ DWORD* const pdwKeyState) noexcept
{
    if (nullptr != pCommandLineEditingKeys)
    {
        *pCommandLineEditingKeys = false;
    }

    if (nullptr != pPopupKeys)
    {
        *pPopupKeys = false;
    }

    if (nullptr != pdwKeyState)
    {
        *pdwKeyState = 0;
    }

    NTSTATUS Status;
    for (;;)
    {
        std::unique_ptr<IInputEvent> inputEvent;
        Status = pInputBuffer->Read(inputEvent,
                                    false, // peek
                                    Wait,
                                    true, // unicode
                                    true); // stream

        if (!NT_SUCCESS(Status))
        {
            return Status;
        }
        else if (inputEvent.get() == nullptr)
        {
            FAIL_FAST_IF(Wait);
            return STATUS_UNSUCCESSFUL;
        }

        if (inputEvent->EventType() == InputEventType::KeyEvent)
        {
            auto keyEvent = std::unique_ptr<KeyEvent>(static_cast<KeyEvent*>(inputEvent.release()));

            auto commandLineEditKey = false;
            if (pCommandLineEditingKeys)
            {
                commandLineEditKey = keyEvent->IsCommandLineEditingKey();
            }
            else if (pPopupKeys)
            {
                commandLineEditKey = keyEvent->IsPopupKey();
            }

            if (pdwKeyState)
            {
                *pdwKeyState = keyEvent->GetActiveModifierKeys();
            }

            if (keyEvent->GetCharData() != 0 && !commandLineEditKey)
            {
                // chars that are generated using alt + numpad
                if (!keyEvent->IsKeyDown() && keyEvent->GetVirtualKeyCode() == VK_MENU)
                {
                    if (keyEvent->IsAltNumpadSet())
                    {
                        if (HIBYTE(keyEvent->GetCharData()))
                        {
                            char chT[2] = {
                                static_cast<char>(HIBYTE(keyEvent->GetCharData())),
                                static_cast<char>(LOBYTE(keyEvent->GetCharData())),
                            };
                            *pwchOut = CharToWchar(chT, 2);
                        }
                        else
                        {
                            // Because USER doesn't know our codepage,
                            // it gives us the raw OEM char and we
                            // convert it to a Unicode character.
                            char chT = LOBYTE(keyEvent->GetCharData());
                            *pwchOut = CharToWchar(&chT, 1);
                        }
                    }
                    else
                    {
                        *pwchOut = keyEvent->GetCharData();
                    }
                    return STATUS_SUCCESS;
                }
                // Ignore Escape and Newline chars
                else if (keyEvent->IsKeyDown() &&
                         (WI_IsFlagSet(pInputBuffer->InputMode, ENABLE_VIRTUAL_TERMINAL_INPUT) ||
                          (keyEvent->GetVirtualKeyCode() != VK_ESCAPE &&
                           keyEvent->GetCharData() != UNICODE_LINEFEED)))
                {
                    *pwchOut = keyEvent->GetCharData();
                    return STATUS_SUCCESS;
                }
            }

            if (keyEvent->IsKeyDown())
            {
                if (pCommandLineEditingKeys && commandLineEditKey)
                {
                    *pCommandLineEditingKeys = true;
                    *pwchOut = static_cast<wchar_t>(keyEvent->GetVirtualKeyCode());
                    return STATUS_SUCCESS;
                }
                else if (pPopupKeys && commandLineEditKey)
                {
                    *pPopupKeys = true;
                    *pwchOut = static_cast<char>(keyEvent->GetVirtualKeyCode());
                    return STATUS_SUCCESS;
                }
                else
                {
                    const auto zeroVkeyData = OneCoreSafeVkKeyScanW(0);
                    const auto zeroVKey = LOBYTE(zeroVkeyData);
                    const auto zeroControlKeyState = HIBYTE(zeroVkeyData);

                    try
                    {
                        // Convert real Windows NT modifier bit into bizarre Console bits
                        auto consoleModKeyState = FromVkKeyScan(zeroControlKeyState);

                        if (zeroVKey == keyEvent->GetVirtualKeyCode() &&
                            keyEvent->DoActiveModifierKeysMatch(consoleModKeyState))
                        {
                            // This really is the character 0x0000
                            *pwchOut = keyEvent->GetCharData();
                            return STATUS_SUCCESS;
                        }
                    }
                    catch (...)
                    {
                        LOG_HR(wil::ResultFromCaughtException());
                    }
                }
            }
        }
    }
}

// Routine Description:
// - This routine returns the total number of screen spaces the characters up to the specified character take up.
til::CoordType RetrieveTotalNumberOfSpaces(const til::CoordType sOriginalCursorPositionX,
                                           _In_reads_(ulCurrentPosition) const WCHAR* const pwchBuffer,
                                           _In_ size_t ulCurrentPosition)
{
    auto XPosition = sOriginalCursorPositionX;
    til::CoordType NumSpaces = 0;

    for (size_t i = 0; i < ulCurrentPosition; i++)
    {
        const auto Char = pwchBuffer[i];

        til::CoordType NumSpacesForChar;
        if (Char == UNICODE_TAB)
        {
            NumSpacesForChar = NUMBER_OF_SPACES_IN_TAB(XPosition);
        }
        else if (IS_CONTROL_CHAR(Char))
        {
            NumSpacesForChar = 2;
        }
        else if (IsGlyphFullWidth(Char))
        {
            NumSpacesForChar = 2;
        }
        else
        {
            NumSpacesForChar = 1;
        }
        XPosition += NumSpacesForChar;
        NumSpaces += NumSpacesForChar;
    }

    return NumSpaces;
}

// Routine Description:
// - This routine returns the number of screen spaces the specified character takes up.
til::CoordType RetrieveNumberOfSpaces(_In_ til::CoordType sOriginalCursorPositionX,
                                      _In_reads_(ulCurrentPosition + 1) const WCHAR* const pwchBuffer,
                                      _In_ size_t ulCurrentPosition)
{
    auto Char = pwchBuffer[ulCurrentPosition];
    if (Char == UNICODE_TAB)
    {
        til::CoordType NumSpaces = 0;
        auto XPosition = sOriginalCursorPositionX;

        for (size_t i = 0; i <= ulCurrentPosition; i++)
        {
            Char = pwchBuffer[i];
            if (Char == UNICODE_TAB)
            {
                NumSpaces = NUMBER_OF_SPACES_IN_TAB(XPosition);
            }
            else if (IS_CONTROL_CHAR(Char))
            {
                NumSpaces = 2;
            }
            else if (IsGlyphFullWidth(Char))
            {
                NumSpaces = 2;
            }
            else
            {
                NumSpaces = 1;
            }
            XPosition += NumSpaces;
        }

        return NumSpaces;
    }
    else if (IS_CONTROL_CHAR(Char))
    {
        return 2;
    }
    else if (IsGlyphFullWidth(Char))
    {
        return 2;
    }
    else
    {
        return 1;
    }
}

// Routine Description:
// - if we have leftover input, copy as much fits into the user's
// buffer and return.  we may have multi line input, if a macro
// has been defined that contains the $T character.
// Arguments:
// - inputBuffer - Pointer to input buffer to read from.
// - buffer - buffer to place read char data into
// - bytesRead - number of bytes read and filled into the buffer
// - readHandleState - input read handle data associated with this read operation
// - unicode - true if read should be unicode, false otherwise
// Return Value:
// - STATUS_NO_MEMORY in low memory situation
// - other relevant NTSTATUS codes
[[nodiscard]] static NTSTATUS _ReadPendingInput(InputBuffer& inputBuffer,
                                                std::span<char> buffer,
                                                size_t& bytesRead,
                                                INPUT_READ_HANDLE_DATA& readHandleState,
                                                const bool unicode)
{
    // TODO: MSFT: 18047766 - Correct this method to not play byte counting games.
    auto fAddDbcsLead = FALSE;
    size_t NumToWrite = 0;
    size_t NumToBytes = 0;
    auto pBuffer = reinterpret_cast<wchar_t*>(buffer.data());
    auto bufferRemaining = buffer.size_bytes();
    bytesRead = 0;

    if (buffer.size_bytes() < sizeof(wchar_t))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    const auto pending = readHandleState.GetPendingInput();
    auto pendingBytes = pending.size() * sizeof(wchar_t);
    auto Tmp = pending.cbegin();

    if (readHandleState.IsMultilineInput())
    {
        if (!unicode)
        {
            if (inputBuffer.IsReadPartialByteSequenceAvailable())
            {
                auto event = inputBuffer.FetchReadPartialByteSequence(false);
                const auto pKeyEvent = static_cast<const KeyEvent* const>(event.get());
                *pBuffer = static_cast<char>(pKeyEvent->GetCharData());
                ++pBuffer;
                bufferRemaining -= sizeof(wchar_t);
                pendingBytes -= sizeof(wchar_t);
                fAddDbcsLead = TRUE;
            }

            if (pendingBytes == 0 || bufferRemaining == 0)
            {
                readHandleState.CompletePending();
                bytesRead = 1;
                return STATUS_SUCCESS;
            }
            else
            {
                for (NumToWrite = 0, Tmp = pending.cbegin(), NumToBytes = 0;
                     NumToBytes < pendingBytes &&
                     NumToBytes < bufferRemaining / sizeof(wchar_t) &&
                     *Tmp != UNICODE_LINEFEED;
                     Tmp++, NumToWrite += sizeof(wchar_t))
                {
                    NumToBytes += IsGlyphFullWidth(*Tmp) ? 2 : 1;
                }
            }
        }

        NumToWrite = 0;
        Tmp = pending.cbegin();
        while (NumToWrite < pendingBytes &&
               *Tmp != UNICODE_LINEFEED)
        {
            ++Tmp;
            NumToWrite += sizeof(wchar_t);
        }

        NumToWrite += sizeof(wchar_t);
        if (NumToWrite > bufferRemaining)
        {
            NumToWrite = bufferRemaining;
        }
    }
    else
    {
        if (!unicode)
        {
            if (inputBuffer.IsReadPartialByteSequenceAvailable())
            {
                auto event = inputBuffer.FetchReadPartialByteSequence(false);
                const auto pKeyEvent = static_cast<const KeyEvent* const>(event.get());
                *pBuffer = static_cast<char>(pKeyEvent->GetCharData());
                ++pBuffer;
                bufferRemaining -= sizeof(wchar_t);
                pendingBytes -= sizeof(wchar_t);
                fAddDbcsLead = TRUE;
            }

            if (pendingBytes == 0)
            {
                readHandleState.CompletePending();
                bytesRead = 1;
                return STATUS_SUCCESS;
            }
            else
            {
                for (NumToWrite = 0, Tmp = pending.cbegin(), NumToBytes = 0;
                     NumToBytes < pendingBytes && NumToBytes < bufferRemaining / sizeof(wchar_t);
                     Tmp++, NumToWrite += sizeof(wchar_t))
                {
                    NumToBytes += IsGlyphFullWidth(*Tmp) ? 2 : 1;
                }
            }
        }

        NumToWrite = (bufferRemaining < pendingBytes) ? bufferRemaining : pendingBytes;
    }

    memmove(pBuffer, pending.data(), NumToWrite);
    pendingBytes -= NumToWrite;
    if (pendingBytes != 0)
    {
        std::wstring_view remainingPending{ pending.data() + (NumToWrite / sizeof(wchar_t)), pendingBytes / sizeof(wchar_t) };
        readHandleState.UpdatePending(remainingPending);
    }
    else
    {
        readHandleState.CompletePending();
    }

    if (!unicode)
    {
        // if ansi, translate string.  we allocated the capture buffer
        // large enough to handle the translated string.
        auto tempBuffer = std::make_unique<char[]>(NumToBytes);
        std::unique_ptr<IInputEvent> partialEvent;

        NumToWrite = TranslateUnicodeToOem(pBuffer,
                                           gsl::narrow<ULONG>(NumToWrite / sizeof(wchar_t)),
                                           tempBuffer.get(),
                                           gsl::narrow<ULONG>(NumToBytes),
                                           partialEvent);
        if (partialEvent.get())
        {
            inputBuffer.StoreReadPartialByteSequence(std::move(partialEvent));
        }

        // clang-format off
#pragma prefast(suppress: __WARNING_POTENTIAL_BUFFER_OVERFLOW_HIGH_PRIORITY, "This access is fine but prefast can't follow it, evidently")
        // clang-format on
        memmove(pBuffer, tempBuffer.get(), NumToWrite);

        if (fAddDbcsLead)
        {
            NumToWrite++;
        }
    }

    bytesRead = NumToWrite;
    return STATUS_SUCCESS;
}

// Routine Description:
// - read in characters until the buffer is full or return is read.
// since we may wait inside this loop, store all important variables
// in the read data structure.  if we do wait, a read data structure
// will be allocated from the heap and its pointer will be stored
// in the wait block.  the CookedReadData will be copied into the
// structure.  the data is freed when the read is completed.
// Arguments:
// - inputBuffer - input buffer to read data from
// - processData - process handle of process making read request
// - buffer - buffer to place read char data
// - bytesRead - on output, the number of bytes read into pwchBuffer
// - controlKeyState - set by a cooked read
// - initialData - text of initial data found in the read message
// - ctrlWakeupMask - used by COOKED_READ_DATA
// - readHandleState - input read handle data associated with this read operation
// - exeName - name of the exe requesting the read
// - unicode - true if read should be unicode, false otherwise
// - waiter - If a wait is necessary this will contain the wait
// object on output
// Return Value:
// - STATUS_UNSUCCESSFUL if not able to access current screen buffer
// - STATUS_NO_MEMORY in low memory situation
// - other relevant HRESULT codes
[[nodiscard]] static HRESULT _ReadLineInput(InputBuffer& inputBuffer,
                                            const HANDLE processData,
                                            std::span<char> buffer,
                                            size_t& bytesRead,
                                            DWORD& controlKeyState,
                                            const std::string_view initialData,
                                            const DWORD ctrlWakeupMask,
                                            INPUT_READ_HANDLE_DATA& readHandleState,
                                            const std::wstring_view exeName,
                                            const bool unicode,
                                            std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    RETURN_HR_IF(E_FAIL, !gci.HasActiveOutputBuffer());

    auto& screenInfo = gci.GetActiveOutputBuffer();

    try
    {
        auto cookedReadData = std::make_unique<COOKED_READ_DATA>(&inputBuffer, // pInputBuffer
                                                                 &readHandleState, // pInputReadHandleData
                                                                 screenInfo, // pScreenInfo
                                                                 buffer.size_bytes(), // UserBufferSize
                                                                 reinterpret_cast<wchar_t*>(buffer.data()), // UserBuffer
                                                                 ctrlWakeupMask, // CtrlWakeupMask
                                                                 exeName, // exe name
                                                                 initialData,
                                                                 reinterpret_cast<ConsoleProcessHandle*>(processData)); //pClientProcess

        gci.SetCookedReadData(cookedReadData.get());
        bytesRead = buffer.size_bytes(); // This parameter on the way in is the size to read, on the way out, it will be updated to what is actually read.
        if (CONSOLE_STATUS_WAIT == cookedReadData->Read(unicode, bytesRead, controlKeyState))
        {
            // memory will be cleaned up by wait queue
            waiter.reset(cookedReadData.release());
        }
        else
        {
            gci.SetCookedReadData(nullptr);
        }
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Character (raw) mode. Read at least one character in. After one
// character has been read, get any more available characters and
// return. The first call to GetChar may block. If we do wait, a read
// data structure will be allocated from the heap and its pointer will
// be stored in the wait block. The RawReadData will be copied into
// the structure. The data is freed when the read is completed.
// Arguments:
// - inputBuffer - input buffer to read data from
// - buffer - on output, the amount of data read, in bytes
// - bytesRead - number of bytes read and placed into buffer
// - readHandleState - input read handle data associated with this read operation
// - unicode - true if read should be unicode, false otherwise
// - waiter  - if a wait is necessary, on output this will contain
// the associated wait object
// Return Value:
// - CONSOLE_STATUS_WAIT if a wait is necessary. ppWaiter will be
// populated.
// - STATUS_SUCCESS on success
// - Other NTSTATUS codes as necessary
[[nodiscard]] static NTSTATUS _ReadCharacterInput(InputBuffer& inputBuffer,
                                                  std::span<char> buffer,
                                                  size_t& bytesRead,
                                                  INPUT_READ_HANDLE_DATA& readHandleState,
                                                  const bool unicode,
                                                  std::unique_ptr<IWaitRoutine>& waiter)
{
    size_t NumToWrite = 0;
    auto addDbcsLead = false;
    auto Status = STATUS_SUCCESS;
    auto pBuffer = reinterpret_cast<wchar_t*>(buffer.data());
    auto bufferRemaining = buffer.size_bytes();
    bytesRead = 0;

    if (buffer.size() < 1)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (bytesRead < bufferRemaining)
    {
        auto pwchBufferTmp = pBuffer;

        NumToWrite = 0;

        if (!unicode && inputBuffer.IsReadPartialByteSequenceAvailable())
        {
            auto event = inputBuffer.FetchReadPartialByteSequence(false);
            const auto pKeyEvent = static_cast<const KeyEvent* const>(event.get());
            *pBuffer = static_cast<char>(pKeyEvent->GetCharData());
            ++pBuffer;
            bufferRemaining -= sizeof(wchar_t);
            addDbcsLead = true;

            if (bufferRemaining == 0)
            {
                bytesRead = 1;
                return STATUS_SUCCESS;
            }
        }
        else
        {
            Status = GetChar(&inputBuffer,
                             pBuffer,
                             true,
                             nullptr,
                             nullptr,
                             nullptr);
        }

        if (Status == CONSOLE_STATUS_WAIT)
        {
            waiter = std::make_unique<RAW_READ_DATA>(&inputBuffer,
                                                     &readHandleState,
                                                     gsl::narrow<ULONG>(buffer.size_bytes()),
                                                     reinterpret_cast<wchar_t*>(buffer.data()));
        }

        if (!NT_SUCCESS(Status))
        {
            bytesRead = 0;
            return Status;
        }

        if (!addDbcsLead)
        {
            bytesRead += IsGlyphFullWidth(*pBuffer) ? 2 : 1;
            NumToWrite += sizeof(wchar_t);
            pBuffer++;
        }

        while (NumToWrite < static_cast<ULONG>(bufferRemaining))
        {
            Status = GetChar(&inputBuffer,
                             pBuffer,
                             false,
                             nullptr,
                             nullptr,
                             nullptr);
            if (!NT_SUCCESS(Status))
            {
                break;
            }
            bytesRead += IsGlyphFullWidth(*pBuffer) ? 2 : 1;
            NumToWrite += sizeof(wchar_t);
            pBuffer++;
        }

        // if ansi, translate string.  we allocated the capture buffer large enough to handle the translated string.
        if (!unicode)
        {
            std::unique_ptr<char[]> tempBuffer;
            try
            {
                tempBuffer = std::make_unique<char[]>(bytesRead);
            }
            catch (...)
            {
                return STATUS_NO_MEMORY;
            }

            pBuffer = pwchBufferTmp;
            std::unique_ptr<IInputEvent> partialEvent;

            bytesRead = TranslateUnicodeToOem(pBuffer,
                                              gsl::narrow<ULONG>(NumToWrite / sizeof(wchar_t)),
                                              tempBuffer.get(),
                                              gsl::narrow<ULONG>(bytesRead),
                                              partialEvent);

            if (partialEvent.get())
            {
                inputBuffer.StoreReadPartialByteSequence(std::move(partialEvent));
            }

#pragma prefast(suppress : 26053 26015, "PREfast claims read overflow. *pReadByteCount is the exact size of tempBuffer as allocated above.")
            memmove(pBuffer, tempBuffer.get(), bytesRead);

            if (addDbcsLead)
            {
                ++bytesRead;
            }
        }
        else
        {
            // We always return the byte count for A & W modes, so in
            // the Unicode case where we didn't translate back, set
            // the return to the byte count that we assembled while
            // pulling characters from the internal buffers.
            bytesRead = NumToWrite;
        }
    }
    return STATUS_SUCCESS;
}

// Routine Description:
// - This routine reads in characters for stream input and does the
// required processing based on the input mode (line, char, echo).
// - This routine returns UNICODE characters.
// Arguments:
// - inputBuffer - Pointer to input buffer to read from.
// - processData - process handle of process making read request
// - buffer - buffer to place read char data into
// - bytesRead - the length of data placed in buffer. Measured in bytes.
// - controlKeyState - set by a cooked read
// - initialData - text of initial data found in the read message
// - ctrlWakeupMask - used by COOKED_READ_DATA
// - readHandleState - read handle data associated with this read
// - exeName- name of the exe requesting the read
// - unicode - true for a unicode read, false for ascii
// - waiter - If a wait is necessary this will contain the wait
// object on output
// Return Value:
// - STATUS_BUFFER_TOO_SMALL if pdwNumBytes is too small to store char
// data.
// - CONSOLE_STATUS_WAIT if a wait is necessary. ppWaiter will be
// populated.
// - STATUS_SUCCESS on success
// - Other NSTATUS codes as necessary
[[nodiscard]] NTSTATUS DoReadConsole(InputBuffer& inputBuffer,
                                     const HANDLE processData,
                                     std::span<char> buffer,
                                     size_t& bytesRead,
                                     ULONG& controlKeyState,
                                     const std::string_view initialData,
                                     const DWORD ctrlWakeupMask,
                                     INPUT_READ_HANDLE_DATA& readHandleState,
                                     const std::wstring_view exeName,
                                     const bool unicode,
                                     std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        waiter.reset();

        bytesRead = 0;

        if (buffer.size() < 1)
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        if (readHandleState.IsInputPending())
        {
            return _ReadPendingInput(inputBuffer,
                                     buffer,
                                     bytesRead,
                                     readHandleState,
                                     unicode);
        }
        else if (WI_IsFlagSet(inputBuffer.InputMode, ENABLE_LINE_INPUT))
        {
            return NTSTATUS_FROM_HRESULT(_ReadLineInput(inputBuffer,
                                                        processData,
                                                        buffer,
                                                        bytesRead,
                                                        controlKeyState,
                                                        initialData,
                                                        ctrlWakeupMask,
                                                        readHandleState,
                                                        exeName,
                                                        unicode,
                                                        waiter));
        }
        else
        {
            return _ReadCharacterInput(inputBuffer,
                                       buffer,
                                       bytesRead,
                                       readHandleState,
                                       unicode,
                                       waiter);
        }
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleAImpl(IConsoleInputObject& context,
                                                    std::span<char> buffer,
                                                    size_t& written,
                                                    std::unique_ptr<IWaitRoutine>& waiter,
                                                    const std::string_view initialData,
                                                    const std::wstring_view exeName,
                                                    INPUT_READ_HANDLE_DATA& readHandleState,
                                                    const HANDLE clientHandle,
                                                    const DWORD controlWakeupMask,
                                                    DWORD& controlKeyState) noexcept
{
    try
    {
        return HRESULT_FROM_NT(DoReadConsole(context,
                                             clientHandle,
                                             buffer,
                                             written,
                                             controlKeyState,
                                             initialData,
                                             controlWakeupMask,
                                             readHandleState,
                                             exeName,
                                             false,
                                             waiter));
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleWImpl(IConsoleInputObject& context,
                                                    std::span<char> buffer,
                                                    size_t& written,
                                                    std::unique_ptr<IWaitRoutine>& waiter,
                                                    const std::string_view initialData,
                                                    const std::wstring_view exeName,
                                                    INPUT_READ_HANDLE_DATA& readHandleState,
                                                    const HANDLE clientHandle,
                                                    const DWORD controlWakeupMask,
                                                    DWORD& controlKeyState) noexcept
{
    try
    {
        return HRESULT_FROM_NT(DoReadConsole(context,
                                             clientHandle,
                                             buffer,
                                             written,
                                             controlKeyState,
                                             initialData,
                                             controlWakeupMask,
                                             readHandleState,
                                             exeName,
                                             true,
                                             waiter));
    }
    CATCH_RETURN();
}

void UnblockWriteConsole(const DWORD dwReason)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.Flags &= ~dwReason;

    if (WI_AreAllFlagsClear(gci.Flags, (CONSOLE_SUSPENDED | CONSOLE_SELECTING | CONSOLE_SCROLLBAR_TRACKING)))
    {
        // There is no longer any reason to suspend output, so unblock it.
        gci.OutputQueue.NotifyWaiters(true);
    }
}
