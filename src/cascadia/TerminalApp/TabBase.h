// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TabBase.g.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

namespace winrt::TerminalApp::implementation
{
    struct TabBase : TabBaseT<TabBase>
    {
    public:
        virtual void Focus(winrt::Windows::UI::Xaml::FocusState focusState) = 0;
        winrt::Windows::UI::Xaml::FocusState FocusState() const noexcept;

        virtual void Shutdown();
        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        void UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs);
        void SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap);
        virtual std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs> BuildStartupActions() const = 0;

        virtual std::optional<winrt::Windows::UI::Color> GetTabColor();
        void ThemeColor(const winrt::Microsoft::Terminal::Settings::Model::ThemeColor& focused,
                        const winrt::Microsoft::Terminal::Settings::Model::ThemeColor& unfocused,
                        const til::color& tabRowColor);

        WINRT_CALLBACK(RequestFocusActiveControl, winrt::delegate<void()>);

        WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
        WINRT_CALLBACK(CloseRequested, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        // The TabViewIndex is the index this Tab object resides in TerminalPage's _tabs vector.
        WINRT_PROPERTY(uint32_t, TabViewIndex, 0);
        // The TabViewNumTabs is the number of Tab objects in TerminalPage's _tabs vector.
        WINRT_PROPERTY(uint32_t, TabViewNumTabs, 0);

        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Title, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Icon, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, ReadOnly, _PropertyChangedHandlers, false);
        WINRT_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewItem, TabViewItem, nullptr);

        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Xaml::FrameworkElement, Content, _PropertyChangedHandlers, nullptr);

    protected:
        winrt::Windows::UI::Xaml::FocusState _focusState{ winrt::Windows::UI::Xaml::FocusState::Unfocused };
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeOtherTabsMenuItem{};
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeTabsAfterMenuItem{};
        winrt::TerminalApp::ShortcutActionDispatch _dispatch;
        Microsoft::Terminal::Settings::Model::IActionMapView _actionMap{ nullptr };
        winrt::hstring _keyChord{};

        winrt::Microsoft::Terminal::Settings::Model::ThemeColor _themeColor{ nullptr };
        winrt::Microsoft::Terminal::Settings::Model::ThemeColor _unfocusedThemeColor{ nullptr };
        til::color _tabRowColor;

        virtual void _CreateContextMenu();
        virtual winrt::hstring _CreateToolTipTitle();

        virtual void _MakeTabViewItem();

        void _AppendCloseMenuItems(winrt::Windows::UI::Xaml::Controls::MenuFlyout flyout);
        void _EnableCloseMenuItems();
        void _CloseTabsAfter();
        void _CloseOtherTabs();
        winrt::fire_and_forget _UpdateSwitchToTabKeyChord();
        void _UpdateToolTip();

        void _RecalculateAndApplyTabColor();
        void _ApplyTabColorOnUIThread(const winrt::Windows::UI::Color& color);
        void _ClearTabBackgroundColor();
        void _RefreshVisualState();
        virtual winrt::Windows::UI::Xaml::Media::Brush _BackgroundBrush() = 0;

        friend class ::TerminalAppLocalTests::TabTests;
    };
}
