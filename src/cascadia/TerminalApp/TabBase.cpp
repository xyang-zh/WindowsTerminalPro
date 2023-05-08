// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "TabBase.h"
#include "TabBase.g.cpp"
#include "Utils.h"
#include "ColorHelper.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::System;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace winrt::TerminalApp::implementation
{
    WUX::FocusState TabBase::FocusState() const noexcept
    {
        return _focusState;
    }

    // Method Description:
    // - Prepares this tab for being removed from the UI hierarchy
    void TabBase::Shutdown()
    {
        Content(nullptr);
        _ClosedHandlers(nullptr, nullptr);
    }

    // Method Description:
    // - Creates a context menu attached to the tab.
    // Currently contains elements allowing the user to close the selected tab
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_CreateContextMenu()
    {
        auto weakThis{ get_weak() };

        // Build the menu
        Controls::MenuFlyout contextMenuFlyout;
        // GH#5750 - When the context menu is dismissed with ESC, toss the focus
        // back to our control.
        contextMenuFlyout.Closed([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_RequestFocusActiveControlHandlers();
            }
        });
        _AppendCloseMenuItems(contextMenuFlyout);
        TabViewItem().ContextFlyout(contextMenuFlyout);
    }

    // Method Description:
    // - Append the close menu items to the context menu flyout
    // Arguments:
    // - flyout - the menu flyout to which the close items must be appended
    // Return Value:
    // - <none>
    void TabBase::_AppendCloseMenuItems(winrt::Windows::UI::Xaml::Controls::MenuFlyout flyout)
    {
        auto weakThis{ get_weak() };

        // Close tabs after
        _closeTabsAfterMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_CloseTabsAfter();
            }
        });
        _closeTabsAfterMenuItem.Text(RS_(L"TabCloseAfter"));
        const auto closeTabsAfterToolTip = RS_(L"TabCloseAfterToolTip");

        WUX::Controls::ToolTipService::SetToolTip(_closeTabsAfterMenuItem, box_value(closeTabsAfterToolTip));
        Automation::AutomationProperties::SetHelpText(_closeTabsAfterMenuItem, closeTabsAfterToolTip);

        // Close other tabs
        _closeOtherTabsMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_CloseOtherTabs();
            }
        });
        _closeOtherTabsMenuItem.Text(RS_(L"TabCloseOther"));
        const auto closeOtherTabsToolTip = RS_(L"TabCloseOtherToolTip");

        WUX::Controls::ToolTipService::SetToolTip(_closeOtherTabsMenuItem, box_value(closeOtherTabsToolTip));
        Automation::AutomationProperties::SetHelpText(_closeOtherTabsMenuItem, closeOtherTabsToolTip);

        // Close
        Controls::MenuFlyoutItem closeTabMenuItem;
        Controls::FontIcon closeSymbol;
        closeSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
        closeSymbol.Glyph(L"\xE711");

        closeTabMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_CloseRequestedHandlers(nullptr, nullptr);
            }
        });
        closeTabMenuItem.Text(RS_(L"TabClose"));
        closeTabMenuItem.Icon(closeSymbol);
        const auto closeTabToolTip = RS_(L"TabCloseToolTip");

        WUX::Controls::ToolTipService::SetToolTip(closeTabMenuItem, box_value(closeTabToolTip));
        Automation::AutomationProperties::SetHelpText(closeTabMenuItem, closeTabToolTip);

        // GH#8238 append the close menu items to the flyout itself until crash in XAML is fixed
        //Controls::MenuFlyoutSubItem closeSubMenu;
        //closeSubMenu.Text(RS_(L"TabCloseSubMenu"));
        //closeSubMenu.Items().Append(_closeTabsAfterMenuItem);
        //closeSubMenu.Items().Append(_closeOtherTabsMenuItem);
        //flyout.Items().Append(closeSubMenu);
        flyout.Items().Append(_closeTabsAfterMenuItem);
        flyout.Items().Append(_closeOtherTabsMenuItem);
        flyout.Items().Append(closeTabMenuItem);
    }

    // Method Description:
    // - Enable the Close menu items based on tab index and total number of tabs
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_EnableCloseMenuItems()
    {
        // close other tabs is enabled only if there are other tabs
        _closeOtherTabsMenuItem.IsEnabled(TabViewNumTabs() > 1);
        // close tabs after is enabled only if there are other tabs on the right
        _closeTabsAfterMenuItem.IsEnabled(TabViewIndex() < TabViewNumTabs() - 1);
    }

    void TabBase::_CloseTabsAfter()
    {
        CloseTabsAfterArgs args{ _TabViewIndex };
        ActionAndArgs closeTabsAfter{ ShortcutAction::CloseTabsAfter, args };

        _dispatch.DoAction(closeTabsAfter);
    }

    void TabBase::_CloseOtherTabs()
    {
        CloseOtherTabsArgs args{ _TabViewIndex };
        ActionAndArgs closeOtherTabs{ ShortcutAction::CloseOtherTabs, args };

        _dispatch.DoAction(closeOtherTabs);
    }

    void TabBase::UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs)
    {
        TabViewIndex(idx);
        TabViewNumTabs(numTabs);
        _EnableCloseMenuItems();
        _UpdateSwitchToTabKeyChord();
    }

    void TabBase::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        _dispatch = dispatch;
    }

    void TabBase::SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap)
    {
        _actionMap = actionMap;
        _UpdateSwitchToTabKeyChord();
    }

    // Method Description:
    // - Sets the key chord resulting in switch to the current tab.
    // Updates tool tip if required
    // Arguments:
    // - keyChord - string representation of the key chord that switches to the current tab
    // Return Value:
    // - <none>
    winrt::fire_and_forget TabBase::_UpdateSwitchToTabKeyChord()
    {
        const auto keyChord = _actionMap ? _actionMap.GetKeyBindingForAction(ShortcutAction::SwitchToTab, SwitchToTabArgs{ _TabViewIndex }) : nullptr;
        const auto keyChordText = keyChord ? KeyChordSerialization::ToString(keyChord) : L"";

        if (_keyChord == keyChordText)
        {
            co_return;
        }

        _keyChord = keyChordText;

        auto weakThis{ get_weak() };

        co_await wil::resume_foreground(TabViewItem().Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            _UpdateToolTip();
        }
    }

    // Method Description:
    // - Creates a text for the title run in the tool tip by returning tab title
    // Arguments:
    // - <none>
    // Return Value:
    // - The value to populate in the title run of the tool tip
    winrt::hstring TabBase::_CreateToolTipTitle()
    {
        return _Title;
    }

    // Method Description:
    // - Sets tab tool tip to a concatenation of title and key chord
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_UpdateToolTip()
    {
        auto titleRun = WUX::Documents::Run();
        titleRun.Text(_CreateToolTipTitle());

        auto textBlock = WUX::Controls::TextBlock{};
        textBlock.TextWrapping(WUX::TextWrapping::Wrap);
        textBlock.TextAlignment(WUX::TextAlignment::Center);
        textBlock.Inlines().Append(titleRun);

        if (!_keyChord.empty())
        {
            auto keyChordRun = WUX::Documents::Run();
            keyChordRun.Text(_keyChord);
            keyChordRun.FontStyle(winrt::Windows::UI::Text::FontStyle::Italic);
            textBlock.Inlines().Append(WUX::Documents::LineBreak{});
            textBlock.Inlines().Append(keyChordRun);
        }

        WUX::Controls::ToolTip toolTip{};
        toolTip.Content(textBlock);
        WUX::Controls::ToolTipService::SetToolTip(TabViewItem(), toolTip);
    }

    // Method Description:
    // - Initializes a TabViewItem for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_MakeTabViewItem()
    {
        TabViewItem(::winrt::MUX::Controls::TabViewItem{});

        // GH#3609 If the tab was tapped, and no one else was around to handle
        // it, then ask our parent to toss focus into the active control.
        TabViewItem().Tapped([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_RequestFocusActiveControlHandlers();
            }
        });
    }

    std::optional<winrt::Windows::UI::Color> TabBase::GetTabColor()
    {
        return std::nullopt;
    }

    void TabBase::ThemeColor(const winrt::Microsoft::Terminal::Settings::Model::ThemeColor& focused,
                             const winrt::Microsoft::Terminal::Settings::Model::ThemeColor& unfocused,
                             const til::color& tabRowColor)
    {
        _themeColor = focused;
        _unfocusedThemeColor = unfocused;
        _tabRowColor = tabRowColor;
        _RecalculateAndApplyTabColor();
    }

    // Method Description:
    // - This function dispatches a function to the UI thread to recalculate
    //   what this tab's current background color should be. If a color is set,
    //   it will apply the given color to the tab's background. Otherwise, it
    //   will clear the tab's background color.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_RecalculateAndApplyTabColor()
    {
        auto weakThis{ get_weak() };

        TabViewItem().Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [weakThis]() {
            auto ptrTab = weakThis.get();
            if (!ptrTab)
            {
                return;
            }

            auto tab{ ptrTab };

            // GetTabColor will return the color set by the color picker, or the
            // color specified in the profile. If neither of those were set,
            // then look to _themeColor to see if there's a value there.
            // Otherwise, clear our color, falling back to the TabView defaults.
            const auto currentColor = tab->GetTabColor();
            if (currentColor.has_value())
            {
                tab->_ApplyTabColorOnUIThread(currentColor.value());
            }
            else if (tab->_themeColor != nullptr)
            {
                // Safely get the active control's brush.
                const Media::Brush terminalBrush{ tab->_BackgroundBrush() };

                if (const auto themeBrush{ tab->_themeColor.Evaluate(Application::Current().Resources(), terminalBrush, false) })
                {
                    // ThemeColor.Evaluate will get us a Brush (because the
                    // TermControl could have an acrylic BG, for example). Take
                    // that brush, and get the color out of it. We don't really
                    // want to have the tab items themselves be acrylic.
                    tab->_ApplyTabColorOnUIThread(til::color{ ThemeColor::ColorFromBrush(themeBrush) });
                }
                else
                {
                    tab->_ClearTabBackgroundColor();
                }
            }
            else
            {
                tab->_ClearTabBackgroundColor();
            }
        });
    }

    // Method Description:
    // - Applies the given color to the background of this tab's TabViewItem.
    // - Sets the tab foreground color depending on the luminance of
    // the background color
    // - This method should only be called on the UI thread.
    // Arguments:
    // - color: the color the user picked for their tab
    // Return Value:
    // - <none>
    void TabBase::_ApplyTabColorOnUIThread(const winrt::Windows::UI::Color& color)
    {
        Media::SolidColorBrush selectedTabBrush{};
        Media::SolidColorBrush deselectedTabBrush{};
        Media::SolidColorBrush fontBrush{};
        Media::SolidColorBrush deselectedFontBrush{};
        Media::SolidColorBrush secondaryFontBrush{};
        Media::SolidColorBrush hoverTabBrush{};
        Media::SolidColorBrush subtleFillColorSecondaryBrush;
        Media::SolidColorBrush subtleFillColorTertiaryBrush;

        // calculate the luminance of the current color and select a font
        // color based on that
        // see https://www.w3.org/TR/WCAG20/#relativeluminancedef
        if (TerminalApp::ColorHelper::IsBrightColor(color))
        {
            auto subtleFillColorSecondary = winrt::Windows::UI::Colors::Black();
            subtleFillColorSecondary.A = 0x09;
            subtleFillColorSecondaryBrush.Color(subtleFillColorSecondary);
            auto subtleFillColorTertiary = winrt::Windows::UI::Colors::Black();
            subtleFillColorTertiary.A = 0x06;
            subtleFillColorTertiaryBrush.Color(subtleFillColorTertiary);
        }
        else
        {
            auto subtleFillColorSecondary = winrt::Windows::UI::Colors::White();
            subtleFillColorSecondary.A = 0x0F;
            subtleFillColorSecondaryBrush.Color(subtleFillColorSecondary);
            auto subtleFillColorTertiary = winrt::Windows::UI::Colors::White();
            subtleFillColorTertiary.A = 0x0A;
            subtleFillColorTertiaryBrush.Color(subtleFillColorTertiary);
        }

        // The tab font should be based on the evaluated appearance of the tab color layered on tab row.
        const auto layeredTabColor = til::color{ color }.layer_over(_tabRowColor);
        if (TerminalApp::ColorHelper::IsBrightColor(layeredTabColor))
        {
            fontBrush.Color(winrt::Windows::UI::Colors::Black());
            auto secondaryFontColor = winrt::Windows::UI::Colors::Black();
            // For alpha value see: https://github.com/microsoft/microsoft-ui-xaml/blob/7a33ad772d77d908aa6b316ec24e6d2eb3ebf571/dev/CommonStyles/Common_themeresources_any.xaml#L269
            secondaryFontColor.A = 0x9E;
            secondaryFontBrush.Color(secondaryFontColor);
        }
        else
        {
            fontBrush.Color(winrt::Windows::UI::Colors::White());
            auto secondaryFontColor = winrt::Windows::UI::Colors::White();
            // For alpha value see: https://github.com/microsoft/microsoft-ui-xaml/blob/7a33ad772d77d908aa6b316ec24e6d2eb3ebf571/dev/CommonStyles/Common_themeresources_any.xaml#L14
            secondaryFontColor.A = 0xC5;
            secondaryFontBrush.Color(secondaryFontColor);
        }

        selectedTabBrush.Color(color);

        // Start with the current tab color, set to Opacity=.3
        til::color deselectedTabColor{ color };
        deselectedTabColor = deselectedTabColor.with_alpha(77); // 255 * .3 = 77

        // If we DON'T have a color set from the color picker, or the profile's
        // tabColor, but we do have a unfocused color in the theme, use the
        // unfocused theme color here instead.
        if (!GetTabColor().has_value() &&
            _unfocusedThemeColor != nullptr)
        {
            // Safely get the active control's brush.
            const Media::Brush terminalBrush{ _BackgroundBrush() };

            // Get the color of the brush.
            if (const auto themeBrush{ _unfocusedThemeColor.Evaluate(Application::Current().Resources(), terminalBrush, false) })
            {
                // We did figure out the brush. Get the color out of it. If it
                // was "accent" or "terminalBackground", then we're gonna set
                // the alpha to .3 manually here.
                // (ThemeColor::UnfocusedTabOpacity will do this for us). If the
                // user sets both unfocused and focused tab.background to
                // terminalBackground, this will allow for some differentiation
                // (and is generally just sensible).
                deselectedTabColor = til::color{ ThemeColor::ColorFromBrush(themeBrush) }.with_alpha(_unfocusedThemeColor.UnfocusedTabOpacity());
            }
        }

        // currently if a tab has a custom color, a deselected state is
        // signified by using the same color with a bit of transparency
        deselectedTabBrush.Color(deselectedTabColor.with_alpha(255));
        deselectedTabBrush.Opacity(deselectedTabColor.a / 255.f);

        hoverTabBrush.Color(color);
        hoverTabBrush.Opacity(0.6);

        // Account for the color of the tab row when setting the color of text
        // on inactive tabs. Consider:
        // * black active tabs
        // * on a white tab row
        // * with a transparent inactive tab color
        //
        // We don't want that to result in white text on a white tab row for
        // inactive tabs.
        const auto deselectedActualColor = deselectedTabColor.layer_over(_tabRowColor);
        if (TerminalApp::ColorHelper::IsBrightColor(deselectedActualColor))
        {
            deselectedFontBrush.Color(winrt::Windows::UI::Colors::Black());
        }
        else
        {
            deselectedFontBrush.Color(winrt::Windows::UI::Colors::White());
        }

        // Prior to MUX 2.7, we set TabViewItemHeaderBackground, but now we can
        // use TabViewItem().Background() for that. HOWEVER,
        // TabViewItem().Background() only sets the color of the tab background
        // when the TabViewItem is unselected. So we still need to set the other
        // properties ourselves.
        //
        // In GH#11294 we thought we'd still need to set
        // TabViewItemHeaderBackground manually, but GH#11382 discovered that
        // Background() was actually okay after all.
        TabViewItem().Background(deselectedTabBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundSelected"), selectedTabBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundPointerOver"), hoverTabBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundPressed"), selectedTabBrush);

        // Similarly, TabViewItem().Foreground()  sets the color for the text
        // when the TabViewItem isn't selected, but not when it is hovered,
        // pressed, dragged, or selected, so we'll need to just set them all
        // anyways.
        TabViewItem().Foreground(deselectedFontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderForeground"), deselectedFontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundSelected"), fontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundPointerOver"), fontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundPressed"), fontBrush);

        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonForeground"), deselectedFontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonForegroundPressed"), secondaryFontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonForegroundPointerOver"), fontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderPressedCloseButtonForeground"), fontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderPointerOverCloseButtonForeground"), fontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderSelectedCloseButtonForeground"), fontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonBackgroundPressed"), subtleFillColorTertiaryBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonBackgroundPointerOver"), subtleFillColorSecondaryBrush);

        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewButtonForegroundActiveTab"), fontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewButtonForegroundPressed"), fontBrush);
        TabViewItem().Resources().Insert(winrt::box_value(L"TabViewButtonForegroundPointerOver"), fontBrush);

        _RefreshVisualState();
    }

    // Method Description:
    // - Clear out any color we've set for the TabViewItem.
    // - This method should only be called on the UI thread.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_ClearTabBackgroundColor()
    {
        static const winrt::hstring keys[] = {
            L"TabViewItemHeaderBackground",
            L"TabViewItemHeaderBackgroundSelected",
            L"TabViewItemHeaderBackgroundPointerOver",
            L"TabViewItemHeaderBackgroundPressed",
            L"TabViewItemHeaderForeground",
            L"TabViewItemHeaderForegroundSelected",
            L"TabViewItemHeaderForegroundPointerOver",
            L"TabViewItemHeaderForegroundPressed",
            L"TabViewItemHeaderCloseButtonForeground",
            L"TabViewItemHeaderCloseButtonForegroundPressed",
            L"TabViewItemHeaderCloseButtonForegroundPointerOver",
            L"TabViewItemHeaderPressedCloseButtonForeground",
            L"TabViewItemHeaderPointerOverCloseButtonForeground",
            L"TabViewItemHeaderSelectedCloseButtonForeground",
            L"TabViewItemHeaderCloseButtonBackgroundPressed",
            L"TabViewItemHeaderCloseButtonBackgroundPointerOver",
            L"TabViewButtonForegroundActiveTab",
            L"TabViewButtonForegroundPressed",
            L"TabViewButtonForegroundPointerOver"
        };

        // simply clear any of the colors in the tab's dict
        for (const auto& keyString : keys)
        {
            auto key = winrt::box_value(keyString);
            if (TabViewItem().Resources().HasKey(key))
            {
                TabViewItem().Resources().Remove(key);
            }
        }

        // GH#11382 DON'T set the background to null. If you do that, then the
        // tab won't be hit testable at all. Transparent, however, is a totally
        // valid hit test target. That makes sense.
        TabViewItem().Background(WUX::Media::SolidColorBrush{ Windows::UI::Colors::Transparent() });

        _RefreshVisualState();
    }

    // Method Description:
    // Toggles the visual state of the tab view item,
    // so that changes to the tab color are reflected immediately
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_RefreshVisualState()
    {
        if (TabViewItem().IsSelected())
        {
            VisualStateManager::GoToState(TabViewItem(), L"Normal", true);
            VisualStateManager::GoToState(TabViewItem(), L"Selected", true);
        }
        else
        {
            VisualStateManager::GoToState(TabViewItem(), L"Selected", true);
            VisualStateManager::GoToState(TabViewItem(), L"Normal", true);
        }
    }

}
