#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_SelectionSettings.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_SelectionHud.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Widgets/SWindow.h"

// ====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SelectionSettingsRejectsInvalid_Test,
    "Ck.DebugOverlay.Selection.SettingsRejectsInvalid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SelectionSettingsRejectsInvalid_Test::RunTest(const FString&)
{
    auto* Settings = NewObject<UCk_DebugOverlay_SelectionSettings>();
    TestNotNull(TEXT("transient selection settings are constructed"), Settings);
    if (Settings == nullptr)
    { return false; }

    auto Valid = Settings->Get_Config();
    Valid.SearchRadius = 4200.0f;
    TestTrue(TEXT("valid candidate establishes a known state"), Settings->TrySet_Config(Valid, false));
    const auto Before = Settings->Get_Config();
    const auto RevisionBefore = Settings->Get_Revision();

    auto Invalid = Before;
    Invalid.SearchRadius = -1.0f;
    TestFalse(TEXT("invalid radius is rejected"), Settings->TrySet_Config(Invalid, false));
    const auto& After = Settings->Get_Config();

    TestEqual(TEXT("invalid config leaves radius unchanged"), After.SearchRadius, Before.SearchRadius);
    TestEqual(TEXT("invalid config leaves hierarchy unchanged"), static_cast<uint8>(After.Hierarchy), static_cast<uint8>(Before.Hierarchy));
    TestEqual(TEXT("invalid config leaves revision unchanged"), Settings->Get_Revision(), RevisionBefore);
    TestFalse(TEXT("invalid config returns a diagnostic"), Settings->Get_LastError().IsEmpty());
    return true;
}

// ====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SelectionSettingsJsonRoundTrip_Test,
    "Ck.DebugOverlay.Selection.SettingsJsonRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SelectionSettingsJsonRoundTrip_Test::RunTest(const FString&)
{
    auto* Source = NewObject<UCk_DebugOverlay_SelectionSettings>();
    auto* Target = NewObject<UCk_DebugOverlay_SelectionSettings>();
    TestNotNull(TEXT("source settings are constructed"), Source);
    TestNotNull(TEXT("target settings are constructed"), Target);
    if (Source == nullptr || Target == nullptr)
    { return false; }

    auto Config = Source->Get_Config();
    Config.Hierarchy = ECk_DebugOverlay_SelectionHierarchy::AllEntities;
    Config.RootAnchor = ECk_DebugOverlay_SelectionRootAnchor::Root;
    Config.Scope = ECk_DebugOverlay_SelectionScope::Nearby;
    Config.Targeting = ECk_DebugOverlay_SelectionTargeting::Cone;
    Config.ViewBias = 0.25f;
    Config.SearchRadius = 8800.0f;
    Config.ConeHalfAngle = 27.0f;
    Config.Order = ECk_DebugOverlay_SelectionOrder::Screen;
    Config.Stability = ECk_DebugOverlay_SelectionStability::Live;
    Config.Family = ECk_DebugOverlay_SelectionFamily::Toggle;
    Config.Labels = ECk_DebugOverlay_SelectionLabels::Shortlist;
    Config.ShowAimCone = false;
    Config.IncludeOccluded = true;
    Config.DiamondScale = 1.75f;
    TestTrue(TEXT("source accepts round-trip configuration"), Source->TrySet_Config(Config, false));

    const FName PresetName{TEXT("RoundTrip")};
    FString Json;
    TestTrue(TEXT("source saves a named preset"), Source->TrySave_NamedPreset(PresetName, false));
    TestTrue(TEXT("source exports named preset JSON"), Source->Export_NamedPresetJson(PresetName, Json));
    TestFalse(TEXT("export produces JSON"), Json.IsEmpty());

    FName ImportedName;
    TestTrue(TEXT("target imports JSON atomically"), Target->TryImport_NamedPresetJson(Json, ImportedName, false));
    TestEqual(TEXT("import preserves name"), ImportedName, PresetName);
    TestTrue(TEXT("target applies imported preset"), Target->TryApply_NamedPreset(ImportedName, false));

    const auto& Result = Target->Get_Config();
    TestEqual(TEXT("hierarchy round-trips"), static_cast<uint8>(Result.Hierarchy), static_cast<uint8>(Config.Hierarchy));
    TestEqual(TEXT("scope round-trips"), static_cast<uint8>(Result.Scope), static_cast<uint8>(Config.Scope));
    TestEqual(TEXT("targeting round-trips"), static_cast<uint8>(Result.Targeting), static_cast<uint8>(Config.Targeting));
    TestEqual(TEXT("radius round-trips"), Result.SearchRadius, Config.SearchRadius);
    TestEqual(TEXT("labels round-trip"), static_cast<uint8>(Result.Labels), static_cast<uint8>(Config.Labels));
    TestEqual(TEXT("occlusion preference round-trips"), Result.IncludeOccluded, Config.IncludeOccluded);
    return true;
}

// ====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SelectionSettingsLoadedConfig_Test,
    "Ck.DebugOverlay.Selection.LoadedConfigRejectsMalformed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SelectionSettingsLoadedConfig_Test::RunTest(const FString&)
{
    auto* Settings = NewObject<UCk_DebugOverlay_SelectionSettings>();
    TestNotNull(TEXT("transient selection settings are constructed"), Settings);
    if (Settings == nullptr)
    { return false; }

    auto& RawConfig = const_cast<FCk_DebugOverlay_SelectionConfig&>(Settings->Get_Config());
    RawConfig.SearchRadius = std::numeric_limits<float>::quiet_NaN();
    const auto NaNRevision = Settings->Get_Revision();
    Settings->Validate_LoadedConfig();
    TestEqual(TEXT("loaded NaN falls back to the entire known default policy"), Settings->Get_Config().SearchRadius, 10000.0f);
    TestTrue(TEXT("loaded NaN records a visible diagnostic"), Settings->Get_LastError().Contains(TEXT("invalid at load")));
    TestEqual(TEXT("loaded NaN publishes one safe replacement revision"), Settings->Get_Revision(), NaNRevision + 1);

    RawConfig = Settings->Get_Config();
    RawConfig.Hierarchy = static_cast<ECk_DebugOverlay_SelectionHierarchy>(255);
    const auto EnumRevision = Settings->Get_Revision();
    Settings->Validate_LoadedConfig();
    TestEqual(TEXT("loaded invalid enum falls back to default hierarchy"),
        static_cast<uint8>(Settings->Get_Config().Hierarchy),
        static_cast<uint8>(ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots));
    TestEqual(TEXT("loaded invalid enum does not leave a partial policy"), Settings->Get_Config().SearchRadius, 10000.0f);
    TestEqual(TEXT("loaded invalid enum publishes one safe replacement revision"), Settings->Get_Revision(), EnumRevision + 1);
    return true;
}

// ====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SelectionSettingsRejectsCorruptNamedPreset_Test,
    "Ck.DebugOverlay.Selection.RejectsCorruptNamedPreset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SelectionSettingsRejectsCorruptNamedPreset_Test::RunTest(const FString&)
{
    auto* Settings = NewObject<UCk_DebugOverlay_SelectionSettings>();
    TestNotNull(TEXT("transient selection settings are constructed"), Settings);
    if (Settings == nullptr)
    { return false; }

    const auto Before = Settings->Get_Config();
    const auto RevisionBefore = Settings->Get_Revision();
    const auto CorruptJson = FString{TEXT("{\"SchemaVersion\":1,\"Name\":\"Broken\",\"Config\":{\"Hierarchy\":255,\"RootAnchor\":0,\"Scope\":0,\"Targeting\":0,\"ViewBias\":0.7,\"SearchRadius\":3500,\"ConeHalfAngle\":15,\"Order\":0,\"Stability\":0,\"Family\":0,\"Labels\":0,\"ShowAimCone\":true,\"IncludeOccluded\":false,\"DiamondScale\":1}}")};
    FName ImportedName;
    TestFalse(TEXT("corrupt named preset JSON is rejected"), Settings->TryImport_NamedPresetJson(CorruptJson, ImportedName, false));
    TestTrue(TEXT("failed import leaves output name empty"), ImportedName.IsNone());
    TestEqual(TEXT("failed import leaves active configuration untouched"), Settings->Get_Config().SearchRadius, Before.SearchRadius);
    TestEqual(TEXT("failed import does not publish a revision"), Settings->Get_Revision(), RevisionBefore);
    TestFalse(TEXT("failed import leaves no named preset to apply"), Settings->TryApply_NamedPreset(TEXT("Broken"), false));
    return true;
}

// ====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SelectionSettingsExpandedRange_Test,
    "Ck.DebugOverlay.Selection.SettingsExpandedRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SelectionSettingsExpandedRange_Test::RunTest(const FString&)
{
    auto* Settings = NewObject<UCk_DebugOverlay_SelectionSettings>();
    TestNotNull(TEXT("transient selection settings are constructed"), Settings);
    if (Settings == nullptr)
    { return false; }

    TestEqual(TEXT("new selection configuration starts at the expanded range"), FCk_DebugOverlay_SelectionConfig{}.SearchRadius, 10000.0f);
    auto ExplicitRange = Settings->Get_Config();
    ExplicitRange.SearchRadius = 3500.0f;
    TestTrue(TEXT("an explicitly selected legacy range remains valid"), Settings->TrySet_Config(ExplicitRange, false));
    Settings->Validate_LoadedConfig();
    TestEqual(TEXT("load validation preserves an explicit legacy range"), Settings->Get_Config().SearchRadius, 3500.0f);
    for (const auto Preset : { ECk_DebugOverlay_SelectionPreset::FocusFamily,
        ECk_DebugOverlay_SelectionPreset::SpatialSweep, ECk_DebugOverlay_SelectionPreset::AimCone })
    {
        TestTrue(TEXT("built-in selection preset applies"), Settings->ApplyPreset(Preset, false));
        TestEqual(TEXT("built-in selection preset uses the expanded range"), Settings->Get_Config().SearchRadius, 10000.0f);
    }
    return true;
}

// ====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SelectionHudPaint_Test,
    "Ck.DebugOverlay.Selection.HudPaint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SelectionHudPaint_Test::RunTest(const FString&)
{
    const auto Hud = SNew(SCkDebugOverlay_SelectionHud);
    auto Marker = SCkDebugOverlay_SelectionHud::FMarker{};
    Marker.Position = FVector2D{32.0f, 24.0f};
    Marker.RelativeLabel = TEXT("+1");
    Marker.Selected = true;
    Marker.Locked = true;
    auto CoLocated = Marker;
    CoLocated.RelativeLabel = TEXT("+2");
    Hud->SetSnapshot({Marker, CoLocated}, {}, 1.0f);

    const TSharedPtr<SWindow> Window = SNew(SWindow);
    auto DrawElements = FSlateWindowElementList{Window};
    auto HitTestGrid = FHittestGrid{};
    const auto Geometry = FGeometry::MakeRoot(FVector2D{320.0f, 180.0f}, FSlateLayoutTransform{});
    const auto PaintArgs = FPaintArgs{&Hud.Get(), HitTestGrid, FVector2D::ZeroVector, 0.0, 0.0f};
    Hud->OnPaint(PaintArgs, Geometry, FSlateRect{0.0f, 0.0f, 320.0f, 180.0f}, DrawElements, 0, FWidgetStyle{}, true);

    const auto& TextElements = DrawElements.GetUncachedDrawElements().Get<static_cast<uint8>(EElementType::ET_Text)>();
    TestEqual(TEXT("co-located relative badges emit Slate text elements"), TextElements.Num(), 2);
    if (TextElements.Num() == 2)
    {
        TestEqual(TEXT("first relative badge text is preserved"), FString{TextElements[0].GetText()}, FString{TEXT("+1")});
        const auto FirstBadgeSize = TextElements[0].GetLocalSize();
        TestTrue(TEXT("relative badge paint geometry has non-zero width"), FirstBadgeSize.X > 0.0f);
        TestTrue(TEXT("relative badge paint geometry has non-zero height"), FirstBadgeSize.Y > 0.0f);
        TestTrue(TEXT("co-located badge never rises above the marker anchor"), TextElements[0].GetPosition().Y >= Marker.Position.Y);
        TestTrue(TEXT("selected badge is painted outside the selected halo"),
            TextElements[0].GetPosition().X > Marker.Position.X + 6.0f * 1.30f + 4.5f);
        TestTrue(TEXT("co-located badges have measured non-overlapping vertical geometry"),
            TextElements[1].GetPosition().Y >= TextElements[0].GetPosition().Y + FirstBadgeSize.Y);
    }
    const auto& BoxElements = DrawElements.GetUncachedDrawElements().Get<static_cast<uint8>(EElementType::ET_Box)>();
    TestTrue(TEXT("relative badge emits rounded backing and accent inset boxes"), BoxElements.Num() >= 2);

    const auto EmptyHud = SNew(SCkDebugOverlay_SelectionHud);
    EmptyHud->SetSnapshot({}, {}, 1.0f);
    auto EmptyDrawElements = FSlateWindowElementList{Window};
    const auto EmptyPaintArgs = FPaintArgs{&EmptyHud.Get(), HitTestGrid, FVector2D::ZeroVector, 0.0, 0.0f};
    EmptyHud->OnPaint(EmptyPaintArgs, Geometry, FSlateRect{0.0f, 0.0f, 320.0f, 180.0f}, EmptyDrawElements, 0, FWidgetStyle{}, true);
    const auto& EmptyBoxes = EmptyDrawElements.GetUncachedDrawElements().Get<static_cast<uint8>(EElementType::ET_Box)>();
    const auto& EmptyText = EmptyDrawElements.GetUncachedDrawElements().Get<static_cast<uint8>(EElementType::ET_Text)>();
    const auto& EmptyLines = EmptyDrawElements.GetUncachedDrawElements().Get<static_cast<uint8>(EElementType::ET_Line)>();
    TestEqual(TEXT("empty HUD emits no box elements"), EmptyBoxes.Num(), 0);
    TestEqual(TEXT("empty HUD emits no text elements"), EmptyText.Num(), 0);
    TestEqual(TEXT("empty HUD emits no line elements"), EmptyLines.Num(), 0);

    auto ZeroGeometryDrawElements = FSlateWindowElementList{Window};
    FSlateDrawElement::MakeText(ZeroGeometryDrawElements, 0,
        Geometry.ToPaintGeometry(FVector2f::ZeroVector, FSlateLayoutTransform{FVector2f{64.0f, 32.0f}}),
        TEXT("Known bad zero geometry"), ck::debug_axes::ScaledFont("Regular", 9), ESlateDrawEffect::None, FLinearColor::White);
    const auto& ZeroGeometryText = ZeroGeometryDrawElements.GetUncachedDrawElements().Get<static_cast<uint8>(EElementType::ET_Text)>();
    TestEqual(TEXT("known-bad zero-size text geometry is culled before ET_Text emission"), ZeroGeometryText.Num(), 0);
    return true;
}

// ====================================================================================================================

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
