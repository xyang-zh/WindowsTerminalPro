// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../renderer/inc/RenderEngineBase.hpp"

#include <functional>

#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>

#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <DirectXMath.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

#include <wrl.h>
#include <wrl/client.h>

#include "CustomTextLayout.h"
#include "CustomTextRenderer.h"
#include "DxFontRenderData.h"
#include "DxSoftFont.h"

#include "../../types/inc/Viewport.hpp"

#include <TraceLoggingProvider.h>

TRACELOGGING_DECLARE_PROVIDER(g_hDxRenderProvider);

namespace Microsoft::Console::Render
{
    class DxEngine final : public RenderEngineBase
    {
    public:
        DxEngine();
        ~DxEngine();
        DxEngine(const DxEngine&) = default;
        DxEngine(DxEngine&&) = default;
        DxEngine& operator=(const DxEngine&) = default;
        DxEngine& operator=(DxEngine&&) = default;

        // Used to release device resources so that another instance of
        // conhost can render to the screen (i.e. only one DirectX
        // application may control the screen at a time.)
        [[nodiscard]] HRESULT Enable() noexcept override;
        [[nodiscard]] HRESULT Disable() noexcept;

        [[nodiscard]] HRESULT SetHwnd(const HWND hwnd) noexcept override;

        [[nodiscard]] HRESULT SetWindowSize(const til::size pixels) noexcept override;

        void SetCallback(std::function<void(const HANDLE)> pfn) noexcept override;
        void SetWarningCallback(std::function<void(const HRESULT)> pfn) noexcept override;

        bool GetRetroTerminalEffect() const noexcept override;
        void SetRetroTerminalEffect(bool enable) noexcept override;

        std::wstring_view GetPixelShaderPath() noexcept override;
        void SetPixelShaderPath(std::wstring_view value) noexcept override;

        void SetForceFullRepaintRendering(bool enable) noexcept override;

        void SetSoftwareRendering(bool enable) noexcept override;

        // IRenderEngine Members
        [[nodiscard]] HRESULT Invalidate(const til::rect* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const til::rect* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const til::rect* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const til::point* const pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;

        [[nodiscard]] bool RequiresContinuousRedraw() noexcept override;

        void WaitUntilCanRender() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;

        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT UpdateSoftFont(const std::span<const uint16_t> bitPattern,
                                             const til::size cellSize,
                                             const size_t centeringHint) noexcept override;

        [[nodiscard]] HRESULT PrepareRenderInfo(const RenderFrameInfo& info) noexcept override;

        [[nodiscard]] HRESULT ResetLineTransform() noexcept override;
        [[nodiscard]] HRESULT PrepareLineTransform(const LineRendition lineRendition,
                                                   const til::CoordType targetRow,
                                                   const til::CoordType viewportLeft) noexcept override;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(const std::span<const Cluster> clusters,
                                              const til::point coord,
                                              const bool fTrimLeft,
                                              const bool lineWrapped) noexcept override;

        [[nodiscard]] HRESULT PaintBufferGridLines(GridLineSet const lines, COLORREF const color, size_t const cchLine, til::point const coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                   const RenderSettings& renderSettings,
                                                   const gsl::not_null<IRenderData*> pData,
                                                   const bool usingSoftFont,
                                                   const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(const int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo, const int iDpi) noexcept override;

        [[nodiscard]] HRESULT GetDirtyArea(std::span<const til::rect>& area) noexcept override;

        [[nodiscard]] HRESULT GetFontSize(_Out_ til::size* pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

        [[nodiscard]] ::Microsoft::Console::Types::Viewport GetViewportInCharacters(const ::Microsoft::Console::Types::Viewport& viewInPixels) const noexcept override;
        [[nodiscard]] ::Microsoft::Console::Types::Viewport GetViewportInPixels(const ::Microsoft::Console::Types::Viewport& viewInCharacters) const noexcept override;

        float GetScaling() const noexcept override;

        void SetSelectionBackground(const COLORREF color, const float alpha = 0.5f) noexcept override;
        void SetAntialiasingMode(const D2D1_TEXT_ANTIALIAS_MODE antialiasingMode) noexcept override;
        void EnableTransparentBackground(const bool isTransparent) noexcept override;

        void UpdateHyperlinkHoveredId(const uint16_t hoveredId) noexcept override;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(_In_ const std::wstring_view newTitle) noexcept override;
        [[nodiscard]] HRESULT _PaintTerminalEffects() noexcept;
        [[nodiscard]] bool _FullRepaintNeeded() const noexcept;

    private:
        enum class SwapChainMode
        {
            ForHwnd,
            ForComposition
        };

        SwapChainMode _chainMode;

        HWND _hwndTarget;
        til::size _sizeTarget;
        int _dpi;
        float _scale;
        float _prevScale;

        std::function<void(const HANDLE)> _pfn;
        std::function<void(const HRESULT)> _pfnWarningCallback;

        bool _isEnabled;
        bool _isPainting;

        til::size _displaySizePixels;

        D2D1_COLOR_F _defaultForegroundColor;
        D2D1_COLOR_F _defaultBackgroundColor;

        D2D1_COLOR_F _foregroundColor;
        D2D1_COLOR_F _backgroundColor;
        D2D1_COLOR_F _selectionBackground;

        LineRendition _currentLineRendition;
        D2D1::Matrix3x2F _currentLineTransform;

        uint16_t _hyperlinkHoveredId;

        bool _firstFrame;
        std::pmr::unsynchronized_pool_resource _pool;
        til::pmr::bitmap _invalidMap;
        til::point _invalidScroll;
        bool _allInvalid;

        bool _presentReady;
        std::vector<til::rect> _presentDirty;
        RECT _presentScroll;
        POINT _presentOffset;
        DXGI_PRESENT_PARAMETERS _presentParams;

        static std::atomic<size_t> _tracelogCount;

        wil::unique_handle _swapChainHandle;

        // Device-Independent Resources
        ::Microsoft::WRL::ComPtr<ID2D1Factory1> _d2dFactory;

        ::Microsoft::WRL::ComPtr<IDWriteFactory1> _dwriteFactory;
        ::Microsoft::WRL::ComPtr<CustomTextLayout> _customLayout;
        ::Microsoft::WRL::ComPtr<CustomTextRenderer> _customRenderer;
        ::Microsoft::WRL::ComPtr<ID2D1StrokeStyle> _strokeStyle;
        ::Microsoft::WRL::ComPtr<ID2D1StrokeStyle> _dashStrokeStyle;
        ::Microsoft::WRL::ComPtr<ID2D1StrokeStyle> _hyperlinkStrokeStyle;

        std::unique_ptr<DxFontRenderData> _fontRenderData;
        DxSoftFont _softFont;
        bool _usingSoftFont;

        D2D1_STROKE_STYLE_PROPERTIES _strokeStyleProperties;
        D2D1_STROKE_STYLE_PROPERTIES _dashStrokeStyleProperties;

        // Device-Dependent Resources
        bool _recreateDeviceRequested;
        bool _haveDeviceResources;
        ::Microsoft::WRL::ComPtr<ID3D11Device> _d3dDevice;
        ::Microsoft::WRL::ComPtr<ID3D11DeviceContext> _d3dDeviceContext;

        ::Microsoft::WRL::ComPtr<ID2D1Device> _d2dDevice;
        ::Microsoft::WRL::ComPtr<ID2D1DeviceContext> _d2dDeviceContext;
        ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> _d2dBitmap;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> _d2dBrushForeground;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> _d2dBrushBackground;

        ::Microsoft::WRL::ComPtr<IDXGIFactory2> _dxgiFactory2;
        ::Microsoft::WRL::ComPtr<IDXGIFactoryMedia> _dxgiFactoryMedia;
        ::Microsoft::WRL::ComPtr<IDXGIDevice> _dxgiDevice;
        ::Microsoft::WRL::ComPtr<IDXGISurface> _dxgiSurface;

        DXGI_SWAP_CHAIN_DESC1 _swapChainDesc;
        ::Microsoft::WRL::ComPtr<IDXGISwapChain1> _dxgiSwapChain;
        wil::unique_handle _swapChainFrameLatencyWaitableObject;
        std::unique_ptr<DrawingContext> _drawingContext;

        // Terminal effects resources.

        // Controls if configured terminal effects are enabled
        bool _terminalEffectsEnabled;

        // Experimental and deprecated retro terminal effect
        //  Preserved for backwards compatibility
        //  Implemented in terms of the more generic pixel shader effect
        //  Has precedence over pixel shader effect
        bool _retroTerminalEffect;

        // Experimental and pixel shader effect
        //  Allows user to load a pixel shader from a few presets or from a file path
        std::wstring _pixelShaderPath;
        bool _pixelShaderLoaded{ false };

        std::chrono::steady_clock::time_point _shaderStartTime;

        // DX resources needed for terminal effects
        ::Microsoft::WRL::ComPtr<ID3D11RenderTargetView> _renderTargetView;
        ::Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertexShader;
        ::Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader;
        ::Microsoft::WRL::ComPtr<ID3D11InputLayout> _vertexLayout;
        ::Microsoft::WRL::ComPtr<ID3D11Buffer> _screenQuadVertexBuffer;
        ::Microsoft::WRL::ComPtr<ID3D11Buffer> _pixelShaderSettingsBuffer;
        ::Microsoft::WRL::ComPtr<ID3D11SamplerState> _samplerState;
        ::Microsoft::WRL::ComPtr<ID3D11Texture2D> _framebufferCapture;

        // Preferences and overrides
        bool _softwareRendering;
        bool _forceFullRepaintRendering;

        D2D1_TEXT_ANTIALIAS_MODE _antialiasingMode;

        bool _defaultBackgroundIsTransparent;

        // DirectX constant buffers need to be a multiple of 16; align to pad the size.
        __declspec(align(16)) struct
        {
            // Note: This can be seen as API endpoint towards user provided pixel shaders.
            //  Changes here can break existing pixel shaders so be careful with changing datatypes
            //  and order of parameters
            float Time;
            float Scale;
            DirectX::XMFLOAT2 Resolution;
            DirectX::XMFLOAT4 Background;
#pragma warning(suppress : 4324) // structure was padded due to __declspec(align())
        } _pixelShaderSettings;

        [[nodiscard]] HRESULT _CreateDeviceResources(const bool createSwapChain) noexcept;
        [[nodiscard]] HRESULT _CreateSurfaceHandle() noexcept;

        bool _HasTerminalEffects() const noexcept;
        std::string _LoadPixelShaderFile() const;
        HRESULT _SetupTerminalEffects();
        void _ComputePixelShaderSettings() noexcept;

        [[nodiscard]] HRESULT _PrepareRenderTarget() noexcept;

        void _ReleaseDeviceResources() noexcept;

        bool _ShouldForceGrayscaleAA() noexcept;

        [[nodiscard]] HRESULT _CreateTextLayout(
            _In_reads_(StringLength) PCWCHAR String,
            _In_ size_t StringLength,
            _Out_ IDWriteTextLayout** ppTextLayout) noexcept;

        [[nodiscard]] HRESULT _CopyFrontToBack() noexcept;

        [[nodiscard]] HRESULT _EnableDisplayAccess(const bool outputEnabled) noexcept;

        [[nodiscard]] til::size _GetClientSize() const;

        void _InvalidateRectangle(const til::rect& rc);
        bool _IsAllInvalid() const noexcept;

        [[nodiscard]] D2D1_COLOR_F _ColorFFromColorRef(const COLORREF color) noexcept;

        // Routine Description:
        // - Helps convert a Direct2D ColorF into a DXGI RGBA
        // Arguments:
        // - color - Direct2D Color F
        // Return Value:
        // - DXGI RGBA
        [[nodiscard]] constexpr DXGI_RGBA s_RgbaFromColorF(const D2D1_COLOR_F color) noexcept
        {
            return { color.r, color.g, color.b, color.a };
        }
    };
}
