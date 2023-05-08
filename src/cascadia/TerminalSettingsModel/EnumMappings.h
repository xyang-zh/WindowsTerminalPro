/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- EnumMappings.h

Abstract:
- Contains mappings from enum name to enum value for the enum types used in our settings.
  These are mainly used in the settings UI for data binding so that we can display
  all possible choices in the UI for each setting/enum.

Author(s):
- Leon Liang - October 2020

--*/
#pragma once

#include "EnumMappings.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct EnumMappings : EnumMappingsT<EnumMappings>
    {
    public:
        EnumMappings() = default;

        // Global Settings
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Windows::UI::Xaml::ElementTheme> ElementTheme();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, NewTabPosition> NewTabPosition();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode> TabViewWidthMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, FirstWindowPreference> FirstWindowPreference();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, LaunchMode> LaunchMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, TabSwitcherMode> TabSwitcherMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::Terminal::Control::CopyFormat> CopyFormat();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, WindowingMode> WindowingMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::Terminal::Core::MatchMode> MatchMode();

        // Profile Settings
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, CloseOnExitMode> CloseOnExitMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Microsoft::Terminal::Control::ScrollbarState> ScrollbarState();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Windows::UI::Xaml::Media::Stretch> BackgroundImageStretchMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::Terminal::Control::TextAntialiasingMode> TextAntialiasingMode();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::Terminal::Core::CursorStyle> CursorStyle();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, uint16_t> FontWeight();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::Terminal::Settings::Model::IntenseStyle> IntenseTextStyle();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Microsoft::Terminal::Core::AdjustTextMode> AdjustIndistinguishableColors();

        // Serial Settings
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Microsoft::Terminal::Control::SerialBoundRate> SerialBoundRate();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Microsoft::Terminal::Control::SerialDataWidth> SerialDataWidth();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Microsoft::Terminal::Control::SerialCheckBit> SerialCheckBit();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Microsoft::Terminal::Control::SerialStopBit> SerialStopBit();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Microsoft::Terminal::Control::SerialFlowControl> SerialFlowControl();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(EnumMappings);
}
