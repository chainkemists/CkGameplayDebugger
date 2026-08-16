#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerToneIcons_SeverityGlyphs,
    "Ck.DebuggerCommon.Axes.ToneIcons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerToneIcons_SeverityGlyphs::RunTest(const FString& Parameters)
{
    using namespace ck::debug_axes;

    // ---- The four severities each have a glyph, and no two share one ----
    const auto Severities = TArray<ECk_Tone>{
        ECk_Tone::Info,
        ECk_Tone::Ok,
        ECk_Tone::Warn,
        ECk_Tone::Err};

    auto SeenIds = TSet<FName>{};

    for (const auto Tone : Severities)
    {
        const auto IconId = Get_ToneIconId(Tone);

        TestFalse(TEXT("Every severity tone has a glyph"), IconId.IsNone());

        // Distinctness is the whole point. Two tones sharing a picture is the defect this replaced: `Skull` used to
        // mean Critical here, Failed in the gallery, and world-trouble in the crowd debugger.
        TestFalse(FString::Printf(TEXT("Tone glyph is unique: %s"), *IconId.ToString()),
            SeenIds.Contains(IconId));

        SeenIds.Add(IconId);

        // The silent-nullptr class of bug: a typo'd file name resolves to no brush and draws nothing, with no error
        // anywhere. Asserting the brush resolves through BOTH style sets is what makes a renamed or deleted SVG a
        // test failure rather than a blank space somebody notices months later. The two sets scan the same
        // `Resources/Icons/**` tree under different prefixes, and the IconToggle path uses the Common one.
        TestNotNull(FString::Printf(TEXT("Tone glyph resolves in FCkDebuggerStyle: %s"), *IconId.ToString()),
            FCkDebuggerStyle::Get_IconBrush(IconId));

        TestNotNull(FString::Printf(TEXT("Tone glyph resolves in FCkDebuggerCommonStyle: %s"), *IconId.ToString()),
            FCkDebuggerCommonStyle::Get_IconBrush(IconId));
    }

    // ---- The two non-severities deliberately have none ----
    // `Neutral` says "nothing to report" and `Accent` says "look here". Handing either a severity picture would be
    // this axis asserting something the tone never claimed, so both answer `NAME_None` and callers draw nothing.
    TestTrue(TEXT("Neutral has no severity glyph"),
        Get_ToneIconId(ECk_Tone::Neutral).IsNone());

    TestTrue(TEXT("Accent has no severity glyph"),
        Get_ToneIconId(ECk_Tone::Accent).IsNone());

    // ---- Severity glyphs are NOT in the decorative pool ----
    // `Icons/General/**` is the pool a feature without a bespoke glyph is assigned from at random. A severity picture
    // handed out as somebody's arbitrary decoration would make the one thing on screen that must mean exactly one
    // thing mean anything at all, which is why these live at the `Resources/Icons` root instead.
    const auto& DecorativePool = FCkDebuggerStyle::Get_GeneralIconPool();

    for (const auto IconId : SeenIds)
    {
        TestFalse(FString::Printf(TEXT("Severity glyph is not decorative: %s"), *IconId.ToString()),
            DecorativePool.Contains(IconId));
    }

    return true;
}

#endif

// --------------------------------------------------------------------------------------------------------------------
