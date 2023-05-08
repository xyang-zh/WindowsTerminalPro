/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- conareainfo.h

Abstract:
- This module contains the structures for the console IME conversion area
- The conversion area is the overlay on the screen where a user attempts to form
  a string that they would like to insert into the buffer.

Author:
- Michael Niksa (MiNiksa) 10-May-2018

Revision History:
- From pieces of convarea.cpp originally authored by KazuM
--*/

#pragma once

#include "../buffer/out/OutputCell.hpp"
#include "../buffer/out/TextAttribute.hpp"
#include "../renderer/inc/FontInfo.hpp"

class SCREEN_INFORMATION;
class TextBuffer;

// Internal structures and definitions used by the conversion area.
class ConversionAreaBufferInfo final
{
public:
    til::size coordCaBuffer;
    til::inclusive_rect rcViewCaWindow;
    til::point coordConView;

    explicit ConversionAreaBufferInfo(const til::size coordBufferSize);
};

class ConversionAreaInfo final
{
public:
    ConversionAreaInfo(const til::size bufferSize,
                       const til::size windowSize,
                       const TextAttribute& fill,
                       const TextAttribute& popupFill,
                       const FontInfo fontInfo);
    ~ConversionAreaInfo() = default;
    ConversionAreaInfo(const ConversionAreaInfo&) = delete;
    ConversionAreaInfo(ConversionAreaInfo&& other);
    ConversionAreaInfo& operator=(const ConversionAreaInfo&) & = delete;
    ConversionAreaInfo& operator=(ConversionAreaInfo&&) & = delete;

    bool IsHidden() const noexcept;
    void SetHidden(const bool fIsHidden) noexcept;
    void ClearArea() noexcept;

    [[nodiscard]] HRESULT Resize(const til::size newSize) noexcept;

    void SetViewPos(const til::point pos) noexcept;
    void SetWindowInfo(const til::inclusive_rect& view) noexcept;
    void Paint() const noexcept;

    void WriteText(const std::vector<OutputCell>& text, const til::CoordType column);
    void SetAttributes(const TextAttribute& attr);

    const TextBuffer& GetTextBuffer() const noexcept;
    const ConversionAreaBufferInfo& GetAreaBufferInfo() const noexcept;

private:
    ConversionAreaBufferInfo _caInfo;
    std::unique_ptr<SCREEN_INFORMATION> _screenBuffer;
    bool _isHidden;
};
