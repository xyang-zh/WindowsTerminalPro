// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "ColorPickupFlyout.h"
#include "TerminalTab.h"
#include "TerminalTab.g.cpp"
#include "Utils.h"
#include "ColorHelper.h"
#include "AppLogic.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::System;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace winrt::TerminalApp::implementation
{
    TerminalTab::TerminalTab(std::shared_ptr<Pane> rootPane)
    {
        _rootPane = rootPane;
        _activePane = nullptr;

        auto firstId = _nextPaneId;

        _rootPane->WalkTree([&](std::shared_ptr<Pane> pane) {
            // update the IDs on each pane
            if (pane->_IsLeaf())
            {
                pane->Id(_nextPaneId);
                _nextPaneId++;
            }
            // Try to find the pane marked active (if it exists)
            if (pane->_lastActive)
            {
                _activePane = pane;
            }
        });

        // In case none of the panes were already marked as the focus, just
        // focus the first one.
        if (_activePane == nullptr)
        {
            const auto firstPane = _rootPane->FindPane(firstId);
            firstPane->SetActive();
            _activePane = firstPane;
        }
        // If the focused pane is a leaf, add it to the MRU panes
        if (const auto id = _activePane->Id())
        {
            _mruPanes.insert(_mruPanes.begin(), id.value());
        }

        _Setup();
    }

    // Method Description:
    // - Shared setup for the constructors. Assumed that _rootPane has been set.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::_Setup()
    {
        _rootClosedToken = _rootPane->Closed([=](auto&& /*s*/, auto&& /*e*/) {
            _ClosedHandlers(nullptr, nullptr);
        });

        Content(_rootPane->GetRootElement());

        _MakeTabViewItem();
        _CreateContextMenu();

        _headerControl.TabStatus(_tabStatus);

        // Add an event handler for the header control to tell us when they want their title to change
        _headerControl.TitleChangeRequested([weakThis = get_weak()](auto&& title) {
            if (auto tab{ weakThis.get() })
            {
                tab->SetTabText(title);
            }
        });

        // GH#9162 - when the header is done renaming, ask for focus to be
        // tossed back to the control, rather into ourselves.
        _headerControl.RenameEnded([weakThis = get_weak()](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_RequestFocusActiveControlHandlers();
            }
        });

        _UpdateHeaderControlMaxWidth();

        // Use our header control as the TabViewItem's header
        TabViewItem().Header(_headerControl);
    }

    // Method Description:
    // - Called when the timer for the bell indicator in the tab header fires
    // - Removes the bell indicator from the tab header
    // Arguments:
    // - sender, e: not used
    void TerminalTab::_BellIndicatorTimerTick(const Windows::Foundation::IInspectable& /*sender*/, const Windows::Foundation::IInspectable& /*e*/)
    {
        ShowBellIndicator(false);
        // Just do a sanity check that the timer still exists before we stop it
        if (_bellIndicatorTimer.has_value())
        {
            _bellIndicatorTimer->Stop();
            _bellIndicatorTimer = std::nullopt;
        }
    }

    // Method Description:
    // - Initializes a TabViewItem for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::_MakeTabViewItem()
    {
        TabBase::_MakeTabViewItem();

        TabViewItem().DoubleTapped([weakThis = get_weak()](auto&& /*s*/, auto&& /*e*/) {
            if (auto tab{ weakThis.get() })
            {
                tab->ActivateTabRenamer();
            }
        });

        UpdateTitle();
        _RecalculateAndApplyTabColor();
    }

    winrt::fire_and_forget TerminalTab::_UpdateHeaderControlMaxWidth()
    {
        auto weakThis{ get_weak() };

        co_await wil::resume_foreground(TabViewItem().Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            try
            {
                // Make sure to try/catch this, because the LocalTests won't be
                // able to use this helper.
                const auto settings{ winrt::TerminalApp::implementation::AppLogic::CurrentAppSettings() };
                if (settings.GlobalSettings().TabWidthMode() == winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::SizeToContent)
                {
                    tab->_headerControl.RenamerMaxWidth(HeaderRenameBoxWidthTitleLength);
                }
                else
                {
                    tab->_headerControl.RenamerMaxWidth(HeaderRenameBoxWidthDefault);
                }
            }
            CATCH_LOG()
        }
    }

    // Method Description:
    // - Returns nullptr if no children of this tab were the last control to be
    //   focused, the active control of the current pane, or the last active child control
    //   of the active pane if it is a parent.
    // - This control might not currently be focused, if the tab itself is not
    //   currently focused.
    // Arguments:
    // - <none>
    // Return Value:
    // - nullptr if no children were marked `_lastFocused`, else the TermControl
    //   that was last focused.
    TermControl TerminalTab::GetActiveTerminalControl() const
    {
        if (_activePane)
        {
            return _activePane->GetLastFocusedTerminalControl();
        }
        return nullptr;
    }

    // Method Description:
    // - Called after construction of a Tab object to bind event handlers to its
    //   associated Pane and TermControl objects
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::Initialize()
    {
        _rootPane->WalkTree([&](std::shared_ptr<Pane> pane) {
            // Attach event handlers to each new pane
            _AttachEventHandlersToPane(pane);
            if (auto control = pane->GetTerminalControl())
            {
                _AttachEventHandlersToControl(pane->Id().value(), control);
            }
        });
    }

    // Method Description:
    // - Updates our focus state. If we're gaining focus, make sure to transfer
    //   focus to the last focused terminal control in our tree of controls.
    // Arguments:
    // - focused: our new focus state
    // Return Value:
    // - <none>
    void TerminalTab::Focus(WUX::FocusState focusState)
    {
        _focusState = focusState;

        if (_focusState != FocusState::Unfocused)
        {
            auto lastFocusedControl = GetActiveTerminalControl();
            if (lastFocusedControl)
            {
                lastFocusedControl.Focus(_focusState);

                // Update our own progress state. This will fire an event signaling
                // that our taskbar progress changed.
                _UpdateProgressState();
            }
            // When we gain focus, remove the bell indicator if it is active
            if (_tabStatus.BellIndicator())
            {
                ShowBellIndicator(false);
            }
        }
    }

    // Method Description:
    // - Returns nullopt if no children of this tab were the last control to be
    //   focused, or the GUID of the profile of the last control to be focused (if
    //   there was one).
    // Arguments:
    // - <none>
    // Return Value:
    // - nullopt if no children of this tab were the last control to be
    //   focused, else the GUID of the profile of the last control to be focused
    Profile TerminalTab::GetFocusedProfile() const noexcept
    {
        return _activePane->GetFocusedProfile();
    }

    // Method Description:
    // - Attempts to update the settings that apply to this tab.
    // - Panes are handled elsewhere, by somebody who can establish broader knowledge
    //   of the settings that apply to all tabs.
    // Return Value:
    // - <none>
    void TerminalTab::UpdateSettings()
    {
        // The tabWidthMode may have changed, update the header control accordingly
        _UpdateHeaderControlMaxWidth();
    }

    // Method Description:
    // - Set the icon on the TabViewItem for this tab.
    // Arguments:
    // - iconPath: The new path string to use as the IconPath for our TabViewItem
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalTab::UpdateIcon(const winrt::hstring iconPath)
    {
        // Don't reload our icon if it hasn't changed.
        if (iconPath == _lastIconPath)
        {
            co_return;
        }

        _lastIconPath = iconPath;

        // If the icon is currently hidden, just return here (but only after setting _lastIconPath to the new path
        // for when we show the icon again)
        if (_iconHidden)
        {
            co_return;
        }

        auto weakThis{ get_weak() };

        co_await wil::resume_foreground(TabViewItem().Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            // The TabViewItem Icon needs MUX while the IconSourceElement in the CommandPalette needs WUX...
            Icon(_lastIconPath);
            TabViewItem().IconSource(IconPathConverter::IconSourceMUX(_lastIconPath));
        }
    }

    // Method Description:
    // - Hide or show the tab icon for this tab
    // - Used when we want to show the progress ring, which should replace the icon
    // Arguments:
    // - hide: if true, we hide the icon; if false, we show the icon
    winrt::fire_and_forget TerminalTab::HideIcon(const bool hide)
    {
        auto weakThis{ get_weak() };

        co_await wil::resume_foreground(TabViewItem().Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            if (tab->_iconHidden != hide)
            {
                if (hide)
                {
                    Icon({});
                    TabViewItem().IconSource(IconSource{ nullptr });
                }
                else
                {
                    Icon(_lastIconPath);
                    TabViewItem().IconSource(IconPathConverter::IconSourceMUX(_lastIconPath));
                }
                tab->_iconHidden = hide;
            }
        }
    }

    // Method Description:
    // - Hide or show the bell indicator in the tab header
    // Arguments:
    // - show: if true, we show the indicator; if false, we hide the indicator
    winrt::fire_and_forget TerminalTab::ShowBellIndicator(const bool show)
    {
        auto weakThis{ get_weak() };

        co_await wil::resume_foreground(TabViewItem().Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            _tabStatus.BellIndicator(show);
        }
    }

    // Method Description:
    // - Activates the timer for the bell indicator in the tab
    // - Called if a bell raised when the tab already has focus
    winrt::fire_and_forget TerminalTab::ActivateBellIndicatorTimer()
    {
        auto weakThis{ get_weak() };

        co_await wil::resume_foreground(TabViewItem().Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            if (!tab->_bellIndicatorTimer.has_value())
            {
                DispatcherTimer bellIndicatorTimer;
                bellIndicatorTimer.Interval(std::chrono::milliseconds(2000));
                bellIndicatorTimer.Tick({ get_weak(), &TerminalTab::_BellIndicatorTimerTick });
                bellIndicatorTimer.Start();
                tab->_bellIndicatorTimer.emplace(std::move(bellIndicatorTimer));
            }
        }
    }

    // Method Description:
    // - Gets the title string of the last focused terminal control in our tree.
    //   Returns the empty string if there is no such control.
    // Arguments:
    // - <none>
    // Return Value:
    // - the title string of the last focused terminal control in our tree.
    winrt::hstring TerminalTab::_GetActiveTitle() const
    {
        if (!_runtimeTabText.empty())
        {
            return _runtimeTabText;
        }
        if (!_activePane->_IsLeaf())
        {
            return RS_(L"MultiplePanes");
        }
        const auto lastFocusedControl = GetActiveTerminalControl();
        return lastFocusedControl ? lastFocusedControl.Title() : L"";
    }

    // Method Description:
    // - Set the text on the TabViewItem for this tab, and bubbles the new title
    //   value up to anyone listening for changes to our title. Callers can
    //   listen for the title change with a PropertyChanged even handler.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalTab::UpdateTitle()
    {
        auto weakThis{ get_weak() };
        co_await wil::resume_foreground(TabViewItem().Dispatcher());
        if (auto tab{ weakThis.get() })
        {
            const auto activeTitle = _GetActiveTitle();
            // Bubble our current tab text to anyone who's listening for changes.
            Title(activeTitle);

            // Update the control to reflect the changed title
            _headerControl.Title(activeTitle);
            Automation::AutomationProperties::SetName(tab->TabViewItem(), activeTitle);
            _UpdateToolTip();
        }
    }

    // Method Description:
    // - Move the viewport of the terminal up or down a number of lines. Negative
    //      values of `delta` will move the view up, and positive values will move
    //      the viewport down.
    // Arguments:
    // - delta: a number of lines to move the viewport relative to the current viewport.
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalTab::Scroll(const int delta)
    {
        auto control = GetActiveTerminalControl();

        co_await wil::resume_foreground(control.Dispatcher());

        const auto currentOffset = control.ScrollOffset();
        control.ScrollViewport(::base::ClampAdd(currentOffset, delta));
    }

    // Method Description:
    // - Serializes the state of this tab as a series of commands that can be
    //   executed to recreate it.
    // Arguments:
    // - <none>
    // Return Value:
    // - A vector of commands
    std::vector<ActionAndArgs> TerminalTab::BuildStartupActions() const
    {
        // Give initial ids (0 for the child created with this tab,
        // 1 for the child after the first split.
        auto state = _rootPane->BuildStartupActions(0, 1);

        {
            ActionAndArgs newTabAction{};
            newTabAction.Action(ShortcutAction::NewTab);
            NewTabArgs newTabArgs{ state.firstPane->GetTerminalArgsForPane() };
            newTabAction.Args(newTabArgs);

            state.args.emplace(state.args.begin(), std::move(newTabAction));
        }

        if (_runtimeTabColor)
        {
            ActionAndArgs setColorAction{};
            setColorAction.Action(ShortcutAction::SetTabColor);

            SetTabColorArgs setColorArgs{ _runtimeTabColor.value() };
            setColorAction.Args(setColorArgs);

            state.args.emplace_back(std::move(setColorAction));
        }

        if (!_runtimeTabText.empty())
        {
            ActionAndArgs renameTabAction{};
            renameTabAction.Action(ShortcutAction::RenameTab);

            RenameTabArgs renameTabArgs{ _runtimeTabText };
            renameTabAction.Args(renameTabArgs);

            state.args.emplace_back(std::move(renameTabAction));
        }

        // If we only have one arg, we only have 1 pane so we don't need any
        // special focus logic
        if (state.args.size() > 1 && state.focusedPaneId.has_value())
        {
            ActionAndArgs focusPaneAction{};
            focusPaneAction.Action(ShortcutAction::FocusPane);
            FocusPaneArgs focusArgs{ state.focusedPaneId.value() };
            focusPaneAction.Args(focusArgs);

            state.args.emplace_back(std::move(focusPaneAction));
        }

        if (_zoomedPane)
        {
            // we start without any panes zoomed so toggle zoom will enable zoom.
            ActionAndArgs zoomPaneAction{};
            zoomPaneAction.Action(ShortcutAction::TogglePaneZoom);

            state.args.emplace_back(std::move(zoomPaneAction));
        }

        return state.args;
    }

    // Method Description:
    // - Split the focused pane in our tree of panes, and place the
    //   given pane into the tree of panes according to the split
    // Arguments:
    // - splitType: The type of split we want to create
    // - splitSize: The size of the split we want to create
    // - pane: The new pane to add to the tree of panes; note that this pane
    //         could itself be a parent pane/the root node of a tree of panes
    // Return Value:
    // - <none>
    void TerminalTab::SplitPane(SplitDirection splitType,
                                const float splitSize,
                                std::shared_ptr<Pane> pane)
    {
        // Add the new event handlers to the new pane(s)
        // and update their ids.
        pane->WalkTree([&](auto p) {
            _AttachEventHandlersToPane(p);
            if (p->_IsLeaf())
            {
                p->Id(_nextPaneId);
                _AttachEventHandlersToControl(p->Id().value(), p->_control);
                _nextPaneId++;
            }
            return false;
        });
        // Make sure to take the ID before calling Split() - Split() will clear out the active pane's ID
        const auto activePaneId = _activePane->Id();
        // Depending on which direction will be split, the new pane can be
        // either the first or second child, but this will always return the
        // original pane first.
        auto [original, newPane] = _activePane->Split(splitType, splitSize, pane);

        // The active pane has an id if it is a leaf
        if (activePaneId)
        {
            original->Id(activePaneId.value());
        }

        _activePane = original;

        // Add a event handlers to the new panes' GotFocus event. When the pane
        // gains focus, we'll mark it as the new active pane.
        _AttachEventHandlersToPane(original);

        // Immediately update our tracker of the focused pane now. If we're
        // splitting panes during startup (from a commandline), then it's
        // possible that the focus events won't propagate immediately. Updating
        // the focus here will give the same effect though.
        _UpdateActivePane(newPane);
    }

    // Method Description:
    // - Removes the currently active pane from this tab. If that was the only
    //   remaining pane, then the entire tab is closed as well.
    // Arguments:
    // - <none>
    // Return Value:
    // - The removed pane, if the remove succeeded.
    std::shared_ptr<Pane> TerminalTab::DetachPane()
    {
        // if we only have one pane, or the focused pane is the root, remove it
        // entirely and close this tab
        if (_rootPane == _activePane)
        {
            return DetachRoot();
        }

        // Attempt to remove the active pane from the tree
        if (const auto pane = _rootPane->DetachPane(_activePane))
        {
            // Just make sure that the remaining pane is marked active
            _UpdateActivePane(_rootPane->GetActivePane());

            return pane;
        }

        return nullptr;
    }

    // Method Description:
    // - Closes this tab and returns the root pane to be used elsewhere.
    // Arguments:
    // - <none>
    // Return Value:
    // - The root pane.
    std::shared_ptr<Pane> TerminalTab::DetachRoot()
    {
        // remove the closed event handler since we are closing the tab
        // manually.
        _rootPane->Closed(_rootClosedToken);
        auto p = _rootPane;
        p->WalkTree([](auto pane) {
            pane->_DetachedHandlers(pane);
        });

        // Clean up references and close the tab
        _rootPane = nullptr;
        _activePane = nullptr;
        Content(nullptr);
        _ClosedHandlers(nullptr, nullptr);

        return p;
    }

    // Method Description:
    // - Add an arbitrary pane to this tab. This will be added as a split on the
    //   currently active pane.
    // Arguments:
    // - pane: The pane to add.
    // Return Value:
    // - <none>
    void TerminalTab::AttachPane(std::shared_ptr<Pane> pane)
    {
        // Add the new event handlers to the new pane(s)
        // and update their ids.
        pane->WalkTree([&](auto p) {
            _AttachEventHandlersToPane(p);
            if (p->_IsLeaf())
            {
                p->Id(_nextPaneId);
                _AttachEventHandlersToControl(p->Id().value(), p->_control);
                _nextPaneId++;
            }
        });

        // pass the old id to the new child
        const auto previousId = _activePane->Id();

        // Add the new pane as an automatic split on the active pane.
        auto first = _activePane->AttachPane(pane, SplitDirection::Automatic);

        // This will be true if the original _activePane is a leaf pane.
        // If it is a parent pane then we don't want to set an ID on it.
        if (previousId)
        {
            first->Id(previousId.value());
        }

        // Update with event handlers on the new child.
        _activePane = first;
        _AttachEventHandlersToPane(first);

        // Make sure that we have the right pane set as the active pane
        if (const auto focus = pane->GetActivePane())
        {
            _UpdateActivePane(focus);
        }
    }

    // Method Description:
    // - Attaches the given color picker to ourselves
    // - Typically will be called after we have sent a request for the color picker
    // Arguments:
    // - colorPicker: The color picker that we should attach to ourselves
    // Return Value:
    // - <none>
    void TerminalTab::AttachColorPicker(TerminalApp::ColorPickupFlyout& colorPicker)
    {
        auto weakThis{ get_weak() };

        _tabColorPickup = colorPicker;

        _colorSelectedToken = _tabColorPickup.ColorSelected([weakThis](auto newTabColor) {
            if (auto tab{ weakThis.get() })
            {
                tab->SetRuntimeTabColor(newTabColor);
            }
        });

        _colorClearedToken = _tabColorPickup.ColorCleared([weakThis]() {
            if (auto tab{ weakThis.get() })
            {
                tab->ResetRuntimeTabColor();
            }
        });

        _pickerClosedToken = _tabColorPickup.Closed([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_tabColorPickup.ColorSelected(tab->_colorSelectedToken);
                tab->_tabColorPickup.ColorCleared(tab->_colorClearedToken);
                tab->_tabColorPickup.Closed(tab->_pickerClosedToken);
                tab->_tabColorPickup = nullptr;
            }
        });

        _tabColorPickup.ShowAt(TabViewItem());
    }

    // Method Description:
    // - Find the currently active pane, and then switch the split direction of
    //   its parent. E.g. switch from Horizontal to Vertical.
    // Return Value:
    // - <none>
    void TerminalTab::ToggleSplitOrientation()
    {
        _rootPane->ToggleSplitOrientation();
    }

    // Method Description:
    // - See Pane::CalcSnappedDimension
    float TerminalTab::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        return _rootPane->CalcSnappedDimension(widthOrHeight, dimension);
    }

    // Method Description:
    // - Attempt to move a separator between panes, as to resize each child on
    //   either size of the separator. See Pane::ResizePane for details.
    // Arguments:
    // - direction: The direction to move the separator in.
    // Return Value:
    // - <none>
    void TerminalTab::ResizePane(const ResizeDirection& direction)
    {
        // NOTE: This _must_ be called on the root pane, so that it can propagate
        // throughout the entire tree.
        _rootPane->ResizePane(direction);
    }

    // Method Description:
    // - Attempt to move focus between panes, as to focus the child on
    //   the other side of the separator. See Pane::NavigateFocus for details.
    // Arguments:
    // - direction: The direction to move the focus in.
    // Return Value:
    // - Whether changing the focus succeeded. This allows a keychord to propagate
    //   to the terminal when no other panes are present (GH#6219)
    bool TerminalTab::NavigateFocus(const FocusDirection& direction)
    {
        // NOTE: This _must_ be called on the root pane, so that it can propagate
        // throughout the entire tree.
        if (const auto newFocus = _rootPane->NavigateDirection(_activePane, direction, _mruPanes))
        {
            // Mark that we want the active pane to changed
            _changingActivePane = true;
            const auto res = _rootPane->FocusPane(newFocus);
            _changingActivePane = false;

            if (_zoomedPane)
            {
                UpdateZoom(newFocus);
            }

            return res;
        }

        return false;
    }

    // Method Description:
    // - Attempts to swap the location of the focused pane with another pane
    //   according to direction. When there are multiple adjacent panes it will
    //   select the first one (top-left-most).
    // Arguments:
    // - direction: The direction to move the pane in.
    // Return Value:
    // - true if two panes were swapped.
    bool TerminalTab::SwapPane(const FocusDirection& direction)
    {
        // You cannot swap panes with the parent/child pane because of the
        // circular reference.
        if (direction == FocusDirection::Parent || direction == FocusDirection::Child)
        {
            return false;
        }
        // NOTE: This _must_ be called on the root pane, so that it can propagate
        // throughout the entire tree.
        if (auto neighbor = _rootPane->NavigateDirection(_activePane, direction, _mruPanes))
        {
            // SwapPanes will refocus the terminal to make sure that it has focus
            // even after moving.
            _changingActivePane = true;
            const auto res = _rootPane->SwapPanes(_activePane, neighbor);
            _changingActivePane = false;
            return res;
        }

        return false;
    }

    bool TerminalTab::FocusPane(const uint32_t id)
    {
        _changingActivePane = true;
        const auto res = _rootPane->FocusPane(id);
        _changingActivePane = false;
        return res;
    }

    // Method Description:
    // - Prepares this tab for being removed from the UI hierarchy by shutting down all active connections.
    void TerminalTab::Shutdown()
    {
        if (_rootPane)
        {
            _rootPane->Shutdown();
        }
    }

    // Method Description:
    // - Closes the currently focused pane in this tab. If it's the last pane in
    //   this tab, our Closed event will be fired (at a later time) for anyone
    //   registered as a handler of our close event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::ClosePane()
    {
        _activePane->Close();
    }

    void TerminalTab::SetTabText(winrt::hstring title)
    {
        _runtimeTabText = title;
        UpdateTitle();
    }

    winrt::hstring TerminalTab::GetTabText() const
    {
        return _runtimeTabText;
    }

    void TerminalTab::ResetTabText()
    {
        _runtimeTabText = L"";
        UpdateTitle();
    }

    // Method Description:
    // - Show a TextBox in the Header to allow the user to set a string
    //     to use as an override for the tab's text
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::ActivateTabRenamer()
    {
        _headerControl.BeginRename();
    }

    // Method Description:
    // - Removes any event handlers set by the tab on the given pane's control.
    //   The pane's ID is the most stable identifier for a given control, because
    //   the control itself doesn't have a particular ID and its pointer is
    //   unstable since it is moved when panes split.
    // Arguments:
    // - paneId: The ID of the pane that contains the given control.
    // - control: the control to remove events from.
    // Return Value:
    // - <none>
    void TerminalTab::_DetachEventHandlersFromControl(const uint32_t paneId, const TermControl& control)
    {
        auto it = _controlEvents.find(paneId);
        if (it != _controlEvents.end())
        {
            auto& events = it->second;

            control.TitleChanged(events.titleToken);
            control.TabColorChanged(events.colorToken);
            control.SetTaskbarProgress(events.taskbarToken);
            control.ReadOnlyChanged(events.readOnlyToken);
            control.FocusFollowMouseRequested(events.focusToken);

            _controlEvents.erase(paneId);
        }
    }

    // Method Description:
    // - Register any event handlers that we may need with the given TermControl.
    //   This should be called on each and every TermControl that we add to the tree
    //   of Panes in this tab. We'll add events too:
    //   * notify us when the control's title changed, so we can update our own
    //     title (if necessary)
    // Arguments:
    // - paneId: the ID of the pane that this control belongs to.
    // - control: the TermControl to add events to.
    // Return Value:
    // - <none>
    void TerminalTab::_AttachEventHandlersToControl(const uint32_t paneId, const TermControl& control)
    {
        auto weakThis{ get_weak() };
        auto dispatcher = TabViewItem().Dispatcher();
        ControlEventTokens events{};

        events.titleToken = control.TitleChanged([weakThis](auto&&, auto&&) {
            // Check if Tab's lifetime has expired
            if (auto tab{ weakThis.get() })
            {
                // The title of the control changed, but not necessarily the title of the tab.
                // Set the tab's text to the active panes' text.
                tab->UpdateTitle();
            }
        });

        events.colorToken = control.TabColorChanged([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                // The control's tabColor changed, but it is not necessarily the
                // active control in this tab. We'll just recalculate the
                // current color anyways.
                tab->_RecalculateAndApplyTabColor();
            }
        });

        events.taskbarToken = control.SetTaskbarProgress([dispatcher, weakThis](auto&&, auto&&) -> winrt::fire_and_forget {
            co_await wil::resume_foreground(dispatcher);
            // Check if Tab's lifetime has expired
            if (auto tab{ weakThis.get() })
            {
                tab->_UpdateProgressState();
            }
        });

        events.readOnlyToken = control.ReadOnlyChanged([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_RecalculateAndApplyReadOnly();
            }
        });

        events.focusToken = control.FocusFollowMouseRequested([weakThis](auto&& sender, auto&&) {
            if (const auto tab{ weakThis.get() })
            {
                if (tab->_focusState != FocusState::Unfocused)
                {
                    if (const auto termControl{ sender.try_as<winrt::Microsoft::Terminal::Control::TermControl>() })
                    {
                        termControl.Focus(FocusState::Pointer);
                    }
                }
            }
        });

        _controlEvents[paneId] = events;
    }

    // Method Description:
    // - Get the combined taskbar state for the tab. This is the combination of
    //   all the states of all our panes. Taskbar states are given a priority
    //   based on the rules in:
    //   https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist3-setprogressstate
    //   under "How the Taskbar Button Chooses the Progress Indicator for a
    //   Group"
    // Arguments:
    // - <none>
    // Return Value:
    // - A TaskbarState object representing the combined taskbar state and
    //   progress percentage of all our panes.
    winrt::TerminalApp::TaskbarState TerminalTab::GetCombinedTaskbarState() const
    {
        std::vector<winrt::TerminalApp::TaskbarState> states;
        if (_rootPane)
        {
            _rootPane->CollectTaskbarStates(states);
        }
        return states.empty() ? winrt::make<winrt::TerminalApp::implementation::TaskbarState>() :
                                *std::min_element(states.begin(), states.end(), TerminalApp::implementation::TaskbarState::ComparePriority);
    }

    // Method Description:
    // - This should be called on the UI thread. If you don't, then it might
    //   silently do nothing.
    // - Update our TabStatus to reflect the progress state of the currently
    //   active pane.
    // - This is called every time _any_ control's progress state changes,
    //   regardless of if that control is the active one or not. This is simpler
    //   then re-attaching this handler to the active control each time it
    //   changes.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::_UpdateProgressState()
    {
        const auto state{ GetCombinedTaskbarState() };

        const auto taskbarState = state.State();
        // The progress of the control changed, but not necessarily the progress of the tab.
        // Set the tab's progress ring to the active pane's progress
        if (taskbarState > 0)
        {
            if (taskbarState == 3)
            {
                // 3 is the indeterminate state, set the progress ring as such
                _tabStatus.IsProgressRingIndeterminate(true);
            }
            else
            {
                // any non-indeterminate state has a value, set the progress ring as such
                _tabStatus.IsProgressRingIndeterminate(false);

                const auto progressValue = gsl::narrow<uint32_t>(state.Progress());
                _tabStatus.ProgressValue(progressValue);
            }
            // Hide the tab icon (the progress ring is placed over it)
            HideIcon(true);
            _tabStatus.IsProgressRingActive(true);
        }
        else
        {
            // Show the tab icon
            HideIcon(false);
            _tabStatus.IsProgressRingActive(false);
        }

        // fire an event signaling that our taskbar progress changed.
        _TaskbarProgressChangedHandlers(nullptr, nullptr);
    }

    // Method Description:
    // - Mark the given pane as the active pane in this tab. All other panes
    //   will be marked as inactive. We'll also update our own UI state to
    //   reflect this newly active pane.
    // Arguments:
    // - pane: a Pane to mark as active.
    // Return Value:
    // - <none>
    void TerminalTab::_UpdateActivePane(std::shared_ptr<Pane> pane)
    {
        // Clear the active state of the entire tree, and mark only the pane as active.
        _rootPane->ClearActive();
        _activePane = pane;
        _activePane->SetActive();

        // Update our own title text to match the newly-active pane.
        UpdateTitle();
        _UpdateProgressState();

        // We need to move the pane to the top of our mru list
        // If its already somewhere in the list, remove it first
        if (const auto paneId = pane->Id())
        {
            for (auto i = _mruPanes.begin(); i != _mruPanes.end(); ++i)
            {
                if (*i == paneId.value())
                {
                    _mruPanes.erase(i);
                    break;
                }
            }
            _mruPanes.insert(_mruPanes.begin(), paneId.value());
        }

        _RecalculateAndApplyReadOnly();

        // Raise our own ActivePaneChanged event.
        _ActivePaneChangedHandlers();
    }

    // Method Description:
    // - Add an event handler to this pane's GotFocus event. When that pane gains
    //   focus, we'll mark it as the new active pane. We'll also query the title of
    //   that pane when it's focused to set our own text, and finally, we'll trigger
    //   our own ActivePaneChanged event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::_AttachEventHandlersToPane(std::shared_ptr<Pane> pane)
    {
        auto weakThis{ get_weak() };
        std::weak_ptr<Pane> weakPane{ pane };

        auto gotFocusToken = pane->GotFocus([weakThis](std::shared_ptr<Pane> sender, WUX::FocusState focus) {
            // Do nothing if the Tab's lifetime is expired or pane isn't new.
            auto tab{ weakThis.get() };

            if (tab)
            {
                if (sender != tab->_activePane)
                {
                    auto senderIsChild = tab->_activePane->_HasChild(sender);

                    // Only move focus if we the program moved focus, or the
                    // user moved with their mouse. This is a problem because a
                    // pane isn't a control itself, and if we have the parent
                    // focused we are fine if the terminal control is focused,
                    // but we don't want to update the active pane.
                    if (!senderIsChild ||
                        (focus == WUX::FocusState::Programmatic && tab->_changingActivePane) ||
                        focus == WUX::FocusState::Pointer)
                    {
                        tab->_UpdateActivePane(sender);
                        tab->_RecalculateAndApplyTabColor();
                    }
                }
                tab->_focusState = WUX::FocusState::Programmatic;
                // This tab has gained focus, remove the bell indicator if it is active
                if (tab->_tabStatus.BellIndicator())
                {
                    tab->ShowBellIndicator(false);
                }
            }
        });

        auto lostFocusToken = pane->LostFocus([weakThis](std::shared_ptr<Pane> /*sender*/) {
            // Do nothing if the Tab's lifetime is expired or pane isn't new.
            auto tab{ weakThis.get() };

            if (tab)
            {
                // update this tab's focus state
                tab->_focusState = WUX::FocusState::Unfocused;
            }
        });

        // Add a Closed event handler to the Pane. If the pane closes out from
        // underneath us, and it's zoomed, we want to be able to make sure to
        // update our state accordingly to un-zoom that pane. See GH#7252.
        auto closedToken = pane->Closed([weakThis, weakPane](auto&& /*s*/, auto&& /*e*/) -> winrt::fire_and_forget {
            if (auto tab{ weakThis.get() })
            {
                if (tab->_zoomedPane)
                {
                    co_await wil::resume_foreground(tab->Content().Dispatcher());

                    tab->Content(tab->_rootPane->GetRootElement());
                    tab->ExitZoom();
                }

                if (auto pane = weakPane.lock())
                {
                    // When a parent pane is selected, but one of its children
                    // close out under it we still need to update title/focus information
                    // but the GotFocus handler will rightly see that the _activePane
                    // did not actually change. Triggering
                    if (pane != tab->_activePane && !tab->_activePane->_IsLeaf())
                    {
                        co_await wil::resume_foreground(tab->Content().Dispatcher());
                        tab->_UpdateActivePane(tab->_activePane);
                    }

                    for (auto i = tab->_mruPanes.begin(); i != tab->_mruPanes.end(); ++i)
                    {
                        if (*i == pane->Id())
                        {
                            tab->_mruPanes.erase(i);
                            break;
                        }
                    }
                }
            }
        });

        // Add a PaneRaiseBell event handler to the Pane
        auto bellToken = pane->PaneRaiseBell([weakThis](auto&& /*s*/, auto&& visual) {
            if (auto tab{ weakThis.get() })
            {
                if (visual)
                {
                    // If visual is set, we need to bubble this event all the way to app host to flash the taskbar
                    // In this part of the chain we bubble it from the hosting tab to the page
                    tab->_TabRaiseVisualBellHandlers();
                }

                // Show the bell indicator in the tab header
                tab->ShowBellIndicator(true);

                // If this tab is focused, activate the bell indicator timer, which will
                // remove the bell indicator once it fires
                // (otherwise, the indicator is removed when the tab gets focus)
                if (tab->_focusState != WUX::FocusState::Unfocused)
                {
                    tab->ActivateBellIndicatorTimer();
                }
            }
        });

        // box the event token so that we can give a reference to it in the
        // event handler.
        auto detachedToken = std::make_shared<winrt::event_token>();
        // Add a Detached event handler to the Pane to clean up tab state
        // and other event handlers when a pane is removed from this tab.
        *detachedToken = pane->Detached([weakThis, weakPane, gotFocusToken, lostFocusToken, closedToken, bellToken, detachedToken](std::shared_ptr<Pane> /*sender*/) {
            // Make sure we do this at most once
            if (auto pane{ weakPane.lock() })
            {
                pane->Detached(*detachedToken);
                pane->GotFocus(gotFocusToken);
                pane->LostFocus(lostFocusToken);
                pane->Closed(closedToken);
                pane->PaneRaiseBell(bellToken);

                if (auto tab{ weakThis.get() })
                {
                    if (auto control = pane->GetTerminalControl())
                    {
                        tab->_DetachEventHandlersFromControl(pane->Id().value(), control);
                    }

                    for (auto i = tab->_mruPanes.begin(); i != tab->_mruPanes.end(); ++i)
                    {
                        if (*i == pane->Id())
                        {
                            tab->_mruPanes.erase(i);
                            break;
                        }
                    }
                }
            }
        });
    }

    // Method Description:
    // - Creates a context menu attached to the tab.
    // Currently contains elements allowing to select or
    // to close the current tab
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::_CreateContextMenu()
    {
        auto weakThis{ get_weak() };

        // "Color..."
        Controls::MenuFlyoutItem chooseColorMenuItem;
        Controls::FontIcon colorPickSymbol;
        colorPickSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
        colorPickSymbol.Glyph(L"\xE790");

        chooseColorMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->RequestColorPicker();
            }
        });
        chooseColorMenuItem.Text(RS_(L"TabColorChoose"));
        chooseColorMenuItem.Icon(colorPickSymbol);

        const auto chooseColorToolTip = RS_(L"ChooseColorToolTip");

        WUX::Controls::ToolTipService::SetToolTip(chooseColorMenuItem, box_value(chooseColorToolTip));
        Automation::AutomationProperties::SetHelpText(chooseColorMenuItem, chooseColorToolTip);

        Controls::MenuFlyoutItem renameTabMenuItem;
        {
            // "Rename Tab"
            Controls::FontIcon renameTabSymbol;
            renameTabSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            renameTabSymbol.Glyph(L"\xE8AC"); // Rename

            renameTabMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    tab->ActivateTabRenamer();
                }
            });
            renameTabMenuItem.Text(RS_(L"RenameTabText"));
            renameTabMenuItem.Icon(renameTabSymbol);

            const auto renameTabToolTip = RS_(L"RenameTabToolTip");

            WUX::Controls::ToolTipService::SetToolTip(renameTabMenuItem, box_value(renameTabToolTip));
            Automation::AutomationProperties::SetHelpText(renameTabMenuItem, renameTabToolTip);
        }

        Controls::MenuFlyoutItem duplicateTabMenuItem;
        {
            // "Duplicate Tab"
            Controls::FontIcon duplicateTabSymbol;
            duplicateTabSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            duplicateTabSymbol.Glyph(L"\xF5ED");

            duplicateTabMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    tab->_DuplicateRequestedHandlers();
                }
            });
            duplicateTabMenuItem.Text(RS_(L"DuplicateTabText"));
            duplicateTabMenuItem.Icon(duplicateTabSymbol);

            const auto duplicateTabToolTip = RS_(L"DuplicateTabToolTip");

            WUX::Controls::ToolTipService::SetToolTip(duplicateTabMenuItem, box_value(duplicateTabToolTip));
            Automation::AutomationProperties::SetHelpText(duplicateTabMenuItem, duplicateTabToolTip);
        }

        Controls::MenuFlyoutItem splitTabMenuItem;
        {
            // "Split Tab"
            Controls::FontIcon splitTabSymbol;
            splitTabSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            splitTabSymbol.Glyph(L"\xF246"); // ViewDashboard

            splitTabMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    tab->_SplitTabRequestedHandlers();
                }
            });
            splitTabMenuItem.Text(RS_(L"SplitTabText"));
            splitTabMenuItem.Icon(splitTabSymbol);

            const auto splitTabToolTip = RS_(L"SplitTabToolTip");

            WUX::Controls::ToolTipService::SetToolTip(splitTabMenuItem, box_value(splitTabToolTip));
            Automation::AutomationProperties::SetHelpText(splitTabMenuItem, splitTabToolTip);
        }

        Controls::MenuFlyoutItem exportTabMenuItem;
        {
            // "Export Tab"
            Controls::FontIcon exportTabSymbol;
            exportTabSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            exportTabSymbol.Glyph(L"\xE74E"); // Save

            exportTabMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    tab->_ExportTabRequestedHandlers();
                }
            });
            exportTabMenuItem.Text(RS_(L"ExportTabText"));
            exportTabMenuItem.Icon(exportTabSymbol);

            const auto exportTabToolTip = RS_(L"ExportTabToolTip");

            WUX::Controls::ToolTipService::SetToolTip(exportTabMenuItem, box_value(exportTabToolTip));
            Automation::AutomationProperties::SetHelpText(exportTabMenuItem, exportTabToolTip);
        }

        Controls::MenuFlyoutItem findMenuItem;
        {
            // "Find"
            Controls::FontIcon findSymbol;
            findSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            findSymbol.Glyph(L"\xF78B"); // SearchMedium

            findMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    tab->_FindRequestedHandlers();
                }
            });
            findMenuItem.Text(RS_(L"FindText"));
            findMenuItem.Icon(findSymbol);

            const auto findToolTip = RS_(L"FindToolTip");

            WUX::Controls::ToolTipService::SetToolTip(findMenuItem, box_value(findToolTip));
            Automation::AutomationProperties::SetHelpText(findMenuItem, findToolTip);
        }

        // Build the menu
        Controls::MenuFlyout contextMenuFlyout;
        Controls::MenuFlyoutSeparator menuSeparator;
        contextMenuFlyout.Items().Append(chooseColorMenuItem);
        contextMenuFlyout.Items().Append(renameTabMenuItem);
        contextMenuFlyout.Items().Append(duplicateTabMenuItem);
        contextMenuFlyout.Items().Append(splitTabMenuItem);
        contextMenuFlyout.Items().Append(exportTabMenuItem);
        contextMenuFlyout.Items().Append(findMenuItem);
        contextMenuFlyout.Items().Append(menuSeparator);

        // GH#5750 - When the context menu is dismissed with ESC, toss the focus
        // back to our control.
        contextMenuFlyout.Closed([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                // GH#10112 - if we're opening the tab renamer, don't
                // immediately toss focus to the control. We don't want to steal
                // focus from the tab renamer.
                if (!tab->_headerControl.InRename() && !tab->GetActiveTerminalControl().SearchBoxEditInFocus())
                {
                    tab->_RequestFocusActiveControlHandlers();
                }
            }
        });
        _AppendCloseMenuItems(contextMenuFlyout);
        TabViewItem().ContextFlyout(contextMenuFlyout);
    }

    // Method Description:
    // Returns the tab color, if any
    // Arguments:
    // - <none>
    // Return Value:
    // - The tab's color, if any
    std::optional<winrt::Windows::UI::Color> TerminalTab::GetTabColor()
    {
        std::optional<winrt::Windows::UI::Color> controlTabColor;
        if (const auto& control = GetActiveTerminalControl())
        {
            if (const auto color = control.TabColor())
            {
                controlTabColor = color.Value();
            }
        }

        // A Tab's color will be the result of layering a variety of sources,
        // from the bottom up:
        //
        // Color                |             | Set by
        // -------------------- | --          | --
        // Runtime Color        | _optional_  | Color Picker / `setTabColor` action
        // Control Tab Color    | _optional_  | Profile's `tabColor`, or a color set by VT
        // Theme Tab Background | _optional_  | `tab.backgroundColor` in the theme (handled in _RecalculateAndApplyTabColor)
        // Tab Default Color    | **default** | TabView in XAML
        //
        // coalesce will get us the first of these values that's
        // actually set, with nullopt being our sentinel for "use the default
        // tabview color" (and clear out any colors we've set).

        return til::coalesce(_runtimeTabColor,
                             controlTabColor,
                             std::optional<Windows::UI::Color>(std::nullopt));
    }

    // Method Description:
    // - Sets the runtime tab background color to the color chosen by the user
    // - Sets the tab foreground color depending on the luminance of
    // the background color
    // Arguments:
    // - color: the color the user picked for their tab
    // Return Value:
    // - <none>
    void TerminalTab::SetRuntimeTabColor(const winrt::Windows::UI::Color& color)
    {
        _runtimeTabColor.emplace(color);
        _RecalculateAndApplyTabColor();
    }

    // Method Description:
    // - Clear the custom runtime color of the tab, if any color is set. This
    //   will re-apply whatever the tab's base color should be (either the color
    //   from the control, the theme, or the default tab color.)
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::ResetRuntimeTabColor()
    {
        _runtimeTabColor.reset();
        _RecalculateAndApplyTabColor();
    }

    winrt::Windows::UI::Xaml::Media::Brush TerminalTab::_BackgroundBrush()
    {
        Media::Brush terminalBrush{ nullptr };
        if (const auto& c{ GetActiveTerminalControl() })
        {
            terminalBrush = c.BackgroundBrush();
        }
        return terminalBrush;
    }

    // Method Description:
    // - Send an event to request for the color picker
    // - The listener should attach the color picker via AttachColorPicker()
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::RequestColorPicker()
    {
        _ColorPickerRequestedHandlers();
    }

    // - Get the total number of leaf panes in this tab. This will be the number
    //   of actual controls hosted by this tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - The total number of leaf panes hosted by this tab.
    int TerminalTab::GetLeafPaneCount() const noexcept
    {
        return _rootPane->GetLeafPaneCount();
    }

    // Method Description:
    // - Calculate if a split is possible with the given direction and size.
    // - Converts Automatic splits to an appropriate direction depending on space.
    // Arguments:
    // - splitType: what direction to split.
    // - splitSize: how big the new split should be.
    // - availableSpace: how much space there is to work with.
    // Return value:
    // - This will return nullopt if a split of the given size/direction was not possible,
    //   or it will return the split direction with automatic converted to a cardinal direction.
    std::optional<SplitDirection> TerminalTab::PreCalculateCanSplit(SplitDirection splitType,
                                                                    const float splitSize,
                                                                    winrt::Windows::Foundation::Size availableSpace) const
    {
        return _rootPane->PreCalculateCanSplit(_activePane, splitType, splitSize, availableSpace).value_or(std::nullopt);
    }

    // Method Description:
    // - Updates the zoomed pane when the focus changes
    // Arguments:
    // - newFocus: the new pane to be zoomed
    // Return Value:
    // - <none>
    void TerminalTab::UpdateZoom(std::shared_ptr<Pane> newFocus)
    {
        // clear the existing content so the old zoomed pane can be added back to the root tree
        Content(nullptr);
        _rootPane->Restore(_zoomedPane);
        _zoomedPane = newFocus;
        _rootPane->Maximize(_zoomedPane);
        Content(_zoomedPane->GetRootElement());
    }

    // Method Description:
    // - Toggle our zoom state.
    //   * If we're not zoomed, then zoom the active pane, making it take the
    //     full size of the tab. We'll achieve this by changing our response to
    //     Tab::GetTabContent, so that it'll return the zoomed pane only.
    //   *  If we're currently zoomed on a pane, un-zoom that pane.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalTab::ToggleZoom()
    {
        if (_zoomedPane)
        {
            ExitZoom();
        }
        else
        {
            EnterZoom();
        }
    }

    void TerminalTab::EnterZoom()
    {
        // Clear the content first, because with parent focusing it is possible
        // to zoom the root pane, but setting the content will not trigger the
        // property changed event since it is the same and you would end up with
        // an empty tab.
        Content(nullptr);
        _zoomedPane = _activePane;
        _rootPane->Maximize(_zoomedPane);
        // Update the tab header to show the magnifying glass
        _tabStatus.IsPaneZoomed(true);
        Content(_zoomedPane->GetRootElement());
    }
    void TerminalTab::ExitZoom()
    {
        Content(nullptr);
        _rootPane->Restore(_zoomedPane);
        _zoomedPane = nullptr;
        // Update the tab header to hide the magnifying glass
        _tabStatus.IsPaneZoomed(false);
        Content(_rootPane->GetRootElement());
    }

    bool TerminalTab::IsZoomed()
    {
        return _zoomedPane != nullptr;
    }

    // Method Description:
    // - Toggle read-only mode on the active pane
    // - If a parent pane is selected, this will ensure that all children have
    //   the same read-only status.
    void TerminalTab::TogglePaneReadOnly()
    {
        auto hasReadOnly = false;
        auto allReadOnly = true;
        _activePane->WalkTree([&](auto p) {
            if (const auto& control{ p->GetTerminalControl() })
            {
                hasReadOnly |= control.ReadOnly();
                allReadOnly &= control.ReadOnly();
            }
        });
        _activePane->WalkTree([&](auto p) {
            if (const auto& control{ p->GetTerminalControl() })
            {
                // If all controls have the same read only state then just toggle
                if (allReadOnly || !hasReadOnly)
                {
                    control.ToggleReadOnly();
                }
                // otherwise set to all read only.
                else if (!control.ReadOnly())
                {
                    control.ToggleReadOnly();
                }
            }
        });
    }

    // Method Description:
    // - Calculates if the tab is read-only.
    // The tab is considered read-only if one of the panes is read-only.
    // If after the calculation the tab is read-only we hide the close button on the tab view item
    void TerminalTab::_RecalculateAndApplyReadOnly()
    {
        const auto control = GetActiveTerminalControl();
        if (control)
        {
            const auto isReadOnlyActive = control.ReadOnly();
            _tabStatus.IsReadOnlyActive(isReadOnlyActive);
        }

        ReadOnly(_rootPane->ContainsReadOnly());
        TabViewItem().IsClosable(!ReadOnly());
    }

    std::shared_ptr<Pane> TerminalTab::GetActivePane() const
    {
        return _activePane;
    }

    // Method Description:
    // - Creates a text for the title run in the tool tip by returning tab title
    // or <profile name>: <tab title> in the case the profile name differs from the title
    // Arguments:
    // - <none>
    // Return Value:
    // - The value to populate in the title run of the tool tip
    winrt::hstring TerminalTab::_CreateToolTipTitle()
    {
        if (const auto& control{ GetActiveTerminalControl() })
        {
            const auto profileName{ control.Settings().ProfileName() };
            if (profileName != Title())
            {
                return fmt::format(L"{}: {}", profileName, Title()).data();
            }
        }

        return Title();
    }
}
