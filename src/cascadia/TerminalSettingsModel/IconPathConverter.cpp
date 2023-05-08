#include "pch.h"
#include "IconPathConverter.h"
#include "IconPathConverter.g.cpp"

#include "Utils.h"

#include <Shlobj.h>
#include <Shlobj_core.h>
#include <wincodec.h>

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
}

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
// These are templates that help us figure out which BitmapIconSource/FontIconSource to use for a given IconSource.
// We have to do this because some of our code still wants to use WUX/MUX IconSources.
#pragma region BitmapIconSource
    template<typename TIconSource>
    struct BitmapIconSource
    {
    };

    template<>
    struct BitmapIconSource<winrt::Microsoft::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Microsoft::UI::Xaml::Controls::BitmapIconSource;
    };

    template<>
    struct BitmapIconSource<winrt::Windows::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Windows::UI::Xaml::Controls::BitmapIconSource;
    };
#pragma endregion

#pragma region FontIconSource
    template<typename TIconSource>
    struct FontIconSource
    {
    };

    template<>
    struct FontIconSource<winrt::Microsoft::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Microsoft::UI::Xaml::Controls::FontIconSource;
    };

    template<>
    struct FontIconSource<winrt::Windows::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Windows::UI::Xaml::Controls::FontIconSource;
    };
#pragma endregion

    // Method Description:
    // - Creates an IconSource for the given path. The icon returned is a colored
    //   icon. If we couldn't create the icon for any reason, we return an empty
    //   IconElement.
    // Template Types:
    // - <TIconSource>: The type of IconSource (MUX, WUX) to generate.
    // Arguments:
    // - path: the full, expanded path to the icon.
    // Return Value:
    // - An IconElement with its IconSource set, if possible.
    template<typename TIconSource>
    TIconSource _getColoredBitmapIcon(const winrt::hstring& path)
    {
        // FontIcon uses glyphs in the private use area, whereas valid URIs only contain ASCII characters.
        // To skip throwing on Uri construction, we can quickly check if the first character is ASCII.
        if (!path.empty() && path.front() < 128)
        {
            try
            {
                winrt::Windows::Foundation::Uri iconUri{ path };
                BitmapIconSource<TIconSource>::type iconSource;
                // Make sure to set this to false, so we keep the RGB data of the
                // image. Otherwise, the icon will be white for all the
                // non-transparent pixels in the image.
                iconSource.ShowAsMonochrome(false);
                iconSource.UriSource(iconUri);
                return iconSource;
            }
            CATCH_LOG();
        }

        return nullptr;
    }

    // Method Description:
    // - Creates an IconSource for the given path.
    //    * If the icon is a path to an image, we'll use that.
    //    * If it isn't, then we'll try and use the text as a FontIcon. If the
    //      character is in the range of symbols reserved for the Segoe MDL2
    //      Asserts, well treat it as such. Otherwise, we'll default to a Sego
    //      UI icon, so things like emoji will work.
    //    * If we couldn't create the icon for any reason, we return an empty
    //      IconElement.
    // Template Types:
    // - <TIconSource>: The type of IconSource (MUX, WUX) to generate.
    // Arguments:
    // - path: the unprocessed path to the icon.
    // Return Value:
    // - An IconElement with its IconSource set, if possible.
    template<typename TIconSource>
    TIconSource _getIconSource(const winrt::hstring& iconPath)
    {
        TIconSource iconSource{ nullptr };

        if (iconPath.size() != 0)
        {
            const auto expandedIconPath{ _expandIconPath(iconPath) };
            iconSource = _getColoredBitmapIcon<TIconSource>(expandedIconPath);

            // If we fail to set the icon source using the "icon" as a path,
            // let's try it as a symbol/emoji.
            //
            // Anything longer than 2 wchar_t's _isn't_ an emoji or symbol, so
            // don't do this if it's just an invalid path.
            if (!iconSource && iconPath.size() <= 2)
            {
                try
                {
                    FontIconSource<TIconSource>::type icon;
                    const auto ch = iconPath[0];

                    // The range of MDL2 Icons isn't explicitly defined, but
                    // we're using this based off the table on:
                    // https://docs.microsoft.com/en-us/windows/uwp/design/style/segoe-ui-symbol-font
                    const auto isMDL2Icon = ch >= L'\uE700' && ch <= L'\uF8FF';
                    if (isMDL2Icon)
                    {
                        icon.FontFamily(winrt::Windows::UI::Xaml::Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
                    }
                    else
                    {
                        // Note: you _do_ need to manually set the font here.
                        icon.FontFamily(winrt::Windows::UI::Xaml::Media::FontFamily{ L"Segoe UI" });
                    }
                    icon.FontSize(12);
                    icon.Glyph(iconPath);
                    iconSource = icon;
                }
                CATCH_LOG();
            }
        }
        if (!iconSource)
        {
            // Set the default IconSource to a BitmapIconSource with a null source
            // (instead of just nullptr) because there's a really weird crash when swapping
            // data bound IconSourceElements in a ListViewTemplate (i.e. CommandPalette).
            // Swapping between nullptr IconSources and non-null IconSources causes a crash
            // to occur, but swapping between IconSources with a null source and non-null IconSources
            // work perfectly fine :shrug:.
            BitmapIconSource<TIconSource>::type icon;
            icon.UriSource(nullptr);
            iconSource = icon;
        }

        return iconSource;
    }

    static winrt::hstring _expandIconPath(hstring iconPath)
    {
        if (iconPath.empty())
        {
            return iconPath;
        }
        winrt::hstring envExpandedPath{ wil::ExpandEnvironmentStringsW<std::wstring>(iconPath.c_str()) };
        return envExpandedPath;
    }

    // Method Description:
    // - Attempt to convert something into another type. For the
    //   IconPathConverter, we support a variety of icons:
    //    * If the icon is a path to an image, we'll use that.
    //    * If it isn't, then we'll try and use the text as a FontIcon. If the
    //      character is in the range of symbols reserved for the Segoe MDL2
    //      Asserts, well treat it as such. Otherwise, we'll default to a Sego
    //      UI icon, so things like emoji will work.
    // - MUST BE CALLED ON THE UI THREAD.
    // Arguments:
    // - value: the input object to attempt to convert into an IconSource.
    // Return Value:
    // - Visible if the object was a string and wasn't the empty string.
    Foundation::IInspectable IconPathConverter::Convert(const Foundation::IInspectable& value,
                                                        const Windows::UI::Xaml::Interop::TypeName& /* targetType */,
                                                        const Foundation::IInspectable& /* parameter */,
                                                        const hstring& /* language */)
    {
        const auto& iconPath = winrt::unbox_value_or<winrt::hstring>(value, L"");
        return _getIconSource<Controls::IconSource>(iconPath);
    }

    // unused for one-way bindings
    Foundation::IInspectable IconPathConverter::ConvertBack(const Foundation::IInspectable& /* value */,
                                                            const Windows::UI::Xaml::Interop::TypeName& /* targetType */,
                                                            const Foundation::IInspectable& /* parameter */,
                                                            const hstring& /* language */)
    {
        throw hresult_not_implemented();
    }

    Windows::UI::Xaml::Controls::IconSource _IconSourceWUX(hstring path)
    {
        return _getIconSource<Windows::UI::Xaml::Controls::IconSource>(path);
    }

    Microsoft::UI::Xaml::Controls::IconSource _IconSourceMUX(hstring path)
    {
        return _getIconSource<Microsoft::UI::Xaml::Controls::IconSource>(path);
    }

    SoftwareBitmap _convertToSoftwareBitmap(HICON hicon,
                                            BitmapPixelFormat pixelFormat,
                                            BitmapAlphaMode alphaMode,
                                            IWICImagingFactory* imagingFactory)
    {
        // Load the icon into an IWICBitmap
        wil::com_ptr<IWICBitmap> iconBitmap;
        THROW_IF_FAILED(imagingFactory->CreateBitmapFromHICON(hicon, iconBitmap.put()));

        // Put the IWICBitmap into a SoftwareBitmap. This may fail if WICBitmap's format is not supported by
        // SoftwareBitmap. CreateBitmapFromHICON always creates RGBA8 so we're ok.
        auto softwareBitmap = winrt::capture<SoftwareBitmap>(
            winrt::create_instance<ISoftwareBitmapNativeFactory>(CLSID_SoftwareBitmapNativeFactory),
            &ISoftwareBitmapNativeFactory::CreateFromWICBitmap,
            iconBitmap.get(),
            false);

        // Convert the pixel format and alpha mode if necessary
        if (softwareBitmap.BitmapPixelFormat() != pixelFormat || softwareBitmap.BitmapAlphaMode() != alphaMode)
        {
            softwareBitmap = SoftwareBitmap::Convert(softwareBitmap, pixelFormat, alphaMode);
        }

        return softwareBitmap;
    }

    SoftwareBitmap _getBitmapFromIconFileAsync(const winrt::hstring& iconPath,
                                               int32_t iconIndex,
                                               uint32_t iconSize)
    {
        wil::unique_hicon hicon;
        LOG_IF_FAILED(SHDefExtractIcon(iconPath.c_str(), iconIndex, 0, &hicon, nullptr, iconSize));

        if (!hicon)
        {
            return nullptr;
        }

        wil::com_ptr<IWICImagingFactory> wicImagingFactory;
        THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicImagingFactory)));

        return _convertToSoftwareBitmap(hicon.get(),
                                        BitmapPixelFormat::Bgra8,
                                        BitmapAlphaMode::Premultiplied,
                                        wicImagingFactory.get());
    }

    // Method Description:
    // - Attempt to get the icon index from the icon path provided
    // Arguments:
    // - iconPath: the full icon path, including the index if present
    // - iconPathWithoutIndex: the place to store the icon path, sans the index if present
    // Return Value:
    // - nullopt if the iconPath is not an exe/dll/lnk file in the first place
    // - 0 if the iconPath is an exe/dll/lnk file but does not contain an index (i.e. we default
    //   to the first icon in the file)
    // - the icon index if the iconPath is an exe/dll/lnk file and contains an index
    std::optional<int> _getIconIndex(const winrt::hstring& iconPath, std::wstring_view& iconPathWithoutIndex)
    {
        const auto pathView = std::wstring_view{ iconPath };
        // Does iconPath have a comma in it? If so, split the string on the
        // comma and look for the index and extension.
        const auto commaIndex = pathView.find(L',');

        // split the path on the comma
        iconPathWithoutIndex = pathView.substr(0, commaIndex);

        // It's an exe, dll, or lnk, so we need to extract the icon from the file.
        if (!til::ends_with(iconPathWithoutIndex, L".exe") &&
            !til::ends_with(iconPathWithoutIndex, L".dll") &&
            !til::ends_with(iconPathWithoutIndex, L".lnk"))
        {
            return std::nullopt;
        }

        if (commaIndex != std::wstring::npos)
        {
            // Convert the string iconIndex to an int
            const auto index = til::to_ulong(pathView.substr(commaIndex + 1));
            if (index == til::to_ulong_error)
            {
                return std::nullopt;
            }
            return static_cast<int>(index);
        }

        // We had a binary path, but no index. Default to 0.
        return 0;
    }

    winrt::Windows::UI::Xaml::Media::Imaging::SoftwareBitmapSource _getImageIconSourceForBinary(std::wstring_view iconPathWithoutIndex,
                                                                                                int index)
    {
        // Try:
        // * c:\Windows\System32\SHELL32.dll, 210
        // * c:\Windows\System32\notepad.exe, 0
        // * C:\Program Files\PowerShell\6-preview\pwsh.exe, 0 (this doesn't exist for me)
        // * C:\Program Files\PowerShell\7\pwsh.exe, 0

        const auto swBitmap{ _getBitmapFromIconFileAsync(winrt::hstring{ iconPathWithoutIndex }, index, 32) };
        if (swBitmap == nullptr)
        {
            return nullptr;
        }

        winrt::Windows::UI::Xaml::Media::Imaging::SoftwareBitmapSource bitmapSource{};
        bitmapSource.SetBitmapAsync(swBitmap);
        return bitmapSource;
    }

    MUX::Controls::IconSource IconPathConverter::IconSourceMUX(const winrt::hstring& iconPath)
    {
        std::wstring_view iconPathWithoutIndex;
        const auto indexOpt = _getIconIndex(iconPath, iconPathWithoutIndex);
        if (!indexOpt.has_value())
        {
            return _IconSourceMUX(iconPath);
        }

        const auto bitmapSource = _getImageIconSourceForBinary(iconPathWithoutIndex, indexOpt.value());

        MUX::Controls::ImageIconSource imageIconSource{};
        imageIconSource.ImageSource(bitmapSource);

        return imageIconSource;
    }

    Windows::UI::Xaml::Controls::IconElement IconPathConverter::IconWUX(const winrt::hstring& iconPath)
    {
        std::wstring_view iconPathWithoutIndex;
        const auto indexOpt = _getIconIndex(iconPath, iconPathWithoutIndex);
        if (!indexOpt.has_value())
        {
            auto source = _IconSourceWUX(iconPath);
            Controls::IconSourceElement icon;
            icon.IconSource(source);
            return icon;
        }

        const auto bitmapSource = _getImageIconSourceForBinary(iconPathWithoutIndex, indexOpt.value());

        winrt::Microsoft::UI::Xaml::Controls::ImageIcon icon{};
        icon.Source(bitmapSource);
        icon.Width(32);
        icon.Height(32);
        return icon;
    }
}
