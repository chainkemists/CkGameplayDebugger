#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkIconStyle.h"

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

    auto SeenIcons = TSet<ECk_Icon>{};

    for (const auto Tone : Severities)
    {
        const auto Icon = Get_ToneIcon(Tone);

        TestTrue(TEXT("Every severity tone has a glyph"), Icon != ECk_Icon::None);

        // Distinctness is the whole point. Two tones sharing a picture is the defect this replaced: `Skull` used to
        // mean Critical here, Failed in the gallery, and world-trouble in the crowd debugger.
        TestFalse(TEXT("Tone glyph is unique"), SeenIcons.Contains(Icon));

        SeenIcons.Add(Icon);

        // The identifier is compile-checked now, but the vendored SVG behind it can still go missing — assert the
        // brush actually registered at both sizes.
        TestNotNull(TEXT("Tone glyph resolves at 16"),
            FCkIconStyle::Get_Brush(Icon, ECk_Icon_BrushSize::Size_16x16));

        TestNotNull(TEXT("Tone glyph resolves at 24"),
            FCkIconStyle::Get_Brush(Icon, ECk_Icon_BrushSize::Size_24x24));
    }

    // ---- The two non-severities deliberately have none ----
    // `Neutral` says "nothing to report" and `Accent` says "look here". Handing either a severity picture would be
    // this axis asserting something the tone never claimed, so both answer `None` and the registry draws nothing.
    TestTrue(TEXT("Neutral has no severity glyph"),
        Get_ToneIcon(ECk_Tone::Neutral) == ECk_Icon::None);

    TestTrue(TEXT("Accent has no severity glyph"),
        Get_ToneIcon(ECk_Tone::Accent) == ECk_Icon::None);

    // ---- Severity glyphs are NOT in the decorative pool ----
    // The generated pool is what an archetype without a bespoke glyph is assigned from at random. A severity picture
    // handed out as somebody's arbitrary decoration would make the one thing on screen that must mean exactly one
    // thing mean anything at all.
    const auto DecorativePool = ck::icons::Get_GeneratedPool();

    for (const auto Icon : SeenIcons)
    {
        TestFalse(TEXT("Severity glyph is not decorative"),
            DecorativePool.Contains(Icon));
    }

    return true;
}

#endif

// --------------------------------------------------------------------------------------------------------------------
