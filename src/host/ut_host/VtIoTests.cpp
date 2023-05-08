// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <wextestclass.h>
#include "../../inc/consoletaeftemplates.hpp"
#include "../../types/inc/Viewport.hpp"

#include "../VtIo.hpp"
#include "../../interactivity/inc/ServiceLocator.hpp"
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/vt/Xterm256Engine.hpp"
#include "../../renderer/vt/XtermEngine.hpp"

#if TIL_FEATURE_CONHOSTDXENGINE_ENABLED
#include "../../renderer/dx/DxRenderer.hpp"
#endif

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace Microsoft::Console::Interactivity;

class Microsoft::Console::VirtualTerminal::VtIoTests
{
    BEGIN_TEST_CLASS(VtIoTests)
        TEST_CLASS_PROPERTY(L"IsolationLevel", L"Class")
    END_TEST_CLASS()

    // General Tests:
    TEST_METHOD(NoOpStartTest);
    TEST_METHOD(ModeParsingTest);

    TEST_METHOD(DtorTestJustEngine);
    TEST_METHOD(DtorTestDeleteVtio);
    TEST_METHOD(DtorTestStackAlloc);
    TEST_METHOD(DtorTestStackAllocMany);

    TEST_METHOD(RendererDtorAndThread);

#if TIL_FEATURE_CONHOSTDXENGINE_ENABLED
    TEST_METHOD(RendererDtorAndThreadAndDx);
#endif

    TEST_METHOD(BasicAnonymousPipeOpeningWithSignalChannelTest);
};

using namespace Microsoft::Console;
using namespace Microsoft::Console::VirtualTerminal;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

void VtIoTests::NoOpStartTest()
{
    VtIo vtio;
    VERIFY_IS_FALSE(vtio.IsUsingVt());

    Log::Comment(L"Verify we succeed at StartIfNeeded even if we weren't initialized");
    VERIFY_SUCCEEDED(vtio.StartIfNeeded());
}

void VtIoTests::ModeParsingTest()
{
    VtIoMode mode;
    VERIFY_SUCCEEDED(VtIo::ParseIoMode(L"xterm", mode));
    VERIFY_ARE_EQUAL(mode, VtIoMode::XTERM);

    VERIFY_SUCCEEDED(VtIo::ParseIoMode(L"xterm-256color", mode));
    VERIFY_ARE_EQUAL(mode, VtIoMode::XTERM_256);

    VERIFY_SUCCEEDED(VtIo::ParseIoMode(L"xterm-ascii", mode));
    VERIFY_ARE_EQUAL(mode, VtIoMode::XTERM_ASCII);

    VERIFY_SUCCEEDED(VtIo::ParseIoMode(L"", mode));
    VERIFY_ARE_EQUAL(mode, VtIoMode::XTERM_256);

    VERIFY_FAILED(VtIo::ParseIoMode(L"garbage", mode));
    VERIFY_ARE_EQUAL(mode, VtIoMode::INVALID);
}

Viewport SetUpViewport()
{
    til::inclusive_rect view;
    view.top = view.left = 0;
    view.bottom = 31;
    view.right = 79;

    return Viewport::FromInclusive(view);
}

void VtIoTests::DtorTestJustEngine()
{
    Log::Comment(NoThrowString().Format(
        L"This test is going to instantiate a bunch of VtIos in different \n"
        L"scenarios to see if something causes a weird cleanup.\n"
        L"It's here because of the strange nature of VtEngine having members\n"
        L"that are only defined in UNIT_TESTING"));

    Log::Comment(NoThrowString().Format(
        L"New some engines and delete them"));
    for (auto i = 0; i < 25; ++i)
    {
        Log::Comment(NoThrowString().Format(
            L"New/Delete loop #%d", i));

        wil::unique_hfile hOutputFile;
        hOutputFile.reset(INVALID_HANDLE_VALUE);
        auto pRenderer256 = new Xterm256Engine(std::move(hOutputFile), SetUpViewport());
        Log::Comment(NoThrowString().Format(L"Made Xterm256Engine"));
        delete pRenderer256;
        Log::Comment(NoThrowString().Format(L"Deleted."));

        hOutputFile.reset(INVALID_HANDLE_VALUE);

        auto pRenderEngineXterm = new XtermEngine(std::move(hOutputFile), SetUpViewport(), false);
        Log::Comment(NoThrowString().Format(L"Made XtermEngine"));
        delete pRenderEngineXterm;
        Log::Comment(NoThrowString().Format(L"Deleted."));

        hOutputFile.reset(INVALID_HANDLE_VALUE);

        auto pRenderEngineXtermAscii = new XtermEngine(std::move(hOutputFile), SetUpViewport(), true);
        Log::Comment(NoThrowString().Format(L"Made XtermEngine"));
        delete pRenderEngineXtermAscii;
        Log::Comment(NoThrowString().Format(L"Deleted."));
    }
}

void VtIoTests::DtorTestDeleteVtio()
{
    Log::Comment(NoThrowString().Format(
        L"This test is going to instantiate a bunch of VtIos in different \n"
        L"scenarios to see if something causes a weird cleanup.\n"
        L"It's here because of the strange nature of VtEngine having members\n"
        L"that are only defined in UNIT_TESTING"));

    Log::Comment(NoThrowString().Format(
        L"New some engines and delete them"));
    for (auto i = 0; i < 25; ++i)
    {
        Log::Comment(NoThrowString().Format(
            L"New/Delete loop #%d", i));

        auto hOutputFile = wil::unique_hfile(INVALID_HANDLE_VALUE);

        hOutputFile.reset(INVALID_HANDLE_VALUE);

        auto vtio = new VtIo();
        Log::Comment(NoThrowString().Format(L"Made VtIo"));
        vtio->_pVtRenderEngine = std::make_unique<Xterm256Engine>(std::move(hOutputFile),
                                                                  SetUpViewport());
        Log::Comment(NoThrowString().Format(L"Made Xterm256Engine"));
        delete vtio;
        Log::Comment(NoThrowString().Format(L"Deleted."));

        hOutputFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
        vtio = new VtIo();
        Log::Comment(NoThrowString().Format(L"Made VtIo"));
        vtio->_pVtRenderEngine = std::make_unique<XtermEngine>(std::move(hOutputFile),
                                                               SetUpViewport(),
                                                               false);
        Log::Comment(NoThrowString().Format(L"Made XtermEngine"));
        delete vtio;
        Log::Comment(NoThrowString().Format(L"Deleted."));

        hOutputFile = wil::unique_hfile(INVALID_HANDLE_VALUE);
        vtio = new VtIo();
        Log::Comment(NoThrowString().Format(L"Made VtIo"));
        vtio->_pVtRenderEngine = std::make_unique<XtermEngine>(std::move(hOutputFile),
                                                               SetUpViewport(),
                                                               true);
        Log::Comment(NoThrowString().Format(L"Made XtermEngine"));
        delete vtio;
        Log::Comment(NoThrowString().Format(L"Deleted."));
    }
}

void VtIoTests::DtorTestStackAlloc()
{
    Log::Comment(NoThrowString().Format(
        L"This test is going to instantiate a bunch of VtIos in different \n"
        L"scenarios to see if something causes a weird cleanup.\n"
        L"It's here because of the strange nature of VtEngine having members\n"
        L"that are only defined in UNIT_TESTING"));

    Log::Comment(NoThrowString().Format(
        L"make some engines and let them fall out of scope"));
    for (auto i = 0; i < 25; ++i)
    {
        Log::Comment(NoThrowString().Format(
            L"Scope Exit Auto cleanup #%d", i));

        wil::unique_hfile hOutputFile;

        hOutputFile.reset(INVALID_HANDLE_VALUE);
        {
            VtIo vtio;
            vtio._pVtRenderEngine = std::make_unique<Xterm256Engine>(std::move(hOutputFile),
                                                                     SetUpViewport());
        }

        hOutputFile.reset(INVALID_HANDLE_VALUE);
        {
            VtIo vtio;
            vtio._pVtRenderEngine = std::make_unique<XtermEngine>(std::move(hOutputFile),
                                                                  SetUpViewport(),
                                                                  false);
        }

        hOutputFile.reset(INVALID_HANDLE_VALUE);
        {
            VtIo vtio;
            vtio._pVtRenderEngine = std::make_unique<XtermEngine>(std::move(hOutputFile),
                                                                  SetUpViewport(),
                                                                  true);
        }
    }
}

void VtIoTests::DtorTestStackAllocMany()
{
    Log::Comment(NoThrowString().Format(
        L"This test is going to instantiate a bunch of VtIos in different \n"
        L"scenarios to see if something causes a weird cleanup.\n"
        L"It's here because of the strange nature of VtEngine having members\n"
        L"that are only defined in UNIT_TESTING"));

    Log::Comment(NoThrowString().Format(
        L"Try an make a whole bunch all at once, and have them all fall out of scope at once."));
    for (auto i = 0; i < 25; ++i)
    {
        Log::Comment(NoThrowString().Format(
            L"Multiple engines, one scope loop #%d", i));

        wil::unique_hfile hOutputFile;
        {
            hOutputFile.reset(INVALID_HANDLE_VALUE);
            VtIo vtio1;
            vtio1._pVtRenderEngine = std::make_unique<Xterm256Engine>(std::move(hOutputFile),
                                                                      SetUpViewport());

            hOutputFile.reset(INVALID_HANDLE_VALUE);
            VtIo vtio2;
            vtio2._pVtRenderEngine = std::make_unique<XtermEngine>(std::move(hOutputFile),
                                                                   SetUpViewport(),
                                                                   false);

            hOutputFile.reset(INVALID_HANDLE_VALUE);
            VtIo vtio3;
            vtio3._pVtRenderEngine = std::make_unique<XtermEngine>(std::move(hOutputFile),
                                                                   SetUpViewport(),
                                                                   true);
        }
    }
}

class MockRenderData : public IRenderData
{
public:
    Microsoft::Console::Types::Viewport GetViewport() noexcept override
    {
        return Microsoft::Console::Types::Viewport{};
    }

    til::point GetTextBufferEndPosition() const noexcept override
    {
        return {};
    }

    const TextBuffer& GetTextBuffer() const noexcept override
    {
        FAIL_FAST_HR(E_NOTIMPL);
    }

    const FontInfo& GetFontInfo() const noexcept override
    {
        FAIL_FAST_HR(E_NOTIMPL);
    }

    std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept override
    {
        return std::vector<Microsoft::Console::Types::Viewport>{};
    }

    void LockConsole() noexcept override
    {
    }

    void UnlockConsole() noexcept override
    {
    }

    std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& /*attr*/) const noexcept override
    {
        return std::make_pair(COLORREF{}, COLORREF{});
    }

    til::point GetCursorPosition() const noexcept override
    {
        return {};
    }

    bool IsCursorVisible() const noexcept override
    {
        return false;
    }

    bool IsCursorOn() const noexcept override
    {
        return false;
    }

    ULONG GetCursorHeight() const noexcept override
    {
        return 42ul;
    }

    CursorType GetCursorStyle() const noexcept override
    {
        return CursorType::FullBox;
    }

    ULONG GetCursorPixelWidth() const noexcept override
    {
        return 12ul;
    }

    bool IsCursorDoubleWidth() const noexcept override
    {
        return false;
    }

    const std::vector<RenderOverlay> GetOverlays() const noexcept override
    {
        return std::vector<RenderOverlay>{};
    }

    const bool IsGridLineDrawingAllowed() noexcept override
    {
        return false;
    }

    const std::wstring_view GetConsoleTitle() const noexcept override
    {
        return std::wstring_view{};
    }

    const bool IsSelectionActive() const override
    {
        return false;
    }

    const bool IsBlockSelection() const noexcept override
    {
        return false;
    }

    void ClearSelection() override
    {
    }

    void SelectNewRegion(const til::point /*coordStart*/, const til::point /*coordEnd*/) override
    {
    }

    const til::point GetSelectionAnchor() const noexcept
    {
        return {};
    }

    const til::point GetSelectionEnd() const noexcept
    {
        return {};
    }

    void ColorSelection(const til::point /*coordSelectionStart*/, const til::point /*coordSelectionEnd*/, const TextAttribute /*attr*/)
    {
    }

    const bool IsUiaDataInitialized() const noexcept
    {
        return true;
    }

    const std::wstring GetHyperlinkUri(uint16_t /*id*/) const
    {
        return {};
    }

    const std::wstring GetHyperlinkCustomId(uint16_t /*id*/) const
    {
        return {};
    }

    const std::vector<size_t> GetPatternId(const til::point /*location*/) const
    {
        return {};
    }
};

void VtIoTests::RendererDtorAndThread()
{
    Log::Comment(NoThrowString().Format(
        L"Test deleting a Renderer a bunch of times"));

    for (auto i = 0; i < 16; ++i)
    {
        auto data = std::make_unique<MockRenderData>();
        auto thread = std::make_unique<Microsoft::Console::Render::RenderThread>();
        auto* pThread = thread.get();
        auto pRenderer = std::make_unique<Microsoft::Console::Render::Renderer>(RenderSettings{}, data.get(), nullptr, 0, std::move(thread));
        VERIFY_SUCCEEDED(pThread->Initialize(pRenderer.get()));
        // Sleep for a hot sec to make sure the thread starts before we enable painting
        // If you don't, the thread might wait on the paint enabled event AFTER
        // EnablePainting gets called, and if that happens, then the thread will
        // never get destructed. This will only ever happen in the vstest test runner,
        // which is what CI uses.
        /*Sleep(500);*/

        pThread->EnablePainting();
        pRenderer->TriggerTeardown();
        pRenderer.reset();
    }
}

#if TIL_FEATURE_CONHOSTDXENGINE_ENABLED
void VtIoTests::RendererDtorAndThreadAndDx()
{
    Log::Comment(NoThrowString().Format(
        L"Test deleting a Renderer a bunch of times"));

    for (auto i = 0; i < 16; ++i)
    {
        auto data = std::make_unique<MockRenderData>();
        auto thread = std::make_unique<Microsoft::Console::Render::RenderThread>();
        auto* pThread = thread.get();
        auto pRenderer = std::make_unique<Microsoft::Console::Render::Renderer>(RenderSettings{}, data.get(), nullptr, 0, std::move(thread));
        VERIFY_SUCCEEDED(pThread->Initialize(pRenderer.get()));

        auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
        pRenderer->AddRenderEngine(dxEngine.get());
        // Sleep for a hot sec to make sure the thread starts before we enable painting
        // If you don't, the thread might wait on the paint enabled event AFTER
        // EnablePainting gets called, and if that happens, then the thread will
        // never get destructed. This will only ever happen in the vstest test runner,
        // which is what CI uses.
        /*Sleep(500);*/

        (void)dxEngine->Enable();
        pThread->EnablePainting();
        pRenderer->TriggerTeardown();
        pRenderer.reset();
    }
}
#endif

void VtIoTests::BasicAnonymousPipeOpeningWithSignalChannelTest()
{
    Log::Comment(L"Test using anonymous pipes for the input and adding a signal channel.");

    Log::Comment(L"\tcreating pipes");

    wil::unique_handle inPipeReadSide;
    wil::unique_handle inPipeWriteSide;
    wil::unique_handle outPipeReadSide;
    wil::unique_handle outPipeWriteSide;
    wil::unique_handle signalPipeReadSide;
    wil::unique_handle signalPipeWriteSide;

    VERIFY_WIN32_BOOL_SUCCEEDED(CreatePipe(&inPipeReadSide, &inPipeWriteSide, nullptr, 0), L"Create anonymous in pipe.");
    VERIFY_WIN32_BOOL_SUCCEEDED(CreatePipe(&outPipeReadSide, &outPipeWriteSide, nullptr, 0), L"Create anonymous out pipe.");
    VERIFY_WIN32_BOOL_SUCCEEDED(CreatePipe(&signalPipeReadSide, &signalPipeWriteSide, nullptr, 0), L"Create anonymous signal pipe.");

    Log::Comment(L"\tinitializing vtio");

    // CreateIoHandlers() assert()s on IsConsoleLocked() to guard against a race condition.
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole();
    const auto cleanup = wil::scope_exit([&]() {
        gci.UnlockConsole();
    });

    VtIo vtio;
    VERIFY_IS_FALSE(vtio.IsUsingVt());
    VERIFY_ARE_EQUAL(nullptr, vtio._pPtySignalInputThread);
    VERIFY_SUCCEEDED(vtio._Initialize(inPipeReadSide.release(), outPipeWriteSide.release(), L"", signalPipeReadSide.release()));
    VERIFY_SUCCEEDED(vtio.CreateAndStartSignalThread());
    VERIFY_SUCCEEDED(vtio.CreateIoHandlers());
    VERIFY_IS_TRUE(vtio.IsUsingVt());
    VERIFY_ARE_NOT_EQUAL(nullptr, vtio._pPtySignalInputThread);
}
