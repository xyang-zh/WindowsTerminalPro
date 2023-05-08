// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorSchemeViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "ColorSchemes.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    inline static constexpr uint8_t ColorTableDivider{ 8 };
    inline static constexpr uint8_t ColorTableSize{ 16 };

    inline static constexpr std::wstring_view ForegroundColorTag{ L"Foreground" };
    inline static constexpr std::wstring_view BackgroundColorTag{ L"Background" };
    inline static constexpr std::wstring_view CursorColorTag{ L"CursorColor" };
    inline static constexpr std::wstring_view SelectionBackgroundColorTag{ L"SelectionBackground" };

    struct ColorSchemeViewModel : ColorSchemeViewModelT<ColorSchemeViewModel>, ViewModelHelper<ColorSchemeViewModel>
    {
    public:
        ColorSchemeViewModel(const Model::ColorScheme scheme, const Editor::ColorSchemesPageViewModel parentPageVM, const Model::CascadiaSettings& settings);

        winrt::hstring Name();
        void Name(winrt::hstring newName);
        hstring ToString()
        {
            return Name();
        }

        bool RequestRename(winrt::hstring newName);

        Editor::ColorTableEntry ColorEntryAt(uint32_t index);
        bool IsDefaultScheme();
        void RefreshIsDefault();

        void DeleteConfirmation_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void SetAsDefault_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        // DON'T YOU DARE ADD A `WINRT_CALLBACK(PropertyChanged` TO A CLASS DERIVED FROM ViewModelHelper. Do this instead:
        using ViewModelHelper<ColorSchemeViewModel>::PropertyChanged;

        WINRT_PROPERTY(bool, IsInBoxScheme);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, NonBrightColorTable, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, BrightColorTable, nullptr);

        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, ForegroundColor, _propertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, BackgroundColor, _propertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CursorColor, _propertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, SelectionBackgroundColor, _propertyChangedHandlers, nullptr);

    private:
        winrt::hstring _Name;
        Model::ColorScheme _scheme;
        Model::CascadiaSettings _settings;
        weak_ref<Editor::ColorSchemesPageViewModel> _parentPageVM{ nullptr };

        void _ColorEntryChangedHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args);
    };

    struct ColorTableEntry : ColorTableEntryT<ColorTableEntry>
    {
    public:
        ColorTableEntry(uint8_t index, Windows::UI::Color color);
        ColorTableEntry(std::wstring_view tag, Windows::UI::Color color);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(Windows::UI::Color, Color, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(IInspectable, Tag, _PropertyChangedHandlers);

    private:
        Windows::UI::Color _color;
    };
};
