// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <precomp.h>

#include "telemetry.hpp"

#pragma warning(push)
#pragma warning(disable : 26494) // _Tlgdata uninitialized from TraceLoggingWrite
#pragma warning(disable : 26477) // Use nullptr instead of NULL or 0 from TraceLoggingWrite
#pragma warning(disable : 26485) // _Tlgdata, no array to pointer decay from TraceLoggingWrite
#pragma warning(disable : 26446) // Prefer gsl::at over unchecked subscript from TraceLoggingLevel
#pragma warning(disable : 26482) // Only index to arrays with constant expressions from TraceLoggingLevel

TRACELOGGING_DEFINE_PROVIDER(g_hConsoleVirtTermParserEventTraceProvider,
                             "Microsoft.Windows.Console.VirtualTerminal.Parser",
                             // {c9ba2a84-d3ca-5e19-2bd6-776a0910cb9d}
                             (0xc9ba2a84, 0xd3ca, 0x5e19, 0x2b, 0xd6, 0x77, 0x6a, 0x09, 0x10, 0xcb, 0x9d));

using namespace Microsoft::Console::VirtualTerminal;

#pragma warning(push)
// Disable 4351 so we can initialize the arrays to 0 without a warning.
#pragma warning(disable : 4351)
TermTelemetry::TermTelemetry() noexcept :
    _uiTimesUsedCurrent(0),
    _uiTimesFailedCurrent(0),
    _uiTimesFailedOutsideRangeCurrent(0),
    _uiTimesUsed(),
    _uiTimesFailed(),
    _uiTimesFailedOutsideRange(0),
    _activityId(),
    _fShouldWriteFinalLog(false)
{
    TraceLoggingRegister(g_hConsoleVirtTermParserEventTraceProvider);

    // Create a random activityId just in case it doesn't get set later in SetActivityId().
    EventActivityIdControl(EVENT_ACTIVITY_CTRL_CREATE_ID, &_activityId);
}
#pragma warning(pop)

TermTelemetry::~TermTelemetry()
{
    try
    {
        WriteFinalTraceLog();
        TraceLoggingUnregister(g_hConsoleVirtTermParserEventTraceProvider);
    }
    CATCH_LOG()
}

// Routine Description:
// - Logs the usage of a particular VT100 code.
//
// Arguments:
// - code - VT100 code.
// Return Value:
// - <none>
void TermTelemetry::Log(const Codes code) noexcept
{
    // Initially we wanted to pass over a string (ex. "CUU") and use a dictionary data type to hold the counts.
    // However we would have to search through the dictionary every time we called this method, so we decided
    // to use an array which has very quick access times.
    // The downside is we have to create an enum type, and then convert them to strings when we finally
    // send out the telemetry, but the upside is we should have very good performance.
    _uiTimesUsed[code]++;
    _uiTimesUsedCurrent++;
}

// Routine Description:
// - Logs a particular VT100 escape code failed or was unsupported.
//
// Arguments:
// - code - VT100 code.
// Return Value:
// - <none>
void TermTelemetry::LogFailed(const wchar_t wch) noexcept
{
    if (wch > CHAR_MAX)
    {
        _uiTimesFailedOutsideRange++;
        _uiTimesFailedOutsideRangeCurrent++;
    }
    else
    {
        // Even though we pass over a wide character, we only care about the ASCII single byte character.
        _uiTimesFailed[wch]++;
        _uiTimesFailedCurrent++;
    }
}

// Routine Description:
// - Gets and resets the total count of codes used.
//
// Arguments:
// - <none>
// Return Value:
// - total number.
unsigned int TermTelemetry::GetAndResetTimesUsedCurrent() noexcept
{
    const auto temp = _uiTimesUsedCurrent;
    _uiTimesUsedCurrent = 0;
    return temp;
}

// Routine Description:
// - Gets and resets the total count of codes failed.
//
// Arguments:
// - <none>
// Return Value:
// - total number.
unsigned int TermTelemetry::GetAndResetTimesFailedCurrent() noexcept
{
    const auto temp = _uiTimesFailedCurrent;
    _uiTimesFailedCurrent = 0;
    return temp;
}

// Routine Description:
// - Gets and resets the total count of codes failed outside the valid range.
//
// Arguments:
// - <none>
// Return Value:
// - total number.
unsigned int TermTelemetry::GetAndResetTimesFailedOutsideRangeCurrent() noexcept
{
    const auto temp = _uiTimesFailedOutsideRangeCurrent;
    _uiTimesFailedOutsideRangeCurrent = 0;
    return temp;
}

// Routine Description:
// - Lets us know whether we should write the final log.  Typically set true when the console has been
// interacted with, to help reduce the amount of telemetry we're sending.
//
// Arguments:
// - writeLog - true if we should write the log.
// Return Value:
// - <none>
void TermTelemetry::SetShouldWriteFinalLog(const bool writeLog) noexcept
{
    _fShouldWriteFinalLog = writeLog;
}

// Routine Description:
// - Sets the activity Id, so we can match our events with other providers (such as Microsoft.Windows.Console.Host).
//
// Arguments:
// - activityId - Pointer to Guid to set our activity Id to.
// Return Value:
// - <none>
void TermTelemetry::SetActivityId(const GUID* activityId) noexcept
{
    _activityId = *activityId;
}

// Routine Description:
// - Writes the final log of all the telemetry collected.  The primary reason to send back a final log instead
// of individual events is to reduce the amount of telemetry being sent and potentially overloading our servers.
//
// Arguments:
// - code - VT100 code.
// Return Value:
// - <none>
void TermTelemetry::WriteFinalTraceLog() const
{
    if (_fShouldWriteFinalLog)
    {
        // Determine if we've logged any VT100 sequences at all.
        auto fLoggedSequence = (_uiTimesFailedOutsideRange > 0);

        if (!fLoggedSequence)
        {
            for (auto n = 0; n < ARRAYSIZE(_uiTimesUsed); n++)
            {
                if (_uiTimesUsed[n] > 0)
                {
                    fLoggedSequence = true;
                    break;
                }
            }
        }

        if (!fLoggedSequence)
        {
            for (auto n = 0; n < ARRAYSIZE(_uiTimesFailed); n++)
            {
                if (_uiTimesFailed[n] > 0)
                {
                    fLoggedSequence = true;
                    break;
                }
            }
        }

        // Only send telemetry if we've logged some VT100 sequences.  This should help reduce the amount of unnecessary
        // telemetry being sent.
        if (fLoggedSequence)
        {
            // I could use the TraceLoggingUIntArray, but then we would have to know the order of the enums on the backend.
            // So just log each enum count separately with its string representation which makes it more human readable.
            // Set the related activity to NULL since we aren't using it.
            TraceLoggingWriteActivity(g_hConsoleVirtTermParserEventTraceProvider,
                                      "ControlCodesUsed",
                                      &_activityId,
                                      NULL,
                                      TraceLoggingUInt32(_uiTimesUsed[CUU], "CUU"),
                                      TraceLoggingUInt32(_uiTimesUsed[CUD], "CUD"),
                                      TraceLoggingUInt32(_uiTimesUsed[CUF], "CUF"),
                                      TraceLoggingUInt32(_uiTimesUsed[CUB], "CUB"),
                                      TraceLoggingUInt32(_uiTimesUsed[CNL], "CNL"),
                                      TraceLoggingUInt32(_uiTimesUsed[CPL], "CPL"),
                                      TraceLoggingUInt32(_uiTimesUsed[CHA], "CHA"),
                                      TraceLoggingUInt32(_uiTimesUsed[CUP], "CUP"),
                                      TraceLoggingUInt32(_uiTimesUsed[ED], "ED"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSED], "DECSED"),
                                      TraceLoggingUInt32(_uiTimesUsed[EL], "EL"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSEL], "DECSEL"),
                                      TraceLoggingUInt32(_uiTimesUsed[SGR], "SGR"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSC], "DECSC"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECRC], "DECRC"),
                                      TraceLoggingUInt32(_uiTimesUsed[SM], "SM"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSET], "DECSET"),
                                      TraceLoggingUInt32(_uiTimesUsed[RM], "RM"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECRST], "DECRST"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECKPAM], "DECKPAM"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECKPNM], "DECKPNM"),
                                      TraceLoggingUInt32(_uiTimesUsed[DSR], "DSR"),
                                      TraceLoggingUInt32(_uiTimesUsed[DA], "DA"),
                                      TraceLoggingUInt32(_uiTimesUsed[DA2], "DA2"),
                                      TraceLoggingUInt32(_uiTimesUsed[DA3], "DA3"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECREQTPARM], "DECREQTPARM"),
                                      TraceLoggingUInt32(_uiTimesUsed[VPA], "VPA"),
                                      TraceLoggingUInt32(_uiTimesUsed[HPR], "HPR"),
                                      TraceLoggingUInt32(_uiTimesUsed[VPR], "VPR"),
                                      TraceLoggingUInt32(_uiTimesUsed[ICH], "ICH"),
                                      TraceLoggingUInt32(_uiTimesUsed[DCH], "DCH"),
                                      TraceLoggingUInt32(_uiTimesUsed[IL], "IL"),
                                      TraceLoggingUInt32(_uiTimesUsed[DL], "DL"),
                                      TraceLoggingUInt32(_uiTimesUsed[SU], "SU"),
                                      TraceLoggingUInt32(_uiTimesUsed[SD], "SD"),
                                      TraceLoggingUInt32(_uiTimesUsed[ANSISYSSC], "ANSISYSSC"),
                                      TraceLoggingUInt32(_uiTimesUsed[ANSISYSRC], "ANSISYSRC"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSTBM], "DECSTBM"),
                                      TraceLoggingUInt32(_uiTimesUsed[NEL], "NEL"),
                                      TraceLoggingUInt32(_uiTimesUsed[IND], "IND"),
                                      TraceLoggingUInt32(_uiTimesUsed[RI], "RI"),
                                      TraceLoggingUInt32(_uiTimesUsed[OSCWT], "OscWindowTitle"),
                                      TraceLoggingUInt32(_uiTimesUsed[HTS], "HTS"),
                                      TraceLoggingUInt32(_uiTimesUsed[CHT], "CHT"),
                                      TraceLoggingUInt32(_uiTimesUsed[CBT], "CBT"),
                                      TraceLoggingUInt32(_uiTimesUsed[TBC], "TBC"),
                                      TraceLoggingUInt32(_uiTimesUsed[ECH], "ECH"),
                                      TraceLoggingUInt32(_uiTimesUsed[DesignateG0], "DesignateG0"),
                                      TraceLoggingUInt32(_uiTimesUsed[DesignateG1], "DesignateG1"),
                                      TraceLoggingUInt32(_uiTimesUsed[DesignateG2], "DesignateG2"),
                                      TraceLoggingUInt32(_uiTimesUsed[DesignateG3], "DesignateG3"),
                                      TraceLoggingUInt32(_uiTimesUsed[LS2], "LS2"),
                                      TraceLoggingUInt32(_uiTimesUsed[LS3], "LS3"),
                                      TraceLoggingUInt32(_uiTimesUsed[LS1R], "LS1R"),
                                      TraceLoggingUInt32(_uiTimesUsed[LS2R], "LS2R"),
                                      TraceLoggingUInt32(_uiTimesUsed[LS3R], "LS3R"),
                                      TraceLoggingUInt32(_uiTimesUsed[SS2], "SS2"),
                                      TraceLoggingUInt32(_uiTimesUsed[SS3], "SS3"),
                                      TraceLoggingUInt32(_uiTimesUsed[DOCS], "DOCS"),
                                      TraceLoggingUInt32(_uiTimesUsed[HVP], "HVP"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSTR], "DECSTR"),
                                      TraceLoggingUInt32(_uiTimesUsed[RIS], "RIS"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSCUSR], "DECSCUSR"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSCA], "DECSCA"),
                                      TraceLoggingUInt32(_uiTimesUsed[DTTERM_WM], "DTTERM_WM"),
                                      TraceLoggingUInt32(_uiTimesUsed[OSCCT], "OscColorTable"),
                                      TraceLoggingUInt32(_uiTimesUsed[OSCSCC], "OscSetCursorColor"),
                                      TraceLoggingUInt32(_uiTimesUsed[OSCRCC], "OscResetCursorColor"),
                                      TraceLoggingUInt32(_uiTimesUsed[OSCFG], "OscForegroundColor"),
                                      TraceLoggingUInt32(_uiTimesUsed[OSCBG], "OscBackgroundColor"),
                                      TraceLoggingUInt32(_uiTimesUsed[OSCSCB], "OscSetClipboard"),
                                      TraceLoggingUInt32(_uiTimesUsed[REP], "REP"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECAC1], "DECAC1"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSWL], "DECSWL"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECDWL], "DECDWL"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECDHL], "DECDHL"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECALN], "DECALN"),
                                      TraceLoggingUInt32(_uiTimesUsed[XTPUSHSGR], "XTPUSHSGR"),
                                      TraceLoggingUInt32(_uiTimesUsed[XTPOPSGR], "XTPOPSGR"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECRQM], "DECRQM"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECCARA], "DECCARA"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECRARA], "DECRARA"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECCRA], "DECCRA"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECFRA], "DECFRA"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECERA], "DECERA"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSERA], "DECSERA"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECSACE], "DECSACE"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECINVM], "DECINVM"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECAC], "DECAC"),
                                      TraceLoggingUInt32(_uiTimesUsed[DECPS], "DECPS"),
                                      TraceLoggingUInt32Array(_uiTimesFailed, ARRAYSIZE(_uiTimesFailed), "Failed"),
                                      TraceLoggingUInt32(_uiTimesFailedOutsideRange, "FailedOutsideRange"));
        }
    }
}

#pragma warning(pop)
