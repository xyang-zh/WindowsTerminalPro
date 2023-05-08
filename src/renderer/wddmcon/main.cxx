// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "main.h"

HINSTANCE g_hInstance;

#define FONT_SIZE 20.0f
#define FONT_FACE L"Courier New"
#define CONSOLE_MARGIN 2
#define MAX_RENDER_ATTEMPTS 3ul

#define REGSTR_PATH_CONKBD L"SYSTEM\\CurrentControlSet\\Control\\ConKbd"
#define REGSTR_VALUE_DISPLAY_INIT_DELAY  L"DisplayInitDelay"
#define REGSTR_VALUE_FONT_SIZE  L"FontSize"

#define SafeRelease(p) if (p) { (p)->Release(); (p) = nullptr;}

typedef struct tagWDDMCONSOLECONTEXT {
    // Console state
    BOOLEAN fOutputEnabled;
    BOOLEAN fInD2DBatch;
    DXGI_MODE_DESC DisplayMode;
    DWORD DisplayInitDelay;
    CD_IO_DISPLAY_SIZE DisplaySize;
    float FontSize;
    float LineHeight;
    float GlyphWidth;
    float DpiX;
    float DpiY;
    WCHAR *pwszGlyphRunAccel;

    // Device-Independent Resources
    ID2D1Factory *pD2DFactory;
    IDWriteFactory *pDWriteFactory;
    IDWriteTextFormat *pDWriteTextFormat;

    // Device-Dependent Resources
    BOOLEAN fHaveDeviceResources;
    ID3D11Device *pD3DDevice;
    ID3D11DeviceContext *pD3DDeviceContext;
    IDXGIAdapter1 *pDXGIAdapter1;
    IDXGIFactory2 *pDXGIFactory2;
    IDXGIFactoryDWM *pDXGIFactoryDWM;
    IDXGIOutput *pDXGIOutput;
    IDXGISwapChainDWM* pDXGISwapChainDWM;
    IDXGISurface *pDXGISurface;
    ID2D1RenderTarget *pD2DSwapChainRT;
    ID2D1SolidColorBrush *pD2DColorBrush;
} WDDMCONSOLECONTEXT, *PWDDMCONSOLECONTEXT;

void
ReleaseDeviceResources(
    _In_ PWDDMCONSOLECONTEXT pCtx
    )
{
    pCtx->fHaveDeviceResources = FALSE;
    SafeRelease(pCtx->pD2DColorBrush);

    if (pCtx->pD2DSwapChainRT && pCtx->fInD2DBatch)
    {
        pCtx->pD2DSwapChainRT->EndDraw();
    }
    SafeRelease(pCtx->pD2DSwapChainRT);

    SafeRelease(pCtx->pDXGISurface);
    SafeRelease(pCtx->pDXGISwapChainDWM);
    SafeRelease(pCtx->pDXGIOutput);

    if (pCtx->pD3DDeviceContext)
    {
        // To ensure the swap chain goes away we must unbind any views from the
        // D3D pipeline
        pCtx->pD3DDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    }
    SafeRelease(pCtx->pD3DDeviceContext);

    SafeRelease(pCtx->pD3DDevice);

    SafeRelease(pCtx->pDXGIAdapter1);
    SafeRelease(pCtx->pDXGIFactoryDWM);
    SafeRelease(pCtx->pDXGIFactory2);
}

VOID
WINAPI
WDDMConDestroy(
    _In_ HANDLE hDisplay
    )
{
    if (hDisplay != nullptr) {
        const auto pCtx = static_cast<PWDDMCONSOLECONTEXT>(hDisplay);
        ReleaseDeviceResources(pCtx);
        SafeRelease(pCtx->pDWriteTextFormat);
        SafeRelease(pCtx->pDWriteFactory);
        SafeRelease(pCtx->pD2DFactory);

        free(pCtx->pwszGlyphRunAccel);
        free(pCtx);
    }
}

HRESULT
ReadSettings(
    _Inout_ PWDDMCONSOLECONTEXT pCtx
    ) noexcept
{
    auto hr = S_OK;
    DWORD Error = ERROR_SUCCESS;
    HKEY hKey = nullptr;
    auto ValueType = REG_NONE;
    DWORD ValueSize = 0;
    DWORD ValueData = 0;

    if (pCtx == nullptr) {
        hr = E_INVALIDARG;
    }

    if (SUCCEEDED(hr)) {
        Error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                             REGSTR_PATH_CONKBD,
                             0,
                             KEY_READ,
                             &hKey);

        if (Error != ERROR_SUCCESS) {
            hr = HRESULT_FROM_WIN32(Error);
        }
    }

    if (SUCCEEDED(hr)) {
        ValueSize = sizeof(ValueData);

        Error = RegQueryValueEx(hKey,
                                REGSTR_VALUE_DISPLAY_INIT_DELAY,
                                nullptr,
                                &ValueType,
                                reinterpret_cast<PBYTE>(&ValueData),
                                &ValueSize);

        if ((Error == ERROR_SUCCESS) &&
            (ValueType == REG_DWORD) &&
            (ValueSize == sizeof(ValueData))) {
            pCtx->DisplayInitDelay = ValueData;
        }

        ValueSize = sizeof(ValueData);

        Error = RegQueryValueEx(hKey,
                                REGSTR_VALUE_FONT_SIZE,
                                nullptr,
                                &ValueType,
                                reinterpret_cast<PBYTE>(&ValueData),
                                &ValueSize);

        if ((Error == ERROR_SUCCESS) &&
            (ValueType == REG_DWORD) &&
            (ValueSize == sizeof(ValueData)) &&
            (ValueData > 0)) {
            pCtx->FontSize = static_cast<float>(ValueData);
        }
    }

    if (hKey != nullptr) {
        RegCloseKey(hKey);
    }

    return hr;
}

HRESULT
CreateTextLayout(
    _In_ PWDDMCONSOLECONTEXT pCtx,
    _In_reads_(StringLength) const wchar_t *String,
    _In_ size_t StringLength,
    _Out_ IDWriteTextLayout **ppTextLayout
    ) noexcept
{
    auto hr = S_OK;

    if (pCtx == nullptr) {
        hr = E_INVALIDARG;
    }

    if (SUCCEEDED(hr)) {
        hr = pCtx->pDWriteFactory->CreateTextLayout(String,
                                                    gsl::narrow_cast<UINT32>(StringLength),
                                                    pCtx->pDWriteTextFormat,
                                                    static_cast<float>(pCtx->DisplayMode.Width),
                                                    pCtx->LineHeight != 0 ? pCtx->LineHeight : static_cast<float>(pCtx->DisplayMode.Height),
                                                    ppTextLayout);
    }

    return hr;
}

HRESULT
CopyFrontToBack(
    _In_ PWDDMCONSOLECONTEXT pCtx
    )
{
    HRESULT hr;
    ID3D11Resource *pBackBuffer = nullptr;
    ID3D11Resource *pFrontBuffer = nullptr;

    hr = pCtx->pDXGISwapChainDWM->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

    if (SUCCEEDED(hr))
    {
        hr = pCtx->pDXGISwapChainDWM->GetBuffer(1, IID_PPV_ARGS(&pFrontBuffer));
    }

    if (SUCCEEDED(hr))
    {
        pCtx->pD3DDeviceContext->CopyResource(pBackBuffer, pFrontBuffer);
    }

    SafeRelease(pFrontBuffer);
    SafeRelease(pBackBuffer);

    return hr;
}

HRESULT
PresentSwapChain(
    _In_ PWDDMCONSOLECONTEXT pCtx
    )
{
    HRESULT hr;

    hr = pCtx->pDXGISwapChainDWM->Present(1, 0);

    if (SUCCEEDED(hr))
    {
        hr = CopyFrontToBack(pCtx);
    }

    return hr;
}

HRESULT
CreateDeviceResources(
    _In_ PWDDMCONSOLECONTEXT pCtx,
    _In_ BOOLEAN fCreateSwapChain
    )
{
    if (pCtx->fHaveDeviceResources) {
        ReleaseDeviceResources(pCtx);
    }

    auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&pCtx->pDXGIFactory2));

    if (SUCCEEDED(hr)) {
        hr = pCtx->pDXGIFactory2->QueryInterface(IID_PPV_ARGS(&pCtx->pDXGIFactoryDWM));
    }

    if (SUCCEEDED(hr)) {
        hr = pCtx->pDXGIFactory2->EnumAdapters1(0, &pCtx->pDXGIAdapter1);
    }

    if (SUCCEEDED(hr)) {
        const DWORD DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT |
                                  D3D11_CREATE_DEVICE_SINGLETHREADED;

        static constexpr D3D_FEATURE_LEVEL FeatureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_1,
        };

        hr = D3D11CreateDevice(pCtx->pDXGIAdapter1,
                               D3D_DRIVER_TYPE_UNKNOWN,
                               nullptr,
                               DeviceFlags,
                               &FeatureLevels[0],
                               ARRAYSIZE(FeatureLevels),
                               D3D11_SDK_VERSION,
                               &pCtx->pD3DDevice,
                               nullptr,
                               &pCtx->pD3DDeviceContext);
    }

    if (SUCCEEDED(hr)) {
        hr = pCtx->pDXGIAdapter1->EnumOutputs(0, &pCtx->pDXGIOutput);
    }

    if (SUCCEEDED(hr)) {
        const DXGI_MODE_DESC currentmode = {0};
        hr = pCtx->pDXGIOutput->FindClosestMatchingMode(&currentmode,
                                                        &pCtx->DisplayMode,
                                                        pCtx->pD3DDevice);
    }

    if (fCreateSwapChain) {
        if (SUCCEEDED(hr)) {
            D3D11_VIEWPORT viewport;
            viewport.Width = static_cast<FLOAT>(pCtx->DisplayMode.Width);
            viewport.Height = static_cast<FLOAT>(pCtx->DisplayMode.Height);
            viewport.TopLeftX = 0;
            viewport.TopLeftY = 0;
            viewport.MinDepth = 0;
            viewport.MaxDepth = 1;
            pCtx->pD3DDeviceContext->RSSetViewports(1, &viewport);
        }

        if (SUCCEEDED(hr)) {
            DXGI_SWAP_CHAIN_DESC SwapChainDesc = { 0 };
            const DXGI_SAMPLE_DESC LocalSampleDesc = { 1, 0 };

            SwapChainDesc.BufferDesc.Width = 0;
            SwapChainDesc.BufferDesc.Height = 0;
            SwapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
            SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
            SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
            SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

            SwapChainDesc.SampleDesc = LocalSampleDesc;
            SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER;
            SwapChainDesc.BufferCount = 2;
            SwapChainDesc.OutputWindow = nullptr;
            SwapChainDesc.Windowed = FALSE;
            SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
            SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_NONPREROTATED;

            hr = pCtx->pDXGIFactoryDWM->CreateSwapChain(pCtx->pD3DDevice,
                                                        &SwapChainDesc,
                                                        pCtx->pDXGIOutput,
                                                        &pCtx->pDXGISwapChainDWM);
        }

        if (SUCCEEDED(hr)) {
            hr = pCtx->pDXGISwapChainDWM->GetBuffer(0, IID_PPV_ARGS(&pCtx->pDXGISurface));
        }

        if (SUCCEEDED(hr)) {
            const auto props =
                D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
                0.0f,
                0.0f);

            hr = pCtx->pD2DFactory->CreateDxgiSurfaceRenderTarget(pCtx->pDXGISurface,
                                                                  &props,
                                                                  &pCtx->pD2DSwapChainRT);
        }

        if (SUCCEEDED(hr)) {
            hr = pCtx->pD2DSwapChainRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                                              &pCtx->pD2DColorBrush);
        }
    }

    if (SUCCEEDED(hr)) {
        pCtx->fHaveDeviceResources = TRUE;
        if (pCtx->fInD2DBatch) {
            pCtx->pD2DSwapChainRT->BeginDraw();
        }
    } else {
        ReleaseDeviceResources(pCtx);
    }

    return hr;
}

HRESULT
WINAPI
WDDMConCreate(
    _In_ HANDLE *phDisplay
    )
{
    auto hr = S_OK;
    IDWriteTextLayout *pTextLayout = nullptr;
    DWRITE_TEXT_METRICS TextMetrics = {};
    const auto pCtx = static_cast<PWDDMCONSOLECONTEXT>(malloc(sizeof(WDDMCONSOLECONTEXT)));

    if (pCtx == nullptr) {
        hr = E_OUTOFMEMORY;
    } else {
        RtlZeroMemory(pCtx, sizeof(WDDMCONSOLECONTEXT));
        pCtx->fOutputEnabled = FALSE;
        pCtx->FontSize = FONT_SIZE;

        ReadSettings(pCtx);
    }

    if (SUCCEEDED(hr)) {
        if (pCtx->DisplayInitDelay != 0) {
            Sleep(pCtx->DisplayInitDelay);
        }

        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pCtx->pD2DFactory);
    }

    if (SUCCEEDED(hr)) {
        hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(pCtx->pDWriteFactory),
            reinterpret_cast<IUnknown **>(&pCtx->pDWriteFactory)
            );
    }

    if (SUCCEEDED(hr)) {
        hr = pCtx->pDWriteFactory->CreateTextFormat(FONT_FACE,
                                                    nullptr,
                                                    DWRITE_FONT_WEIGHT_NORMAL,
                                                    DWRITE_FONT_STYLE_NORMAL,
                                                    DWRITE_FONT_STRETCH_NORMAL,
                                                    pCtx->FontSize,
                                                    L"en-us",
                                                    &pCtx->pDWriteTextFormat);
    }

    if (SUCCEEDED(hr)) {
        hr = pCtx->pDWriteTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    if (SUCCEEDED(hr)) {
        hr = CreateDeviceResources(pCtx, FALSE);
    }

    if (SUCCEEDED(hr)) {
        ReleaseDeviceResources(pCtx);

        hr = CreateTextLayout(pCtx,
                              L"M",
                              1,
                              &pTextLayout);
    }

    if (SUCCEEDED(hr)) {
        hr = pTextLayout->GetMetrics(&TextMetrics);
        pCtx->GlyphWidth = TextMetrics.width;
        pCtx->LineHeight = TextMetrics.height;
    }

    if (SUCCEEDED(hr)) {
#pragma warning(push)
#pragma warning(disable : 4996) // GetDesktopDpi is deprecated.
        pCtx->pD2DFactory->GetDesktopDpi(&pCtx->DpiX, &pCtx->DpiY);
#pragma warning(pop)
        const auto MaxWidth  = pTextLayout->GetMaxWidth();
        const auto MaxHeight = pTextLayout->GetMaxHeight();
        pCtx->GlyphWidth = floor(pCtx->GlyphWidth);
        pCtx->LineHeight = floor(pCtx->LineHeight);
        pCtx->DisplaySize.Width = static_cast<ULONG>(MaxWidth / pCtx->GlyphWidth);
        pCtx->DisplaySize.Height = static_cast<ULONG>(MaxHeight / pCtx->LineHeight) + 1;
        pCtx->DisplaySize.Width -= CONSOLE_MARGIN * 2;
        pCtx->DisplaySize.Height -= CONSOLE_MARGIN * 2;
    }

    if (SUCCEEDED(hr)) {
        const size_t width = pCtx->DisplaySize.Width;
        pCtx->pwszGlyphRunAccel = static_cast<WCHAR*>(malloc(sizeof(WCHAR) * (width + 1)));
        if (pCtx->pwszGlyphRunAccel == nullptr) {
            hr = E_OUTOFMEMORY;
        }
    }

    SafeRelease(pTextLayout);

    if (SUCCEEDED(hr)) {
        *phDisplay = pCtx;
    } else if (pCtx != nullptr) {
        WDDMConDestroy(pCtx);
    }

    return hr;
}

D2D1::ColorF ConsoleColors[] = { D2D1::ColorF(D2D1::ColorF::Black),
                                 D2D1::ColorF(D2D1::ColorF::DarkBlue),
                                 D2D1::ColorF(D2D1::ColorF::DarkGreen),
                                 D2D1::ColorF(D2D1::ColorF::DarkCyan),
                                 D2D1::ColorF(D2D1::ColorF::DarkRed),
                                 D2D1::ColorF(D2D1::ColorF::DarkMagenta),
                                 D2D1::ColorF(D2D1::ColorF::Olive),
                                 D2D1::ColorF(D2D1::ColorF::DarkGray),
                                 D2D1::ColorF(D2D1::ColorF::LightGray),
                                 D2D1::ColorF(D2D1::ColorF::Blue),
                                 D2D1::ColorF(D2D1::ColorF::Lime),
                                 D2D1::ColorF(D2D1::ColorF::Cyan),
                                 D2D1::ColorF(D2D1::ColorF::Red),
                                 D2D1::ColorF(D2D1::ColorF::Magenta),
                                 D2D1::ColorF(D2D1::ColorF::Yellow),
                                 D2D1::ColorF(D2D1::ColorF::White) };

HRESULT
WINAPI
WDDMConBeginUpdateDisplayBatch(
    _In_ HANDLE hDisplay
    )
{
    auto hr = S_OK;
    PWDDMCONSOLECONTEXT pCtx = nullptr;

    if (hDisplay == nullptr) {
        hr = E_INVALIDARG;
    } else {
        pCtx = static_cast<PWDDMCONSOLECONTEXT>(hDisplay);
    }

    if (SUCCEEDED(hr) && pCtx->fInD2DBatch) {
        hr = E_INVALIDARG;
    }

    if (SUCCEEDED(hr) && pCtx->fOutputEnabled) {
        if (!pCtx->fHaveDeviceResources) {
            hr = CreateDeviceResources(pCtx, TRUE);
        }

        if (SUCCEEDED(hr)) {
            pCtx->pD2DSwapChainRT->BeginDraw();
            pCtx->fInD2DBatch = TRUE;
        }
    }

    return hr;
}

HRESULT
WINAPI
WDDMConEndUpdateDisplayBatch(
    _In_ HANDLE hDisplay
    )
{
    auto hr = S_OK;
    PWDDMCONSOLECONTEXT pCtx = nullptr;

    if (hDisplay == nullptr) {
        hr = E_INVALIDARG;
    } else {
        pCtx = static_cast<PWDDMCONSOLECONTEXT>(hDisplay);
    }

    if (SUCCEEDED(hr) && !pCtx->fInD2DBatch) {
        hr = E_INVALIDARG;
    }

    if (SUCCEEDED(hr) && pCtx->fHaveDeviceResources) {
        pCtx->fInD2DBatch = FALSE;

        hr = pCtx->pD2DSwapChainRT->EndDraw();

        if (SUCCEEDED(hr)) {
           hr = PresentSwapChain(pCtx);
        }

        if (FAILED(hr)) {
            ReleaseDeviceResources(pCtx);
        }
    }

    return hr;
}

HRESULT
WINAPI
WDDMConUpdateDisplay(
    _In_ HANDLE hDisplay,
    _In_ const CD_IO_ROW_INFORMATION *pRowInformation,
    _In_ BOOLEAN fInvalidate
    )
{
    auto hr = S_OK;
    PWDDMCONSOLECONTEXT pCtx = nullptr;

    if (hDisplay == nullptr || pRowInformation == nullptr) {
        hr = E_INVALIDARG;
    } else {
        pCtx = static_cast<PWDDMCONSOLECONTEXT>(hDisplay);
    }

    // To prevent an infinite loop, we need to limit the number of times we try to render.
    // WDDMCon is used typically in bring-up scenarios, especially ones with unstable graphics drivers.
    // As such without the limit, an unstable graphics device can cause us to get stuck here
    // and hang console subsystem activities indefinitely.
    ULONG RenderAttempts = 0;

ReRender:
    ULONG ColumnIndex = 0;
    auto LineY = 0.0f;
    if (SUCCEEDED(hr) && pCtx->fOutputEnabled) {
        if (SUCCEEDED(hr) && !pCtx->fHaveDeviceResources) {
            hr = CreateDeviceResources(pCtx, TRUE);
        }

        if (SUCCEEDED(hr)) {
            LineY = pRowInformation->Index * pCtx->LineHeight;
            LineY += CONSOLE_MARGIN * pCtx->LineHeight;
            if (!pCtx->fInD2DBatch) {
                pCtx->pD2DSwapChainRT->BeginDraw();
            }
        }

        while (SUCCEEDED(hr)) {
            IDWriteTextLayout *pTextLayout = nullptr;
            if (fInvalidate ||
                (memcmp(&pRowInformation->New[ColumnIndex],
                        &pRowInformation->Old[ColumnIndex],
                        sizeof(CD_IO_CHARACTER)) != 0)) {
                const auto* const pCharacter = &pRowInformation->New[ColumnIndex];
                const auto CharacterOrigin = (ColumnIndex + CONSOLE_MARGIN) * pCtx->GlyphWidth;
                const auto ColumnIndexStart = ColumnIndex;
                auto ColumnIndexReadAhead = ColumnIndex + 1;
                ULONG GlyphRunLength;

                pCtx->pwszGlyphRunAccel[ColumnIndex] = pRowInformation->New[ColumnIndex].Character;
                if (ColumnIndexReadAhead != pCtx->DisplaySize.Width) {
                    while (pRowInformation->New[ColumnIndex].Attribute == pRowInformation->New[ColumnIndexReadAhead].Attribute) {
                        if (memcmp(&pRowInformation->New[ColumnIndexReadAhead],
                                   &pRowInformation->Old[ColumnIndexReadAhead],
                                   sizeof(CD_IO_CHARACTER)) == 0) {
                            break;
                        }

                        pCtx->pwszGlyphRunAccel[ColumnIndexReadAhead] = pRowInformation->New[ColumnIndexReadAhead].Character;

                        if (++ColumnIndexReadAhead == pCtx->DisplaySize.Width) {
                            break;
                        }
                    }
                }
                pCtx->pwszGlyphRunAccel[ColumnIndexReadAhead] = '\0';
                GlyphRunLength = ColumnIndexReadAhead - ColumnIndex;

                if (SUCCEEDED(hr)) {
                    hr = CreateTextLayout(pCtx,
                                          &pCtx->pwszGlyphRunAccel[ColumnIndex],
                                          GlyphRunLength,
                                          &pTextLayout);
                }

                ColumnIndex = ColumnIndexReadAhead - 1;

                if (SUCCEEDED(hr)) {
                    auto GlyphRectangle = D2D1::RectF(CharacterOrigin,
                                                      LineY,
                                                      CharacterOrigin + pCtx->GlyphWidth * GlyphRunLength,
                                                      LineY + pCtx->LineHeight);
                    const auto Origin = D2D1::Point2F(CharacterOrigin, LineY);

                    if (ColumnIndexStart == 0) {
                        GlyphRectangle.left = 0.0f;
                    }

                    if (static_cast<UINT>(pRowInformation->Index) == 0) {
                        GlyphRectangle.top = 0.0f;
                    }

                    if (ColumnIndex == pCtx->DisplaySize.Width - 1) {
                        GlyphRectangle.right = static_cast<float>(pCtx->DisplayMode.Width);
                    }

                    if (static_cast<UINT>(pRowInformation->Index) == pCtx->DisplaySize.Height - 1) {
                        GlyphRectangle.bottom = static_cast<float>(pCtx->DisplayMode.Height);
                    }

                    pCtx->pD2DColorBrush->SetColor(
                        ConsoleColors[(pCharacter->Attribute >> 4) & 0xF]);
                    pCtx->pD2DSwapChainRT->FillRectangle(&GlyphRectangle,
                                                          pCtx->pD2DColorBrush);

                    pCtx->pD2DColorBrush->SetColor(ConsoleColors[pCharacter->Attribute & 0xF]);
                    pCtx->pD2DSwapChainRT->DrawTextLayout(Origin,
                                                          pTextLayout,
                                                          pCtx->pD2DColorBrush,
                                                          D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
                }

                SafeRelease(pTextLayout);
            }

            if (++ColumnIndex == pCtx->DisplaySize.Width) {
                break;
            }
        }

        if (SUCCEEDED(hr)) {
            if (!pCtx->fInD2DBatch) {
                hr = pCtx->pD2DSwapChainRT->EndDraw();

                if (SUCCEEDED(hr)) {
                    hr = PresentSwapChain(pCtx);
                }
            }
        }

        if (FAILED(hr) && pCtx != nullptr && pCtx->fHaveDeviceResources) {
            ReleaseDeviceResources(pCtx);
            RenderAttempts++;

            if (RenderAttempts < MAX_RENDER_ATTEMPTS)
            {
                hr = S_OK;
#pragma warning(suppress : 26438 26448)
                goto ReRender;
            }
        }
    }

    return hr;
}


HRESULT
WINAPI
WDDMConGetDisplaySize(
    _In_ HANDLE hDisplay,
    _In_ CD_IO_DISPLAY_SIZE *pDisplaySize
    ) noexcept
{
    auto hr = S_OK;
    PWDDMCONSOLECONTEXT pCtx = nullptr;

    if (hDisplay == nullptr) {
        hr = E_INVALIDARG;
    } else {
        pCtx = static_cast<PWDDMCONSOLECONTEXT>(hDisplay);
    }

    if (SUCCEEDED(hr)) {
        *pDisplaySize = pCtx->DisplaySize;
    }

    return hr;
}

HRESULT
WINAPI
WDDMConEnableDisplayAccess(
    _In_ HANDLE hDisplay,
    _In_ BOOLEAN fOutputEnabled
    )
{
    auto hr = S_OK;
    PWDDMCONSOLECONTEXT pCtx = nullptr;

    if (hDisplay == nullptr) {
        hr = E_INVALIDARG;
    } else {
        pCtx = static_cast<PWDDMCONSOLECONTEXT>(hDisplay);
    }

    if (SUCCEEDED(hr) &&
        fOutputEnabled == pCtx->fOutputEnabled) {
        hr = E_NOT_VALID_STATE;
    }

    if (SUCCEEDED(hr)) {
        pCtx->fOutputEnabled = fOutputEnabled;
        if (!fOutputEnabled) {
            ReleaseDeviceResources(pCtx);
        }
    }

    return hr;
}
