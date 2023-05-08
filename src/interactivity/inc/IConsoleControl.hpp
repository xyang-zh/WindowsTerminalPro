/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IConsoleControl.hpp

Abstract:
- Defines methods that delegate the execution of privileged operations or notify
  Windows subsystems about console state.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

namespace Microsoft::Console::Interactivity
{
    class IConsoleControl
    {
    public:
        virtual ~IConsoleControl() = default;
        [[nodiscard]] virtual NTSTATUS NotifyConsoleApplication(DWORD dwProcessId) = 0;
        [[nodiscard]] virtual NTSTATUS SetForeground(HANDLE hProcess, BOOL fForeground) = 0;
        [[nodiscard]] virtual NTSTATUS EndTask(DWORD dwProcessId, DWORD dwEventType, ULONG ulCtrlFlags) = 0;
        [[nodiscard]] virtual NTSTATUS SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId) = 0;
    };
}
