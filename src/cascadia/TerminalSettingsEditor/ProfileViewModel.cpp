// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ProfileViewModel.h"
#include "ProfileViewModel.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>
#include "../WinRTUtils/inc/Utils.h"
#include "../../renderer/base/FontCache.h"

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Windows::Foundation::Collections::IObservableVector<Editor::Font> ProfileViewModel::_MonospaceFontList{ nullptr };
    Windows::Foundation::Collections::IObservableVector<Editor::Font> ProfileViewModel::_FontList{ nullptr };
    Windows::Foundation::Collections::IObservableVector<winrt::hstring> ProfileViewModel::_SerialList{ nullptr };

    ProfileViewModel::ProfileViewModel(const Model::Profile& profile, const Model::CascadiaSettings& appSettings) :
        _profile{ profile },
        _defaultAppearanceViewModel{ winrt::make<implementation::AppearanceViewModel>(profile.DefaultAppearance().try_as<AppearanceConfig>()) },
        _originalProfileGuid{ profile.Guid() },
        _appSettings{ appSettings },
        _unfocusedAppearanceViewModel{ nullptr }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(AntiAliasingMode, TextAntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode, L"Profile_AntialiasingMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(CloseOnExitMode, CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, L"Profile_CloseOnExit", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(ScrollState, ScrollbarState, winrt::Microsoft::Terminal::Control::ScrollbarState, L"Profile_ScrollbarVisibility", L"Content");

        INITIALIZE_BINDABLE_ENUM_SETTING(BoundRate, SerialBoundRate, winrt::Microsoft::Terminal::Control::SerialBoundRate, L"Profile_BoundRate", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(DataWidth, SerialDataWidth, winrt::Microsoft::Terminal::Control::SerialDataWidth, L"Profile_DataWidth", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(CheckBit, SerialCheckBit, winrt::Microsoft::Terminal::Control::SerialCheckBit, L"Profile_CheckBit", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(StopBit, SerialStopBit, winrt::Microsoft::Terminal::Control::SerialStopBit, L"Profile_StopBit", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(FlowControl, SerialFlowControl, winrt::Microsoft::Terminal::Control::SerialFlowControl, L"Profile_FlowControl", L"Content");

        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        //  unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"IsBaseLayer")
            {
                // we _always_ want to show the background image settings in base layer
                _NotifyChanges(L"BackgroundImageSettingsVisible");
            }
            else if (viewModelProperty == L"StartingDirectory")
            {
                // notify listener that all starting directory related values might have changed
                // NOTE: this is similar to what is done with BackgroundImagePath above
                _NotifyChanges(L"UseParentProcessDirectory", L"UseCustomStartingDirectory");
            }
            else if (viewModelProperty == L"AntialiasingMode")
            {
                _NotifyChanges(L"CurrentAntiAliasingMode");
            }
            else if (viewModelProperty == L"CloseOnExit")
            {
                _NotifyChanges(L"CurrentCloseOnExitMode");
            }
            else if (viewModelProperty == L"BellStyle")
            {
                _NotifyChanges(L"IsBellStyleFlagSet");
            }
            else if (viewModelProperty == L"ScrollState")
            {
                _NotifyChanges(L"CurrentScrollState");
            }
            else if (viewModelProperty == L"BoundRate")
            {
                _NotifyChanges(L"CurrentBoundRate");
            }
            else if (viewModelProperty == L"DataWidth")
            {
                _NotifyChanges(L"CurrentDataWidth");
            }
            else if (viewModelProperty == L"CheckBit")
            {
                _NotifyChanges(L"CurrentCheckBit");
            }
            else if (viewModelProperty == L"StopBit")
            {
                _NotifyChanges(L"CurrentStopBit");
            }
            else if (viewModelProperty == L"FlowControl")
            {
                _NotifyChanges(L"CurrentFlowControl");
            }
        });

        // Do the same for the starting directory
        if (!StartingDirectory().empty())
        {
            _lastStartingDirectoryPath = StartingDirectory();
        }

        // generate the font list, if we don't have one
        if (!_FontList || !_MonospaceFontList)
        {
            UpdateFontList();
        }

        // serial list
        if (!_SerialList)
        {
            UpdateSerial();
        }

        if (profile.HasUnfocusedAppearance())
        {
            _unfocusedAppearanceViewModel = winrt::make<implementation::AppearanceViewModel>(profile.UnfocusedAppearance().try_as<AppearanceConfig>());
        }

        _defaultAppearanceViewModel.IsDefault(true);
    }

    winrt::Windows::Foundation::IInspectable ProfileViewModel::CurrentSerial()
    {
        std::vector<winrt::hstring> serialList = GetAllSerial();
        winrt::Windows::Foundation::IInspectable fallback;
        winrt::hstring curr{ _profile.SerialPort() };

        if (curr.empty())
        {
            if (!serialList.empty())
            {
                auto first = _SerialList.begin();
                fallback = box_value(*first);
                _profile.SerialPort(*first);
            }
            else
            {
                fallback = box_value(L"");
            }
        }
        else
        {
            if (!(std::find(serialList.begin(), serialList.end(), curr) != serialList.end()))
            {
                //curr = curr + L" (invalid)";
                //serialList.emplace_back(curr);
                //_SerialList = single_threaded_observable_vector<winrt::hstring>(std::move(serialList));
                //_NotifyChanges(L"SerialList");
                fallback = box_value(L"None");
            }
            else
            {
                fallback = box_value(curr);
            }
        }

        return fallback;
    }

    void ProfileViewModel::CurrentSerial(const winrt::Windows::Foundation::IInspectable& tag)
    {        
        if (!tag) { return; }
        winrt::hstring select{ unbox_value<hstring>(tag) };
        _profile.SerialPort(select);
    }

    std::vector<winrt::hstring> ProfileViewModel::GetAllSerial(void)
    {
        HKEY hRegKey;
        int nCount = 0;
        std::vector<winrt::hstring> serialList;

        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Hardware\\DeviceMap\\SerialComm", 0, KEY_READ, &hRegKey) == ERROR_SUCCESS)
        {
            while (true)
            {
                TCHAR szName[MAX_PATH] = { 0 };
                TCHAR szPort[MAX_PATH] = { 0 };
                DWORD nValueSize = MAX_PATH - 1;
                DWORD nDataSize = MAX_PATH - 1;
                DWORD nType;
                if (::RegEnumValue(hRegKey, nCount, szName, &nValueSize, NULL, &nType, (LPBYTE)szPort, &nDataSize) == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }
                serialList.emplace_back(szPort);
                nCount++;
            }
            ::RegCloseKey(hRegKey);
        }

        return serialList;
    }

    void ProfileViewModel::UpdateSerial(void)
    {
        std::vector<winrt::hstring> serialList = GetAllSerial();
        std::vector<winrt::hstring> _serialList = serialList;

        // serialist
        _SerialList = single_threaded_observable_vector<winrt::hstring>(std::move(serialList));

        // currserial
        winrt::hstring curr{ _profile.SerialPort() };
        if (curr.empty())
        {
            if (!_serialList.empty())
            {
                auto first = _serialList.begin();
                _profile.SerialPort(*first);
            }
            else
            {
                _profile.SerialPort(L"");
            }
        }
    }

    void ProfileViewModel::Serial_DropDownOpened(Windows::Foundation::IInspectable const& /*sender*/,
                                                 Windows::Foundation::IInspectable const& /*e*/)
    {
        std::vector<winrt::hstring> serialList = GetAllSerial();
        _SerialList = single_threaded_observable_vector<winrt::hstring>(std::move(serialList));
        _NotifyChanges(L"SerialList");
    }

    bool ProfileViewModel::SerialEnable(void)
    {
        return _profile.SerialEnable();
    }

    void ProfileViewModel::SerialEnable(bool value)
    {
        _profile.SerialEnable(value);
    }

    Model::TerminalSettings ProfileViewModel::TermSettings() const
    {
        return Model::TerminalSettings::CreateForPreview(_appSettings, _profile);
    }

    // Method Description:
    // - Updates the lists of fonts and sorts them alphabetically
    void ProfileViewModel::UpdateFontList() noexcept
    try
    {
        // initialize font list
        std::vector<Editor::Font> fontList;
        std::vector<Editor::Font> monospaceFontList;

        // get the font collection; subscribe to updates
        const auto fontCollection = ::Microsoft::Console::Render::FontCache::GetFresh();

        for (UINT32 i = 0; i < fontCollection->GetFontFamilyCount(); ++i)
        {
            try
            {
                // get the font family
                com_ptr<IDWriteFontFamily> fontFamily;
                THROW_IF_FAILED(fontCollection->GetFontFamily(i, fontFamily.put()));

                // get the font's localized names
                com_ptr<IDWriteLocalizedStrings> localizedFamilyNames;
                THROW_IF_FAILED(fontFamily->GetFamilyNames(localizedFamilyNames.put()));

                // construct a font entry for tracking
                if (const auto fontEntry{ _GetFont(localizedFamilyNames) })
                {
                    // check if the font is monospaced
                    try
                    {
                        com_ptr<IDWriteFont> font;
                        THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_NORMAL,
                                                                         DWRITE_FONT_STRETCH::DWRITE_FONT_STRETCH_NORMAL,
                                                                         DWRITE_FONT_STYLE::DWRITE_FONT_STYLE_NORMAL,
                                                                         font.put()));

                        // add the font name to our list of monospace fonts
                        const auto castedFont{ font.try_as<IDWriteFont1>() };
                        if (castedFont && castedFont->IsMonospacedFont())
                        {
                            monospaceFontList.emplace_back(fontEntry);
                        }
                    }
                    CATCH_LOG();

                    // add the font name to our list of all fonts
                    fontList.emplace_back(std::move(fontEntry));
                }
            }
            CATCH_LOG();
        }

        // sort and save the lists
        std::sort(begin(fontList), end(fontList), FontComparator());
        _FontList = single_threaded_observable_vector<Editor::Font>(std::move(fontList));

        std::sort(begin(monospaceFontList), end(monospaceFontList), FontComparator());
        _MonospaceFontList = single_threaded_observable_vector<Editor::Font>(std::move(monospaceFontList));
    }
    CATCH_LOG();

    Editor::Font ProfileViewModel::_GetFont(com_ptr<IDWriteLocalizedStrings> localizedFamilyNames)
    {
        // used for the font's name as an identifier (i.e. text block's font family property)
        std::wstring nameID;
        UINT32 nameIDIndex;

        // used for the font's localized name
        std::wstring localizedName;
        UINT32 localizedNameIndex;

        // use our current locale to find the localized name
        auto exists{ FALSE };
        HRESULT hr;
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
        if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH))
        {
            hr = localizedFamilyNames->FindLocaleName(localeName, &localizedNameIndex, &exists);
        }
        if (SUCCEEDED(hr) && !exists)
        {
            // if we can't find the font for our locale, fallback to the en-us one
            // Source: https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-findlocalename
            hr = localizedFamilyNames->FindLocaleName(L"en-us", &localizedNameIndex, &exists);
        }
        if (!exists)
        {
            // failed to find the correct locale, using the first one
            localizedNameIndex = 0;
        }

        // get the localized name
        UINT32 nameLength;
        THROW_IF_FAILED(localizedFamilyNames->GetStringLength(localizedNameIndex, &nameLength));

        localizedName.resize(nameLength);
        THROW_IF_FAILED(localizedFamilyNames->GetString(localizedNameIndex, localizedName.data(), nameLength + 1));

        // now get the nameID
        hr = localizedFamilyNames->FindLocaleName(L"en-us", &nameIDIndex, &exists);
        if (FAILED(hr) || !exists)
        {
            // failed to find it, using the first one
            nameIDIndex = 0;
        }

        // get the nameID
        THROW_IF_FAILED(localizedFamilyNames->GetStringLength(nameIDIndex, &nameLength));
        nameID.resize(nameLength);
        THROW_IF_FAILED(localizedFamilyNames->GetString(nameIDIndex, nameID.data(), nameLength + 1));

        if (!nameID.empty() && !localizedName.empty())
        {
            return make<Font>(nameID, localizedName);
        }
        return nullptr;
    }

    winrt::guid ProfileViewModel::OriginalProfileGuid() const noexcept
    {
        return _originalProfileGuid;
    }

    bool ProfileViewModel::CanDeleteProfile() const
    {
        return !IsBaseLayer();
    }

    Editor::AppearanceViewModel ProfileViewModel::DefaultAppearance()
    {
        return _defaultAppearanceViewModel;
    }

    bool ProfileViewModel::HasUnfocusedAppearance()
    {
        return _profile.HasUnfocusedAppearance();
    }

    bool ProfileViewModel::EditableUnfocusedAppearance() const noexcept
    {
        return Feature_EditableUnfocusedAppearance::IsEnabled();
    }

    bool ProfileViewModel::ShowUnfocusedAppearance()
    {
        return EditableUnfocusedAppearance() && HasUnfocusedAppearance();
    }

    void ProfileViewModel::CreateUnfocusedAppearance()
    {
        _profile.CreateUnfocusedAppearance();

        _unfocusedAppearanceViewModel = winrt::make<implementation::AppearanceViewModel>(_profile.UnfocusedAppearance().try_as<AppearanceConfig>());
        _unfocusedAppearanceViewModel.SchemesList(DefaultAppearance().SchemesList());
        _unfocusedAppearanceViewModel.WindowRoot(DefaultAppearance().WindowRoot());

        _NotifyChanges(L"UnfocusedAppearance", L"HasUnfocusedAppearance", L"ShowUnfocusedAppearance");
    }

    void ProfileViewModel::DeleteUnfocusedAppearance()
    {
        _profile.DeleteUnfocusedAppearance();

        _unfocusedAppearanceViewModel = nullptr;

        _NotifyChanges(L"UnfocusedAppearance", L"HasUnfocusedAppearance", L"ShowUnfocusedAppearance");
    }

    Editor::AppearanceViewModel ProfileViewModel::UnfocusedAppearance()
    {
        return _unfocusedAppearanceViewModel;
    }

    bool ProfileViewModel::VtPassthroughAvailable() const noexcept
    {
        return Feature_VtPassthroughMode::IsEnabled() && Feature_VtPassthroughModeSettingInUI::IsEnabled();
    }

    bool ProfileViewModel::UseParentProcessDirectory()
    {
        return StartingDirectory().empty();
    }

    // This function simply returns the opposite of UseParentProcessDirectory.
    // We bind the 'IsEnabled' parameters of the textbox and browse button
    // to this because it needs to be the reverse of UseParentProcessDirectory
    // but we don't want to create a whole new converter for inverting a boolean
    bool ProfileViewModel::UseCustomStartingDirectory()
    {
        return !UseParentProcessDirectory();
    }

    void ProfileViewModel::UseParentProcessDirectory(const bool useParent)
    {
        if (useParent)
        {
            // Stash the current value of StartingDirectory. If the user
            // checks and un-checks the "Use parent process directory" button, we want
            // the path that we display in the text box to remain unchanged.
            //
            // Only stash this value if it's not empty
            if (!StartingDirectory().empty())
            {
                _lastStartingDirectoryPath = StartingDirectory();
            }
            StartingDirectory(L"");
        }
        else
        {
            // Restore the path we had previously cached as long as it wasn't empty
            // If it was empty, set the starting directory to %USERPROFILE%
            // (we need to set it to something non-empty otherwise we will automatically
            // disable the text box)
            if (_lastStartingDirectoryPath.empty())
            {
                StartingDirectory(L"%USERPROFILE%");
            }
            else
            {
                StartingDirectory(_lastStartingDirectoryPath);
            }
        }
    }

    bool ProfileViewModel::IsBellStyleFlagSet(const uint32_t flag)
    {
        return (WI_EnumValue(BellStyle()) & flag) == flag;
    }

    void ProfileViewModel::SetBellStyleAudible(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Audible, winrt::unbox_value<bool>(on));
        BellStyle(currentStyle);
    }

    void ProfileViewModel::SetBellStyleWindow(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Window, winrt::unbox_value<bool>(on));
        BellStyle(currentStyle);
    }

    void ProfileViewModel::SetBellStyleTaskbar(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Taskbar, winrt::unbox_value<bool>(on));
        BellStyle(currentStyle);
    }

    void ProfileViewModel::DeleteProfile()
    {
        auto deleteProfileArgs{ winrt::make_self<DeleteProfileEventArgs>(Guid()) };
        _DeleteProfileHandlers(*this, *deleteProfileArgs);
    }

    void ProfileViewModel::SetupAppearances(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel> schemesList, Editor::IHostedInWindow windowRoot)
    {
        DefaultAppearance().SchemesList(schemesList);
        DefaultAppearance().WindowRoot(windowRoot);
        if (UnfocusedAppearance())
        {
            UnfocusedAppearance().SchemesList(schemesList);
            UnfocusedAppearance().WindowRoot(windowRoot);
        }
    }
}
