// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/ColorScheme.h"
#include "../TerminalSettingsModel/CascadiaSettings.h"
#include "JsonTestClass.h"
#include "TestUtils.h"
#include <defaults.h>

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace SettingsModelLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class SerializationTests : public JsonTestClass
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(SerializationTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(GlobalSettings);
        TEST_METHOD(Profile);
        TEST_METHOD(ColorScheme);
        TEST_METHOD(Actions);
        TEST_METHOD(CascadiaSettings);
        TEST_METHOD(LegacyFontSettings);

    private:
        // Method Description:
        // - deserializes and reserializes a json string representing a settings object model of type T
        // - verifies that the generated json string matches the provided one
        // Template Types:
        // - <T>: The type of Settings Model object to generate (must be impl type)
        // Arguments:
        // - jsonString - JSON string we're performing the test on
        // Return Value:
        // - the JsonObject representing this instance
        template<typename T>
        void RoundtripTest(const std::string_view& jsonString)
        {
            const auto json{ VerifyParseSucceeded(jsonString) };
            const auto settings{ T::FromJson(json) };
            const auto result{ settings->ToJson() };

            // Compare toString(json) instead of jsonString here.
            // The toString writes the json out alphabetically.
            // This trick allows jsonString to _not_ have to be
            // written alphabetically.
            VERIFY_ARE_EQUAL(toString(json), toString(result));
        }
    };

    void SerializationTests::GlobalSettings()
    {
        static constexpr std::string_view globalsString{ R"(
            {
                "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",

                "initialRows": 30,
                "initialCols": 120,
                "initialPosition": ",",
                "launchMode": "default",
                "alwaysOnTop": false,
                "inputServiceWarning": true,
                "copyOnSelect": false,
                "copyFormatting": "all",
                "wordDelimiters": " /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502",

                "alwaysShowTabs": true,
                "showTabsInTitlebar": true,
                "showTerminalTitleInTitlebar": true,
                "tabWidthMode": "equal",
                "tabSwitcherMode": "mru",

                "startOnUserLogin": false,
                "theme": "system",
                "snapToGridOnResize": true,
                "disableAnimations": false,

                "confirmCloseAllTabs": true,
                "largePasteWarning": true,
                "multiLinePasteWarning": true,
                "trimPaste": true,

                "experimental.input.forceVT": false,
                "experimental.rendering.forceFullRepaint": false,
                "experimental.rendering.software": false,

                "actions": []
            })" };

        static constexpr std::string_view smallGlobalsString{ R"(
            {
                "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "actions": []
            })" };

        RoundtripTest<implementation::GlobalAppSettings>(globalsString);
        RoundtripTest<implementation::GlobalAppSettings>(smallGlobalsString);
    }

    void SerializationTests::Profile()
    {
        static constexpr std::string_view profileString{ R"(
            {
                "name": "Windows PowerShell",
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",

                "commandline": "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
                "startingDirectory": "%USERPROFILE%",

                "icon": "ms-appx:///ProfileIcons/{61c54bbd-c2c6-5271-96e7-009a87ff44bf}.png",
                "hidden": false,

                "tabTitle": "Cool Tab",
                "suppressApplicationTitle": false,

                "font": {
                    "face": "Cascadia Mono",
                    "size": 12.0,
                    "weight": "normal"
                },
                "padding": "8, 8, 8, 8",
                "antialiasingMode": "grayscale",

                "cursorShape": "bar",
                "cursorColor": "#CCBBAA",
                "cursorHeight": 10,

                "altGrAliasing": true,

                "colorScheme": "Campbell",
                "tabColor": "#0C0C0C",
                "foreground": "#AABBCC",
                "background": "#BBCCAA",
                "selectionBackground": "#CCAABB",

                "useAcrylic": false,
                "opacity": 50,

                "backgroundImage": "made_you_look.jpeg",
                "backgroundImageStretchMode": "uniformToFill",
                "backgroundImageAlignment": "center",
                "backgroundImageOpacity": 1.0,

                "scrollbarState": "visible",
                "snapOnInput": true,
                "historySize": 9001,

                "closeOnExit": "graceful",
                "experimental.retroTerminalEffect": false
            })" };

        static constexpr std::string_view smallProfileString{ R"(
            {
                "name": "Custom Profile"
            })" };

        // Setting "tabColor" to null tests two things:
        // - null should count as an explicit user-set value, not falling back to the parent's value
        // - null should be acceptable even though we're working with colors
        static constexpr std::string_view weirdProfileString{ R"(
            {
                "guid" : "{8b039d4d-77ca-5a83-88e1-dfc8e895a127}",
                "name": "Weird Profile",
                "hidden": false,
                "tabColor": null,
                "foreground": null,
                "source": "local"
            })" };

        RoundtripTest<implementation::Profile>(profileString);
        RoundtripTest<implementation::Profile>(smallProfileString);
        RoundtripTest<implementation::Profile>(weirdProfileString);
    }

    void SerializationTests::ColorScheme()
    {
        static constexpr std::string_view schemeString{ R"({
                                            "name": "Campbell",

                                            "cursorColor": "#FFFFFF",
                                            "selectionBackground": "#131313",

                                            "background": "#0C0C0C",
                                            "foreground": "#F2F2F2",

                                            "black": "#0C0C0C",
                                            "blue": "#0037DA",
                                            "cyan": "#3A96DD",
                                            "green": "#13A10E",
                                            "purple": "#881798",
                                            "red": "#C50F1F",
                                            "white": "#CCCCCC",
                                            "yellow": "#C19C00",
                                            "brightBlack": "#767676",
                                            "brightBlue": "#3B78FF",
                                            "brightCyan": "#61D6D6",
                                            "brightGreen": "#16C60C",
                                            "brightPurple": "#B4009E",
                                            "brightRed": "#E74856",
                                            "brightWhite": "#F2F2F2",
                                            "brightYellow": "#F9F1A5"
                                        })" };

        RoundtripTest<implementation::ColorScheme>(schemeString);
    }

    void SerializationTests::Actions()
    {
        // simple command
        static constexpr std::string_view actionsString1{ R"([
                                                { "command": "paste" }
                                            ])" };

        // complex command
        static constexpr std::string_view actionsString2A{ R"([
                                                { "command": { "action": "setTabColor" } }
                                            ])" };
        static constexpr std::string_view actionsString2B{ R"([
                                                { "command": { "action": "setTabColor", "color": "#112233" } }
                                            ])" };
        static constexpr std::string_view actionsString2C{ R"([
                                                { "command": { "action": "copy" } },
                                                { "command": { "action": "copy", "singleLine": true, "copyFormatting": "html" } }
                                            ])" };

        // simple command with key chords
        static constexpr std::string_view actionsString3{ R"([
                                                { "command": "toggleAlwaysOnTop", "keys": "ctrl+a" },
                                                { "command": "toggleAlwaysOnTop", "keys": "ctrl+b" }
                                            ])" };

        // complex command with key chords
        static constexpr std::string_view actionsString4A{ R"([
                                                { "command": { "action": "adjustFontSize", "delta": 1.0 }, "keys": "ctrl+c" },
                                                { "command": { "action": "adjustFontSize", "delta": 1.0 }, "keys": "ctrl+d" }
                                            ])" };
        // GH#13323 - these can be fragile. In the past, the order these get
        // re-serialized as has been not entirely stable. We don't really care
        // about the order they get re-serialized in, but the tests aren't
        // clever enough to compare the structure, only the literal string
        // itself. Feel free to change as needed.
        static constexpr std::string_view actionsString4B{ R"([
                                                { "command": { "action": "findMatch", "direction": "prev" }, "keys": "ctrl+shift+r" },
                                                { "command": { "action": "adjustFontSize", "delta": 1.0 }, "keys": "ctrl+d" }
                                            ])" };

        // command with name and icon and multiple key chords
        static constexpr std::string_view actionsString5{ R"([
                                                { "icon": "image.png", "name": "Scroll To Top Name", "command": "scrollToTop", "keys": "ctrl+e" },
                                                { "command": "scrollToTop", "keys": "ctrl+f" }
                                            ])" };

        // complex command with new terminal args
        static constexpr std::string_view actionsString6{ R"([
                                                { "command": { "action": "newTab", "index": 0 }, "keys": "ctrl+g" },
                                            ])" };

        // complex command with meaningful null arg
        static constexpr std::string_view actionsString7{ R"([
                                                { "command": { "action": "renameWindow", "name": null }, "keys": "ctrl+h" }
                                            ])" };

        // nested command
        static constexpr std::string_view actionsString8{ R"([
                                                {
                                                    "name": "Change font size...",
                                                    "commands": [
                                                        { "command": { "action": "adjustFontSize", "delta": 1.0 } },
                                                        { "command": { "action": "adjustFontSize", "delta": -1.0 } },
                                                        { "command": "resetFontSize" },
                                                    ]
                                                }
                                            ])" };

        // iterable command
        static constexpr std::string_view actionsString9A{ R"([
                                                {
                                                    "name": "New tab",
                                                    "commands": [
                                                        {
                                                            "iterateOn": "profiles",
                                                            "icon": "${profile.icon}",
                                                            "name": "${profile.name}",
                                                            "command": { "action": "newTab", "profile": "${profile.name}" }
                                                        }
                                                    ]
                                                }
                                            ])" };
        static constexpr std::string_view actionsString9B{ R"([
                                                {
                                                    "commands":
                                                    [
                                                        {
                                                            "command":
                                                            {
                                                                "action": "sendInput",
                                                                "input": "${profile.name}"
                                                            },
                                                            "iterateOn": "profiles"
                                                        }
                                                    ],
                                                    "name": "Send Input ..."
                                                }
                                        ])" };
        static constexpr std::string_view actionsString9C{ R""([
                                                {
                                                    "commands":
                                                    [
                                                        {
                                                            "commands":
                                                            [
                                                                {
                                                                    "command":
                                                                    {
                                                                        "action": "sendInput",
                                                                        "input": "${profile.name} ${scheme.name}"
                                                                    },
                                                                    "iterateOn": "schemes"
                                                                }
                                                            ],
                                                            "iterateOn": "profiles",
                                                            "name": "nest level (${profile.name})"
                                                        }
                                                    ],
                                                    "name": "Send Input (Evil) ..."
                                                }
                                            ])"" };
        static constexpr std::string_view actionsString9D{ R""([
                                                {
                                                    "command":
                                                    {
                                                        "action": "newTab",
                                                        "profile": "${profile.name}"
                                                    },
                                                    "icon": "${profile.icon}",
                                                    "iterateOn": "profiles",
                                                    "name": "${profile.name}: New tab"
                                                }
                                            ])"" };

        // unbound command
        static constexpr std::string_view actionsString10{ R"([
                                                { "command": "unbound", "keys": "ctrl+c" }
                                            ])" };

        Log::Comment(L"simple command");
        RoundtripTest<implementation::ActionMap>(actionsString1);

        Log::Comment(L"complex commands");
        RoundtripTest<implementation::ActionMap>(actionsString2A);
        RoundtripTest<implementation::ActionMap>(actionsString2B);
        RoundtripTest<implementation::ActionMap>(actionsString2C);

        Log::Comment(L"simple command with key chords");
        RoundtripTest<implementation::ActionMap>(actionsString3);

        Log::Comment(L"complex commands with key chords");
        RoundtripTest<implementation::ActionMap>(actionsString4A);
        RoundtripTest<implementation::ActionMap>(actionsString4B);

        Log::Comment(L"command with name and icon and multiple key chords");
        RoundtripTest<implementation::ActionMap>(actionsString5);

        Log::Comment(L"complex command with new terminal args");
        RoundtripTest<implementation::ActionMap>(actionsString6);

        Log::Comment(L"complex command with meaningful null arg");
        RoundtripTest<implementation::ActionMap>(actionsString7);

        Log::Comment(L"nested command");
        RoundtripTest<implementation::ActionMap>(actionsString8);

        Log::Comment(L"iterable command");
        RoundtripTest<implementation::ActionMap>(actionsString9A);
        RoundtripTest<implementation::ActionMap>(actionsString9B);
        RoundtripTest<implementation::ActionMap>(actionsString9C);
        RoundtripTest<implementation::ActionMap>(actionsString9D);

        Log::Comment(L"unbound command");
        RoundtripTest<implementation::ActionMap>(actionsString10);
    }

    void SerializationTests::CascadiaSettings()
    {
        static constexpr std::string_view settingsString{ R"({
            "$help" : "https://aka.ms/terminal-documentation",
            "$schema" : "https://aka.ms/terminal-profiles-schema",
            "defaultProfile": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
            "disabledProfileSources": [ "Windows.Terminal.Wsl" ],
            "newTabMenu":
            [
                {
                    "type": "remainingProfiles"
                }
            ],
            "profiles": {
                "defaults": {
                    "font": {
                        "face": "Zamora Code"
                    }
                },
                "list": [
                    {
                        "font": { "face": "Cascadia Code" },
                        "guid": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
                        "name": "HowettShell"
                    },
                    {
                        "hidden": true,
                        "guid": "{c08b0496-e71c-5503-b84e-3af7a7a6d2a7}",
                        "name": "BhojwaniShell"
                    },
                    {
                        "antialiasingMode": "aliased",
                        "guid": "{fe9df758-ac22-5c20-922d-c7766cdd13af}",
                        "name": "NiksaShell"
                    }
                ]
            },
            "schemes": [
                {
                    "name": "Cinnamon Roll",

                    "cursorColor": "#FFFFFD",
                    "selectionBackground": "#FFFFFF",

                    "background": "#3C0315",
                    "foreground": "#FFFFFD",

                    "black": "#282A2E",
                    "blue": "#0170C5",
                    "cyan": "#3F8D83",
                    "green": "#76AB23",
                    "purple": "#7D498F",
                    "red": "#BD0940",
                    "white": "#FFFFFD",
                    "yellow": "#E0DE48",
                    "brightBlack": "#676E7A",
                    "brightBlue": "#5C98C5",
                    "brightCyan": "#8ABEB7",
                    "brightGreen": "#B5D680",
                    "brightPurple": "#AC79BB",
                    "brightRed": "#BD6D85",
                    "brightWhite": "#FFFFFD",
                    "brightYellow": "#FFFD76"
                }
            ],
            "actions": [
                { "command": { "action": "sendInput", "input": "VT Griese Mode" }, "keys": "ctrl+k" }
            ],
            "theme": "system",
            "themes": []
        })" };

        const auto settings{ winrt::make_self<implementation::CascadiaSettings>(settingsString) };

        const auto result{ settings->ToJson() };
        VERIFY_ARE_EQUAL(toString(VerifyParseSucceeded(settingsString)), toString(result));
    }

    void SerializationTests::LegacyFontSettings()
    {
        static constexpr std::string_view profileString{ R"(
            {
                "name": "Profile with legacy font settings",

                "fontFace": "Cascadia Mono",
                "fontSize": 12.0,
                "fontWeight": "normal"
            })" };

        static constexpr std::string_view expectedOutput{ R"(
            {
                "name": "Profile with legacy font settings",

                "font": {
                    "face": "Cascadia Mono",
                    "size": 12.0,
                    "weight": "normal"
                }
            })" };

        const auto json{ VerifyParseSucceeded(profileString) };
        const auto settings{ implementation::Profile::FromJson(json) };
        const auto result{ settings->ToJson() };

        const auto jsonOutput{ VerifyParseSucceeded(expectedOutput) };

        VERIFY_ARE_EQUAL(toString(jsonOutput), toString(result));
    }
}
