/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- screenInfoUiaProvider.hpp

Abstract:
- This module provides UI Automation access to the screen buffer to
  support both automation tests and accessibility (screen reading)
  applications.
- This is the ConHost extension of ScreenInfoUiaProviderBase.hpp
- Based on examples, sample code, and guidance from
  https://msdn.microsoft.com/en-us/library/windows/desktop/ee671596(v=vs.85).aspx

Author(s):
- Carlos Zamora   (CaZamor)   2019
--*/

#pragma once

#include <UIAutomationCore.h>

#include "../types/ScreenInfoUiaProviderBase.h"
#include "../types/UiaTextRangeBase.hpp"
#include "uiaTextRange.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class WindowUiaProvider;

    class ScreenInfoUiaProvider final : public Microsoft::Console::Types::ScreenInfoUiaProviderBase
    {
    public:
        ScreenInfoUiaProvider() = default;
        HRESULT RuntimeClassInitialize(_In_ Render::IRenderData* pData,
                                       _In_ WindowUiaProvider* const pUiaParent);

        // IRawElementProviderFragment methods
        IFACEMETHODIMP Navigate(_In_ NavigateDirection direction,
                                _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) override;
        IFACEMETHODIMP get_BoundingRectangle(_Out_ UiaRect* pRect) override;
        IFACEMETHODIMP get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider) override;

        HWND GetWindowHandle() const;
        void ChangeViewport(const til::inclusive_rect& NewWindow) override;

    protected:
        HRESULT GetSelectionRange(_In_ IRawElementProviderSimple* pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // degenerate range
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // degenerate range at cursor position
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                const Cursor& cursor,
                                const std::wstring_view wordDelimiters,
                                _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // specific endpoint range
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                const til::point start,
                                const til::point end,
                                const std::wstring_view wordDelimiters,
                                _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

        // range from a UiaPoint
        HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                const UiaPoint point,
                                const std::wstring_view wordDelimiters,
                                _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr) override;

    private:
        // weak reference to uia parent
        WindowUiaProvider* _pUiaParent;
    };
}
