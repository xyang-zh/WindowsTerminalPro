// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "DeleteProfileEventArgs.g.h"
#include "ProfileViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "Appearances.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ProfileViewModel : ProfileViewModelT<ProfileViewModel>, ViewModelHelper<ProfileViewModel>
    {
    public:
        // font face
        static void UpdateFontList() noexcept;
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> CompleteFontList() noexcept { return _FontList; };     
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> MonospaceFontList() noexcept { return _MonospaceFontList; };

        // serial
        Windows::Foundation::Collections::IObservableVector<winrt::hstring> SerialList() noexcept { return _SerialList; };
        Windows::Foundation::IInspectable CurrentSerial();
        void CurrentSerial(const winrt::Windows::Foundation::IInspectable& tag);
        std::vector<winrt::hstring> GetAllSerial(void);
        void UpdateSerial(void);
        void Serial_DropDownOpened(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& e);
        bool SerialEnable(void);
        void SerialEnable(bool value);

        ProfileViewModel(const Model::Profile& profile, const Model::CascadiaSettings& settings);
        Model::TerminalSettings TermSettings() const;
        void DeleteProfile();

        void SetupAppearances(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel> schemesList, Editor::IHostedInWindow windowRoot);

        // bell style bits
        bool IsBellStyleFlagSet(const uint32_t flag);
        void SetBellStyleAudible(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleWindow(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleTaskbar(winrt::Windows::Foundation::IReference<bool> on);

        void SetAcrylicOpacityPercentageValue(double value)
        {
            Opacity(winrt::Microsoft::Terminal::Settings::Editor::Converters::PercentageValueToPercentage(value));
        };

        void SetPadding(double value)
        {
            Padding(to_hstring(value));
        }

        // starting directory
        bool UseParentProcessDirectory();
        void UseParentProcessDirectory(const bool useParent);
        bool UseCustomStartingDirectory();

        // general profile knowledge
        winrt::guid OriginalProfileGuid() const noexcept;
        bool CanDeleteProfile() const;
        Editor::AppearanceViewModel DefaultAppearance();
        Editor::AppearanceViewModel UnfocusedAppearance();
        bool HasUnfocusedAppearance();
        bool EditableUnfocusedAppearance() const noexcept;
        bool ShowUnfocusedAppearance();
        void CreateUnfocusedAppearance();
        void DeleteUnfocusedAppearance();
        bool VtPassthroughAvailable() const noexcept;

        VIEW_MODEL_OBSERVABLE_PROPERTY(ProfileSubPage, CurrentPage);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_profile, Guid);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_profile, ConnectionType);
        OBSERVABLE_PROJECTED_SETTING(_profile, Name);
        OBSERVABLE_PROJECTED_SETTING(_profile, Source);
        OBSERVABLE_PROJECTED_SETTING(_profile, Hidden);
        OBSERVABLE_PROJECTED_SETTING(_profile, Icon);
        OBSERVABLE_PROJECTED_SETTING(_profile, CloseOnExit);
        OBSERVABLE_PROJECTED_SETTING(_profile, TabTitle);
        OBSERVABLE_PROJECTED_SETTING(_profile, TabColor);
        OBSERVABLE_PROJECTED_SETTING(_profile, SuppressApplicationTitle);
        OBSERVABLE_PROJECTED_SETTING(_profile, UseAcrylic);
        OBSERVABLE_PROJECTED_SETTING(_profile, ScrollState);
        OBSERVABLE_PROJECTED_SETTING(_profile, Padding);
        OBSERVABLE_PROJECTED_SETTING(_profile, Commandline);
        OBSERVABLE_PROJECTED_SETTING(_profile, StartingDirectory);
        OBSERVABLE_PROJECTED_SETTING(_profile, AntialiasingMode);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), Foreground);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), Background);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), SelectionBackground);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), CursorColor);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), Opacity);
        OBSERVABLE_PROJECTED_SETTING(_profile, HistorySize);
        OBSERVABLE_PROJECTED_SETTING(_profile, SnapOnInput);
        OBSERVABLE_PROJECTED_SETTING(_profile, AltGrAliasing);
        OBSERVABLE_PROJECTED_SETTING(_profile, BellStyle);
        OBSERVABLE_PROJECTED_SETTING(_profile, UseAtlasEngine);
        OBSERVABLE_PROJECTED_SETTING(_profile, Elevate);
        OBSERVABLE_PROJECTED_SETTING(_profile, VtPassthrough);
        OBSERVABLE_PROJECTED_SETTING(_profile, BoundRate);
        OBSERVABLE_PROJECTED_SETTING(_profile, DataWidth);
        OBSERVABLE_PROJECTED_SETTING(_profile, CheckBit);
        OBSERVABLE_PROJECTED_SETTING(_profile, StopBit);
        OBSERVABLE_PROJECTED_SETTING(_profile, FlowControl);

        WINRT_PROPERTY(bool, IsBaseLayer, false);
        WINRT_PROPERTY(bool, FocusDeleteButton, false);
        WINRT_PROPERTY(IHostedInWindow, WindowRoot, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, Microsoft::Terminal::Settings::Model::CloseOnExitMode, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, Microsoft::Terminal::Control::ScrollbarState, ScrollState);

        GETSET_BINDABLE_ENUM_SETTING(BoundRate, Microsoft::Terminal::Control::SerialBoundRate, BoundRate);
        GETSET_BINDABLE_ENUM_SETTING(DataWidth, Microsoft::Terminal::Control::SerialDataWidth, DataWidth);
        GETSET_BINDABLE_ENUM_SETTING(CheckBit, Microsoft::Terminal::Control::SerialCheckBit, CheckBit);
        GETSET_BINDABLE_ENUM_SETTING(StopBit, Microsoft::Terminal::Control::SerialStopBit, StopBit);
        GETSET_BINDABLE_ENUM_SETTING(FlowControl, Microsoft::Terminal::Control::SerialFlowControl, FlowControl);

        TYPED_EVENT(DeleteProfile, Editor::ProfileViewModel, Editor::DeleteProfileEventArgs);

    private:
        Model::Profile _profile;
        winrt::guid _originalProfileGuid;
        winrt::hstring _lastBgImagePath;
        winrt::hstring _lastStartingDirectoryPath;
        Editor::AppearanceViewModel _defaultAppearanceViewModel;

        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _MonospaceFontList;
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _FontList;
        static Windows::Foundation::Collections::IObservableVector<winrt::hstring> _SerialList;

        static Editor::Font _GetFont(com_ptr<IDWriteLocalizedStrings> localizedFamilyNames);

        Model::CascadiaSettings _appSettings;
        Editor::AppearanceViewModel _unfocusedAppearanceViewModel;
    };

    struct DeleteProfileEventArgs :
        public DeleteProfileEventArgsT<DeleteProfileEventArgs>
    {
    public:
        DeleteProfileEventArgs(guid profileGuid) :
            _ProfileGuid(profileGuid) {}

        guid ProfileGuid() const noexcept { return _ProfileGuid; }

    private:
        guid _ProfileGuid{};
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    // Since we have static functions, we need a factory.
    BASIC_FACTORY(ProfileViewModel);
}
