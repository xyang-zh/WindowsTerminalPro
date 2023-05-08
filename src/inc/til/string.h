// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    _TIL_INLINEPREFIX std::wstring visualize_control_codes(std::wstring str) noexcept
    {
        for (auto& ch : str)
        {
            if (ch < 0x20)
            {
                ch += 0x2400;
            }
            else if (ch == 0x20)
            {
                ch = 0x2423; // replace space with ␣
            }
            else if (ch == 0x7f)
            {
                ch = 0x2421; // replace del with ␡
            }
        }
        return str;
    }

    _TIL_INLINEPREFIX std::wstring visualize_control_codes(std::wstring_view str)
    {
        return visualize_control_codes(std::wstring{ str });
    }

    namespace details
    {
        inline constexpr uint8_t __ = 0b00;
        inline constexpr uint8_t F_ = 0b10; // stripped in clean_filename
        inline constexpr uint8_t _P = 0b01; // stripped in clean_path
        inline constexpr uint8_t FP = 0b11; // stripped in clean_filename and clean_path
        static constexpr std::array<uint8_t, 128> pathFilter{ {
            // clang-format off
            __ /* NUL */, __ /* SOH */, __ /* STX */, __ /* ETX */, __ /* EOT */, __ /* ENQ */, __ /* ACK */, __ /* BEL */, __ /* BS  */, __ /* HT  */, __ /* LF  */, __ /* VT  */, __ /* FF  */, __ /* CR  */, __ /* SO  */, __ /* SI  */,
            __ /* DLE */, __ /* DC1 */, __ /* DC2 */, __ /* DC3 */, __ /* DC4 */, __ /* NAK */, __ /* SYN */, __ /* ETB */, __ /* CAN */, __ /* EM  */, __ /* SUB */, __ /* ESC */, __ /* FS  */, __ /* GS  */, __ /* RS  */, __ /* US  */,
            __ /* SP  */, __ /* !   */, FP /* "   */, __ /* #   */, __ /* $   */, __ /* %   */, __ /* &   */, __ /* '   */, __ /* (   */, __ /* )   */, FP /* *   */, __ /* +   */, __ /* ,   */, __ /* -   */, __ /* .   */, F_ /* /   */,
            __ /* 0   */, __ /* 1   */, __ /* 2   */, __ /* 3   */, __ /* 4   */, __ /* 5   */, __ /* 6   */, __ /* 7   */, __ /* 8   */, __ /* 9   */, F_ /* :   */, __ /* ;   */, FP /* <   */, __ /* =   */, FP /* >   */, FP /* ?   */,
            __ /* @   */, __ /* A   */, __ /* B   */, __ /* C   */, __ /* D   */, __ /* E   */, __ /* F   */, __ /* G   */, __ /* H   */, __ /* I   */, __ /* J   */, __ /* K   */, __ /* L   */, __ /* M   */, __ /* N   */, __ /* O   */,
            __ /* P   */, __ /* Q   */, __ /* R   */, __ /* S   */, __ /* T   */, __ /* U   */, __ /* V   */, __ /* W   */, __ /* X   */, __ /* Y   */, __ /* Z   */, __ /* [   */, F_ /* \   */, __ /* ]   */, __ /* ^   */, __ /* _   */,
            __ /* `   */, __ /* a   */, __ /* b   */, __ /* c   */, __ /* d   */, __ /* e   */, __ /* f   */, __ /* g   */, __ /* h   */, __ /* i   */, __ /* j   */, __ /* k   */, __ /* l   */, __ /* m   */, __ /* n   */, __ /* o   */,
            __ /* p   */, __ /* q   */, __ /* r   */, __ /* s   */, __ /* t   */, __ /* u   */, __ /* v   */, __ /* w   */, __ /* x   */, __ /* y   */, __ /* z   */, __ /* {   */, FP /* |   */, __ /* }   */, __ /* ~   */, __ /* DEL */,
            // clang-format on
        } };
    }

    _TIL_INLINEPREFIX std::wstring clean_filename(std::wstring str) noexcept
    {
        using namespace til::details;
        std::erase_if(str, [](auto ch) {
            // This lookup is branchless: It always checks the filter, but throws
            // away the result if ch >= 128. This is faster than using `&&` (branchy).
            return ((til::at(details::pathFilter, ch & 127) & F_) != 0) & (ch < 128);
        });
        return str;
    }

    _TIL_INLINEPREFIX std::wstring clean_path(std::wstring str) noexcept
    {
        using namespace til::details;
        std::erase_if(str, [](auto ch) {
            return ((til::at(details::pathFilter, ch & 127) & _P) != 0) & (ch < 128);
        });
        return str;
    }

    // is_legal_path rules on whether a path contains any non-path characters.
    // it **DOES NOT** rule on whether a path exists.
    _TIL_INLINEPREFIX constexpr bool is_legal_path(const std::wstring_view str) noexcept
    {
        using namespace til::details;
        return !std::any_of(std::begin(str), std::end(str), [](auto&& ch) {
            return ((til::at(details::pathFilter, ch & 127) & _P) != 0) & (ch < 128);
        });
    }

    // std::string_view::starts_with support for C++17.
    template<typename T, typename Traits>
    constexpr bool starts_with(const std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& prefix) noexcept
    {
        return str.size() >= prefix.size() && __builtin_memcmp(str.data(), prefix.data(), prefix.size() * sizeof(T)) == 0;
    }

    constexpr bool starts_with(const std::string_view& str, const std::string_view& prefix) noexcept
    {
        return starts_with<>(str, prefix);
    }

    constexpr bool starts_with(const std::wstring_view& str, const std::wstring_view& prefix) noexcept
    {
        return starts_with<>(str, prefix);
    }

    // std::string_view::ends_with support for C++17.
    template<typename T, typename Traits>
    constexpr bool ends_with(const std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& suffix) noexcept
    {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
        return str.size() >= suffix.size() && __builtin_memcmp(str.data() + (str.size() - suffix.size()), suffix.data(), suffix.size() * sizeof(T)) == 0;
    }

    constexpr bool ends_with(const std::string_view& str, const std::string_view& prefix) noexcept
    {
        return ends_with<>(str, prefix);
    }

    constexpr bool ends_with(const std::wstring_view& str, const std::wstring_view& prefix) noexcept
    {
        return ends_with<>(str, prefix);
    }

    inline constexpr unsigned long to_ulong_error = ULONG_MAX;

    // Just like std::wcstoul, but without annoying locales and null-terminating strings.
    // It has been fuzz-tested against clang's strtoul implementation.
    template<typename T, typename Traits>
    _TIL_INLINEPREFIX constexpr unsigned long to_ulong(const std::basic_string_view<T, Traits>& str, unsigned long base = 0) noexcept
    {
        static constexpr unsigned long maximumValue = ULONG_MAX / 16;

        // We don't have to test ptr for nullability, as we only access it under either condition:
        // * str.length() > 0, for determining the base
        // * ptr != end, when parsing the characters; if ptr is null, length will be 0 and thus end == ptr
#pragma warning(push)
#pragma warning(disable : 26429) // Symbol 'ptr' is never tested for nullness, it can be marked as not_null
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead
        auto ptr = str.data();
        const auto end = ptr + str.length();
        unsigned long accumulator = 0;
        unsigned long value = ULONG_MAX;

        if (!base)
        {
            base = 10;

            if (str.length() > 1 && *ptr == '0')
            {
                base = 8;
                ++ptr;

                if (str.length() > 2 && (*ptr == 'x' || *ptr == 'X'))
                {
                    base = 16;
                    ++ptr;
                }
            }
        }

        if (ptr == end)
        {
            return to_ulong_error;
        }

        for (;; accumulator *= base)
        {
            value = ULONG_MAX;
            if (*ptr >= '0' && *ptr <= '9')
            {
                value = *ptr - '0';
            }
            else if (*ptr >= 'A' && *ptr <= 'F')
            {
                value = *ptr - 'A' + 10;
            }
            else if (*ptr >= 'a' && *ptr <= 'f')
            {
                value = *ptr - 'a' + 10;
            }
            else
            {
                return to_ulong_error;
            }

            accumulator += value;
            if (accumulator >= maximumValue)
            {
                return to_ulong_error;
            }

            if (++ptr == end)
            {
                return accumulator;
            }
        }
#pragma warning(pop)
    }

    constexpr unsigned long to_ulong(const std::string_view& str, unsigned long base = 0) noexcept
    {
        return to_ulong<>(str, base);
    }

    constexpr unsigned long to_ulong(const std::wstring_view& str, unsigned long base = 0) noexcept
    {
        return to_ulong<>(str, base);
    }

    // Just like std::tolower, but without annoying locales.
    template<typename T>
    constexpr T tolower_ascii(T c)
    {
        if ((c >= 'A') && (c <= 'Z'))
        {
            c |= 0x20;
        }

        return c;
    }

    // Just like std::toupper, but without annoying locales.
    template<typename T>
    constexpr T toupper_ascii(T c)
    {
        if ((c >= 'a') && (c <= 'z'))
        {
            c &= ~0x20;
        }

        return c;
    }

    // Just like std::wstring_view::operator==().
    //
    // At the time of writing wmemcmp() is not an intrinsic for MSVC,
    // but the STL uses it to implement wide string comparisons.
    // This produces 3x the assembly _per_ comparison and increases
    // runtime by 2-3x for strings of medium length (16 characters)
    // and 5x or more for long strings (128 characters or more).
    // See: https://github.com/microsoft/STL/issues/2289
    template<typename T, typename Traits>
    bool equals(const std::basic_string_view<T, Traits>& lhs, const std::basic_string_view<T, Traits>& rhs) noexcept
    {
        return lhs.size() == rhs.size() && __builtin_memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(T)) == 0;
    }

    // Just like _memicmp, but without annoying locales.
    template<typename T, typename Traits>
    bool equals_insensitive_ascii(const std::basic_string_view<T, Traits>& str1, const std::basic_string_view<T, Traits>& str2) noexcept
    {
        if (str1.size() != str2.size())
        {
            return false;
        }

#pragma warning(push)
#pragma warning(disable : 26429) // Symbol 'data1' is never tested for nullness, it can be marked as not_null
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead
        auto remaining = str1.size();
        auto data1 = str1.data();
        auto data2 = str2.data();
        for (; remaining; --remaining, ++data1, ++data2)
        {
            if (*data1 != *data2 && tolower_ascii(*data1) != tolower_ascii(*data2))
            {
                return false;
            }
        }
#pragma warning(pop)

        return true;
    }

    inline bool equals_insensitive_ascii(const std::string_view& str1, const std::string_view& str2) noexcept
    {
        return equals_insensitive_ascii<>(str1, str2);
    }

    inline bool equals_insensitive_ascii(const std::wstring_view& str1, const std::wstring_view& str2) noexcept
    {
        return equals_insensitive_ascii<>(str1, str2);
    }

    template<typename T, typename Traits>
    constexpr bool starts_with_insensitive_ascii(const std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& prefix) noexcept
    {
        return str.size() >= prefix.size() && equals_insensitive_ascii<>({ str.data(), prefix.size() }, prefix);
    }

    constexpr bool starts_with_insensitive_ascii(const std::string_view& str, const std::string_view& prefix) noexcept
    {
        return starts_with_insensitive_ascii<>(str, prefix);
    }

    constexpr bool starts_with_insensitive_ascii(const std::wstring_view& str, const std::wstring_view& prefix) noexcept
    {
        return starts_with_insensitive_ascii<>(str, prefix);
    }

    template<typename T, typename Traits>
    constexpr bool ends_with_insensitive_ascii(const std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& suffix) noexcept
    {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
        return str.size() >= suffix.size() && equals_insensitive_ascii<>({ str.data() - suffix.size(), suffix.size() }, suffix);
    }

    constexpr bool ends_with_insensitive_ascii(const std::string_view& str, const std::string_view& prefix) noexcept
    {
        return ends_with_insensitive_ascii<>(str, prefix);
    }

    constexpr bool ends_with_insensitive_ascii(const std::wstring_view& str, const std::wstring_view& prefix) noexcept
    {
        return ends_with<>(str, prefix);
    }

    // Give the arguments ("foo bar baz", " "), this method will
    // * modify the first argument to "bar baz"
    // * return "foo"
    // If the needle cannot be found the "str" argument is returned as is.
    template<typename T, typename Traits>
    std::basic_string_view<T, Traits> prefix_split(std::basic_string_view<T, Traits>& str, const std::basic_string_view<T, Traits>& needle) noexcept
    {
        using view_type = std::basic_string_view<T, Traits>;

        const auto idx = str.find(needle);
        // > If the needle cannot be found the "str" argument is returned as is.
        // ...but if needle is empty, idx will always be npos, forcing us to return str.
        if (idx == view_type::npos || needle.empty())
        {
            return std::exchange(str, {});
        }

        const auto suffixIdx = idx + needle.size();
        const view_type result{ str.data(), idx };
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead
        str = { str.data() + suffixIdx, str.size() - suffixIdx };
        return result;
    }

    inline std::string_view prefix_split(std::string_view& str, const std::string_view& needle) noexcept
    {
        return prefix_split<>(str, needle);
    }

    inline std::wstring_view prefix_split(std::wstring_view& str, const std::wstring_view& needle) noexcept
    {
        return prefix_split<>(str, needle);
    }
}
