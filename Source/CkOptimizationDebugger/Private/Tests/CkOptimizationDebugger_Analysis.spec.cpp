#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ScanContext.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Lighting.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Texture.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"
#include "CkOptimizationDebugger/Settings/CkOptimizationDebuggerSettings.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Analysis_ThresholdDefaults,
    "Ck.OptimizationDebugger.Analysis.ThresholdDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Analysis_ThresholdDefaults::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_thresholds;

    // The plain struct's defaults and the settings object's defaults are two copies of the same numbers. They are
    // pinned together here because a settings default that moves without this one moving would make a
    // default-constructed scan silently disagree with a configured one — and nothing else would notice.
    const auto Defaults = FCkOptimizationDebugger_Thresholds{};
    const auto* Settings = GetDefault<UCkOptimizationDebuggerSettings>();

    TestNotNull(TEXT("The settings CDO is reachable"), Settings);

    if (Settings == nullptr)
    { return false; }

    TestEqual(TEXT("MaxTriangleCountLOD0 agrees"), Defaults.MaxTriangleCountLOD0, Settings->MaxTriangleCountLOD0);
    TestEqual(TEXT("MinTrianglesForNanite agrees"), Defaults.MinTrianglesForNanite, Settings->MinTrianglesForNanite);
    TestEqual(TEXT("MaxTrianglesForNaniteWarning agrees"),
        Defaults.MaxTrianglesForNaniteWarning, Settings->MaxTrianglesForNaniteWarning);
    TestEqual(TEXT("MaxCollisionPrimitives agrees"), Defaults.MaxCollisionPrimitives, Settings->MaxCollisionPrimitives);
    TestEqual(TEXT("MaxTextureSize agrees"), Defaults.MaxTextureSize, Settings->MaxTextureSize);
    TestEqual(TEXT("MaxMaterialSlots agrees"), Defaults.MaxMaterialSlots, Settings->MaxMaterialSlots);
    TestEqual(TEXT("MaxTextureSamplers agrees"), Defaults.MaxTextureSamplers, Settings->MaxTextureSamplers);
    TestEqual(TEXT("MaxMovableLights agrees"), Defaults.MaxMovableLights, Settings->MaxMovableLights);
    TestEqual(TEXT("MaxLightmapResolution agrees"), Defaults.MaxLightmapResolution, Settings->MaxLightmapResolution);
    TestEqual(TEXT("MinRepeatedActorsForInstancing agrees"),
        Defaults.MinRepeatedActorsForInstancing, Settings->MinRepeatedActorsForInstancing);
    TestEqual(TEXT("MaxBlueprintDependencies agrees"),
        Defaults.MaxBlueprintDependencies, Settings->MaxBlueprintDependencies);
    TestEqual(TEXT("MinTexturesForStreamingWarning agrees"),
        Defaults.MinTexturesForStreamingWarning, Settings->MinTexturesForStreamingWarning);

    // Copying from a null settings pointer must not zero the struct: a scan judged by zero budgets would flag every
    // asset in the project, which is the loudest possible way to say nothing.
    const auto FromNull = Build_FromSettings(nullptr);
    TestEqual(TEXT("A missing settings object falls back to the defaults, never to zero"),
        FromNull.MaxTriangleCountLOD0, Defaults.MaxTriangleCountLOD0);

    const auto FromCdo = Build_FromSettings(Settings);
    TestEqual(TEXT("Copying from the CDO reproduces its values"),
        FromCdo.MaxTextureSize, Settings->MaxTextureSize);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Analysis_GraduatedSeverity,
    "Ck.OptimizationDebugger.Analysis.GraduatedSeverity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Analysis_GraduatedSeverity::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_thresholds;

    const auto AsInt = [](ECkOptimizationDebugger_Severity InSeverity) -> int32
    {
        return static_cast<int32>(InSeverity);
    };

    // --- escalation saturates ---
    TestEqual(TEXT("Minor escalates to Major"),
        AsInt(Escalate_One(ECkOptimizationDebugger_Severity::Minor)),
        AsInt(ECkOptimizationDebugger_Severity::Major));
    TestEqual(TEXT("Major escalates to Critical"),
        AsInt(Escalate_One(ECkOptimizationDebugger_Severity::Major)),
        AsInt(ECkOptimizationDebugger_Severity::Critical));
    TestEqual(TEXT("Critical is the ceiling — escalation saturates rather than wrapping"),
        AsInt(Escalate_One(ECkOptimizationDebugger_Severity::Critical)),
        AsInt(ECkOptimizationDebugger_Severity::Critical));

    // --- grading around the budget ---
    const auto Budget = 100.0;
    const auto Base = ECkOptimizationDebugger_Severity::Major;

    TestEqual(TEXT("Exactly at the budget reads as the base severity"),
        AsInt(Get_GraduatedSeverity(Budget, Budget, Base)), AsInt(Base));
    TestEqual(TEXT("Just past the budget still reads as the base severity"),
        AsInt(Get_GraduatedSeverity(Budget + 1.0, Budget, Base)), AsInt(Base));
    TestEqual(TEXT("Just under twice the budget still reads as the base severity"),
        AsInt(Get_GraduatedSeverity((Budget * 2.0) - 1.0, Budget, Base)), AsInt(Base));

    TestEqual(TEXT("Twice the budget escalates one step — the 2x rule"),
        AsInt(Get_GraduatedSeverity(Budget * 2.0, Budget, Base)),
        AsInt(ECkOptimizationDebugger_Severity::Critical));
    TestEqual(TEXT("Ten times the budget is no worse than Critical"),
        AsInt(Get_GraduatedSeverity(Budget * 10.0, Budget, Base)),
        AsInt(ECkOptimizationDebugger_Severity::Critical));

    TestEqual(TEXT("A Minor-based check grades to Major at 2x, not straight to Critical"),
        AsInt(Get_GraduatedSeverity(Budget * 2.0, Budget, ECkOptimizationDebugger_Severity::Minor)),
        AsInt(ECkOptimizationDebugger_Severity::Major));

    // --- total by construction ---
    TestEqual(TEXT("A zero budget cannot be exceeded by a ratio, so it grades to the base severity"),
        AsInt(Get_GraduatedSeverity(1000.0, 0.0, Base)), AsInt(Base));
    TestEqual(TEXT("A negative budget behaves the same way rather than escalating"),
        AsInt(Get_GraduatedSeverity(1000.0, -5.0, Base)), AsInt(Base));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Analysis_FindingConstruction,
    "Ck.OptimizationDebugger.Analysis.FindingConstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Analysis_FindingConstruction::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_scan;

    // Every check builds its findings through these three helpers, so this is where the "one key format" claim is
    // actually pinned: a check that hand-rolled a key would be the one thing that breaks row reuse across re-scans.
    const auto AssetTarget = Build_AssetTarget(
        FSoftObjectPath{TEXT("/Game/Spec/SM_Boulder.SM_Boulder")}, TEXT("SM_Boulder"));

    TestEqual(TEXT("An asset target is an Asset"),
        static_cast<int32>(AssetTarget.Kind),
        static_cast<int32>(ECkOptimizationDebugger_TargetKind::Asset));

    const auto AssetFinding = Build_Finding(FName{TEXT("Mesh.TriangleBudget")},
        ECkOptimizationDebugger_Severity::Major, ECkOptimizationDebugger_Category::Mesh,
        AssetTarget, TEXT("Triangle count over budget"), TEXT("Why."), TEXT("What to do."));

    TestEqual(TEXT("The finding's key is the one the model would derive"),
        AssetFinding.StableKey,
        ck_optimization_debugger_model::Build_StableKey(AssetFinding.CheckId, AssetFinding.Target));
    TestFalse(TEXT("A finding carries no auto-fix unless a check says so"), AssetFinding.HasAutoFix);

    const auto ActorTarget = Build_ActorTarget(
        FSoftObjectPath{TEXT("/Game/Maps/L_Main.L_Main:PersistentLevel.SM_Rock_12")},
        TEXT("SM_Rock_12"), TEXT("L_Main"));

    TestEqual(TEXT("An actor target names its level"), ActorTarget.LevelName, FString{TEXT("L_Main")});
    TestEqual(TEXT("An actor target is an Actor"),
        static_cast<int32>(ActorTarget.Kind),
        static_cast<int32>(ECkOptimizationDebugger_TargetKind::Actor));

    const auto SettingsTarget = Build_ProjectSettingsTarget(TEXT("Rendering"), TEXT("Ray tracing"));

    TestEqual(TEXT("A settings target carries its section rather than a path"),
        SettingsTarget.SettingsSectionName, FString{TEXT("Rendering")});
    TestTrue(TEXT("...and no object path at all"), SettingsTarget.Path.IsNull());

    // Two settings findings from DIFFERENT checks on the SAME section must not collide, because the key is built
    // from the check id first.
    const auto RayTracing = Build_Finding(FName{TEXT("ProjectSettings.RayTracing")},
        ECkOptimizationDebugger_Severity::Minor, ECkOptimizationDebugger_Category::ProjectSettings,
        SettingsTarget, TEXT("Ray tracing is on"), TEXT("Why."), TEXT("What to do."));

    const auto Streaming = Build_Finding(FName{TEXT("ProjectSettings.TextureStreamingDisabled")},
        ECkOptimizationDebugger_Severity::Critical, ECkOptimizationDebugger_Category::ProjectSettings,
        SettingsTarget, TEXT("Texture streaming is off"), TEXT("Why."), TEXT("What to do."));

    TestNotEqual(TEXT("Two settings checks on one section produce two distinct keys"),
        RayTracing.StableKey, Streaming.StableKey);

    // --- the usage sentence an asset finding appends ---
    auto Asset = FCkOptimizationDebugger_ScanAsset{};
    TestTrue(TEXT("An asset nothing references appends nothing"), Build_UsageSentence(Asset).IsEmpty());

    Asset.ReferencingLevelNames = {TEXT("L_Main"), TEXT("L_Sub")};
    Asset.UsageCount = 7;

    const auto Sentence = Build_UsageSentence(Asset);
    TestTrue(TEXT("The usage sentence names every referencing level"),
        Sentence.Contains(TEXT("L_Main")) && Sentence.Contains(TEXT("L_Sub")));
    TestTrue(TEXT("...and how many placements the scan saw"), Sentence.Contains(TEXT("7")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

// The check families are declared only in editor builds — see their headers. The synthetic context below needs no
// world, but the function it drives does not exist without them.
#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Analysis_LightmapFindingIdentity,
    "Ck.OptimizationDebugger.Analysis.LightmapFindingIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Analysis_LightmapFindingIdentity::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model;

    // The CONTRACT under test is the one `FCkOptimizationDebugger_FindingRow::StableKey` states: a key is unique per
    // (check, thing). `Lighting.LightmapResolution` reads a per-COMPONENT override and targets the ACTOR, so an
    // actor carrying two over-budget static mesh components is the case where a per-component finding would emit the
    // same key twice.
    //
    // Why that matters beyond tidiness: the window reuses list rows by this key. A duplicate makes every rebuild
    // allocate a fresh row for the second one, which fires `RequestListRefresh` on every keystroke in the filter box
    // and leaves that row unable to hold a selection.
    //
    // No world, no actors — the check is a pure function over the gathered context, which is the whole reason the
    // scan is split into gather and check.
    auto Thresholds = FCkOptimizationDebugger_Thresholds{};
    Thresholds.MaxLightmapResolution = 128;

    const auto ActorPath = FSoftObjectPath{TEXT("/Game/Spec/L_Spec.L_Spec:PersistentLevel.SM_Spec")};

    const auto MakeUsage = [&ActorPath](const FString& InMeshName, int32 InLightMapRes)
        -> FCkOptimizationDebugger_ScanMeshUsage
    {
        auto Usage = FCkOptimizationDebugger_ScanMeshUsage{};

        Usage.ActorPath = ActorPath;
        Usage.ActorDisplayName = FString{TEXT("SM_Spec")};
        Usage.LevelName = FString{TEXT("L_Spec")};
        Usage.MeshDisplayName = InMeshName;
        Usage.OverridesLightMapRes = true;
        Usage.OverriddenLightMapRes = InLightMapRes;

        return Usage;
    };

    auto Context = FCkOptimizationDebugger_ScanContext{};
    Context.MeshUsages = {MakeUsage(TEXT("SM_Body"), 512), MakeUsage(TEXT("SM_Trim"), 256)};

    auto Findings = TArray<FCkOptimizationDebugger_FindingRow>{};
    ck_optimization_debugger_checks_lighting::Run_Checks(Context, Thresholds, Findings);

    TestEqual(TEXT("Two over-budget components on ONE actor produce ONE finding"), Findings.Num(), 1);

    auto SeenKeys = TSet<FString>{};

    for (const auto& Finding : Findings)
    {
        TestFalse(ck::Format_UE(TEXT("{} is emitted exactly once"), Finding.StableKey),
            SeenKeys.Contains(Finding.StableKey));

        SeenKeys.Add(Finding.StableKey);
    }

    if (Findings.Num() == 1)
    {
        // Graded and worded by the WORST override, not by whichever component the walk reached first — otherwise the
        // severity would depend on component iteration order.
        TestTrue(TEXT("...naming the worst override, not the first one seen"),
            Findings[0].Explanation.Contains(TEXT("512")));
        TestTrue(TEXT("...and saying how many components are over budget"),
            Findings[0].Explanation.Contains(TEXT("2")));
    }

    // Two DIFFERENT actors are two different things and must still be two rows.
    auto SecondActor = MakeUsage(TEXT("SM_Body"), 512);
    SecondActor.ActorPath = FSoftObjectPath{TEXT("/Game/Spec/L_Spec.L_Spec:PersistentLevel.SM_Other")};
    SecondActor.ActorDisplayName = FString{TEXT("SM_Other")};

    Context.MeshUsages.Add(SecondActor);

    Findings.Reset();
    ck_optimization_debugger_checks_lighting::Run_Checks(Context, Thresholds, Findings);

    TestEqual(TEXT("Two actors produce two findings"), Findings.Num(), 2);

    if (Findings.Num() == 2)
    {
        TestNotEqual(TEXT("...with distinct stable keys"), Findings[0].StableKey, Findings[1].StableKey);
    }

    // A component at exactly the budget is not over it — the check compares strictly.
    auto AtBudget = FCkOptimizationDebugger_ScanContext{};
    AtBudget.MeshUsages = {MakeUsage(TEXT("SM_Body"), 128)};

    Findings.Reset();
    ck_optimization_debugger_checks_lighting::Run_Checks(AtBudget, Thresholds, Findings);

    TestEqual(TEXT("An override exactly at the budget is not a finding"), Findings.Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Analysis_EffectiveTextureDims,
    "Ck.OptimizationDebugger.Analysis.EffectiveTextureDims",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Analysis_EffectiveTextureDims::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_checks_texture;

    // A texture budget is a statement about what SHIPS. An artist who imports at 4096 and caps the build at 2048 has
    // already done the thing the finding would ask for, so judging the imported size reports them as over budget for
    // being conservative — the false positive this case exists to keep out.
    //
    // The fixtures are transient in-memory textures: a spec whose answer depends on an asset in /Game is a spec that
    // fails on somebody else's branch.
    const auto MakeTexture = []() -> UTexture2D*
    {
        auto* Texture = NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient);

        Texture->Source.Init(4096, 4096, 1, 1, TSF_BGRA8);

        // Platform-independence pin: TMGS_NoMipmaps makes CalculateLODBias return 0 outright
        // (engine TextureLODSettings.cpp:191), so no device profile on the running machine can bias the built size
        // and turn these expectations into whatever this workstation happens to be configured for.
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->LODBias = 0;

        return Texture;
    };

    const auto TestBuiltSize = [this](const FString& InCase, UTexture2D* InTexture, int32 InExpectedWidth, int32 InExpectedHeight)
        -> void
    {
        const auto Dims = TryGet_TextureDims(InTexture);

        if (NOT TestTrue(ck::Format_UE(TEXT("{}: the dimensions are readable"), InCase), Dims.IsSet()))
        { return; }

        TestEqual(ck::Format_UE(TEXT("{}: the SOURCE width is what was imported"), InCase), Dims->SourceWidth, 4096);
        TestEqual(ck::Format_UE(TEXT("{}: the SOURCE height is what was imported"), InCase), Dims->SourceHeight, 4096);

        TestEqual(ck::Format_UE(TEXT("{}: the BUILT width is what ships"), InCase), Dims->BuiltWidth, InExpectedWidth);
        TestEqual(ck::Format_UE(TEXT("{}: the BUILT height is what ships"), InCase), Dims->BuiltHeight, InExpectedHeight);
    };

    // A: nothing caps the build, so the two sizes agree.
    TestBuiltSize(TEXT("Uncapped"), MakeTexture(), 4096, 4096);

    // B: Resize During Build — the texture editor's "Max In-Game" line, and the case that produced the false positive.
    auto* Resized = MakeTexture();
    Resized->PowerOfTwoMode = ETexturePowerOfTwoSetting::ResizeToSpecificResolution;
    Resized->ResizeDuringBuildX = 2048;
    Resized->ResizeDuringBuildY = 2048;

    TestBuiltSize(TEXT("ResizeDuringBuild"), Resized, 2048, 2048);

    // C: Maximum Texture Size — the other build-time cap, and the one the finding's own recommendation names.
    auto* Capped = MakeTexture();
    Capped->MaxTextureSize = 1024;

    TestBuiltSize(TEXT("MaxTextureSize"), Capped, 1024, 1024);

    // D: both, where the tighter one has to win — a rule applied in the wrong order would report 2048 here.
    auto* Both = MakeTexture();
    Both->PowerOfTwoMode = ETexturePowerOfTwoSetting::ResizeToSpecificResolution;
    Both->ResizeDuringBuildX = 2048;
    Both->ResizeDuringBuildY = 2048;
    Both->MaxTextureSize = 1024;

    TestBuiltSize(TEXT("ResizeAndMaxSize"), Both, 1024, 1024);

    // A texture that is not a texture is not a finding either.
    TestFalse(TEXT("A null texture reports no dimensions"), TryGet_TextureDims(nullptr).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif

#endif
