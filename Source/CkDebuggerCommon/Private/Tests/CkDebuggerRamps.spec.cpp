#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include <limits>

// --------------------------------------------------------------------------------------------------------------------
// Totality coverage for the palette-derived domain ramps. The ramps are called from paint paths on
// values a debugger DIVIDED to produce — out-of-range and NaN inputs are expected traffic, not
// programmer error, so every entry point must still return a usable color.
// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugger_ramp_tests
{
    constexpr auto ColorTolerance = 0.0001f;

    auto Is_Usable(const FLinearColor& InColor) -> bool
    {
        const auto Channels = TArray<float>{InColor.R, InColor.G, InColor.B, InColor.A};

        for (const auto Channel : Channels)
        {
            if (FMath::IsNaN(Channel) || NOT FMath::IsFinite(Channel))
            { return false; }
        }

        return true;
    }

    auto Get_NaN() -> float
    {
        return std::numeric_limits<float>::quiet_NaN();
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerRamps_HeatRampIsTotal,
    "Ck.DebuggerCommon.Style.HeatRampIsTotal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerRamps_HeatRampIsTotal::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_ramp_tests;

    // The three named stops are the contract every consumer reads the ramp against.
    TestTrue(TEXT("0.0 is the Ok stop"),
        ck::debug_axes::Get_HeatColor(0.0f).Equals(CkStyle::Ok(), ColorTolerance));

    TestTrue(TEXT("0.5 is the Warn stop — 'exactly at budget'"),
        ck::debug_axes::Get_HeatColor(0.5f).Equals(CkStyle::Warn(), ColorTolerance));

    TestTrue(TEXT("1.0 is the Err stop"),
        ck::debug_axes::Get_HeatColor(1.0f).Equals(CkStyle::Err(), ColorTolerance));

    // Out-of-range clamps to the endpoints rather than extrapolating off the palette.
    TestTrue(TEXT("Negative input clamps to the cold end"),
        ck::debug_axes::Get_HeatColor(-12.5f).Equals(ck::debug_axes::Get_HeatColor(0.0f), ColorTolerance));

    TestTrue(TEXT("Above-one input clamps to the hot end"),
        ck::debug_axes::Get_HeatColor(37.0f).Equals(ck::debug_axes::Get_HeatColor(1.0f), ColorTolerance));

    // A zero-range normalization (0/0) is the realistic NaN source; it must read as coldest.
    TestTrue(TEXT("NaN reads as the cold end"),
        ck::debug_axes::Get_HeatColor(Get_NaN()).Equals(ck::debug_axes::Get_HeatColor(0.0f), ColorTolerance));

    for (auto Step = 0; Step <= 20; ++Step)
    {
        const auto Alpha = static_cast<float>(Step) / 20.0f;

        TestTrue(
            *ck::Format_UE(TEXT("Heat at {} is a usable color"), Alpha),
            Is_Usable(ck::debug_axes::Get_HeatColor(Alpha)));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerRamps_ScoreRampIsTotal,
    "Ck.DebuggerCommon.Style.ScoreRampIsTotal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerRamps_ScoreRampIsTotal::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_ramp_tests;

    TestTrue(TEXT("0.0 is the Info stop — worst score"),
        ck::debug_axes::Get_ScoreColor(0.0f).Equals(CkStyle::Info(), ColorTolerance));

    TestTrue(TEXT("1.0 is the Ok stop — best score"),
        ck::debug_axes::Get_ScoreColor(1.0f).Equals(CkStyle::Ok(), ColorTolerance));

    TestTrue(TEXT("Negative input clamps to the worst end"),
        ck::debug_axes::Get_ScoreColor(-1.0f).Equals(CkStyle::Info(), ColorTolerance));

    TestTrue(TEXT("Above-one input clamps to the best end"),
        ck::debug_axes::Get_ScoreColor(4.0f).Equals(CkStyle::Ok(), ColorTolerance));

    TestTrue(TEXT("NaN reads as the worst end"),
        ck::debug_axes::Get_ScoreColor(Get_NaN()).Equals(CkStyle::Info(), ColorTolerance));

    for (auto Step = 0; Step <= 20; ++Step)
    {
        const auto Alpha = static_cast<float>(Step) / 20.0f;

        TestTrue(
            *ck::Format_UE(TEXT("Score at {} is a usable color"), Alpha),
            Is_Usable(ck::debug_axes::Get_ScoreColor(Alpha)));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerRamps_CategoricalPaletteWrapsAndIsDistinct,
    "Ck.DebuggerCommon.Style.CategoricalPaletteWrapsAndIsDistinct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerRamps_CategoricalPaletteWrapsAndIsDistinct::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_ramp_tests;

    const auto PaletteSize = ck::debug_axes::Get_CategoricalPaletteSize();

    TestTrue(TEXT("The palette has entries"), PaletteSize > 0);

    if (PaletteSize <= 0)
    { return false; }

    // Every entry must be a real color — a hole would render as invisible black on a dark panel.
    auto Entries = TArray<FLinearColor>{};
    for (auto Index = 0; Index < PaletteSize; ++Index)
    {
        const auto Color = ck::debug_axes::Get_CategoricalColor(Index);

        TestTrue(
            *ck::Format_UE(TEXT("Categorical entry {} is a usable color"), Index),
            Is_Usable(Color));

        Entries.Add(Color);
    }

    // Distinctness is the whole point of a categorical palette — two buckets that read the same
    // defeat the affordance. NOTE: this asserts against the CkStyle palette as configured, so a
    // local Editor Preferences edit that collapses two Category roles will (correctly) red here.
    for (auto Lhs = 0; Lhs < Entries.Num(); ++Lhs)
    {
        for (auto Rhs = Lhs + 1; Rhs < Entries.Num(); ++Rhs)
        {
            TestFalse(
                *ck::Format_UE(TEXT("Categorical entries {} and {} are distinguishable"), Lhs, Rhs),
                Entries[Lhs].Equals(Entries[Rhs], 0.02f));
        }
    }

    // Wrap, in both directions — the helper takes any int32 a caller can produce.
    for (auto Index = 0; Index < PaletteSize; ++Index)
    {
        TestTrue(
            *ck::Format_UE(TEXT("Index {} wraps forward"), Index),
            ck::debug_axes::Get_CategoricalColor(Index + PaletteSize)
                .Equals(Entries[Index], ColorTolerance));

        TestTrue(
            *ck::Format_UE(TEXT("Index {} wraps forward twice"), Index),
            ck::debug_axes::Get_CategoricalColor(Index + PaletteSize * 3)
                .Equals(Entries[Index], ColorTolerance));
    }

    TestTrue(TEXT("-1 wraps to the last entry"),
        ck::debug_axes::Get_CategoricalColor(-1).Equals(Entries.Last(), ColorTolerance));

    TestTrue(TEXT("-PaletteSize wraps to the first entry"),
        ck::debug_axes::Get_CategoricalColor(-PaletteSize).Equals(Entries[0], ColorTolerance));

    TestTrue(TEXT("A large negative index is still in-palette"),
        Entries.ContainsByPredicate([](const FLinearColor& InEntry)
        {
            return ck::debug_axes::Get_CategoricalColor(-987654).Equals(InEntry, ColorTolerance);
        }));

    TestTrue(TEXT("MIN_int32 does not escape the palette"),
        Entries.ContainsByPredicate([](const FLinearColor& InEntry)
        {
            return ck::debug_axes::Get_CategoricalColor(TNumericLimits<int32>::Min() + 1)
                .Equals(InEntry, ColorTolerance);
        }));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerRamps_CategoricalByNameIsStable,
    "Ck.DebuggerCommon.Style.CategoricalByNameIsStable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerRamps_CategoricalByNameIsStable::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_ramp_tests;

    const auto PaletteSize = ck::debug_axes::Get_CategoricalPaletteSize();

    auto Entries = TArray<FLinearColor>{};
    for (auto Index = 0; Index < PaletteSize; ++Index)
    { Entries.Add(ck::debug_axes::Get_CategoricalColor(Index)); }

    const auto Keys = TArray<FName>{
        TEXT("FCkInspector_Transform"),
        TEXT("FCkInspector_Network"),
        TEXT("Some.Gameplay.Tag"),
        TEXT(""),
        NAME_None,
    };

    for (const auto& Key : Keys)
    {
        const auto First = ck::debug_axes::Get_CategoricalColor(Key);
        const auto Second = ck::debug_axes::Get_CategoricalColor(Key);

        TestTrue(
            *ck::Format_UE(TEXT("Key '{}' resolves deterministically"), Key.ToString()),
            First.Equals(Second, ColorTolerance));

        TestTrue(
            *ck::Format_UE(TEXT("Key '{}' resolves to a palette entry"), Key.ToString()),
            Entries.ContainsByPredicate([&First](const FLinearColor& InEntry)
            {
                return InEntry.Equals(First, ColorTolerance);
            }));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
