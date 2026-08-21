#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_FixPlan.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and the spec files sit in the same merged translation
// unit as each other.
namespace ck_optimization_debugger_fixplan_spec
{
    auto
        Build_Change(
            const FString& InObject,
            const FString& InProperty,
            const FString& InBefore,
            const FString& InAfter)
        -> FCkOptimizationDebugger_PlannedChange
    {
        auto Change = FCkOptimizationDebugger_PlannedChange{};
        Change.ObjectPath = FSoftObjectPath{ck::Format_UE(TEXT("/Game/Spec/{}.{}"), InObject, InObject)};
        Change.ObjectLabel = InObject;
        Change.PropertyLabel = InProperty;
        Change.BeforeText = InBefore;
        Change.AfterText = InAfter;

        return Change;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_Plan(
            const FString& InTargetName,
            const FString& InVerb,
            const TArray<FCkOptimizationDebugger_PlannedChange>& InChanges)
        -> FCkOptimizationDebugger_FixPlan
    {
        auto Plan = FCkOptimizationDebugger_FixPlan{};
        Plan.Finding.CheckId = FName{TEXT("Spec.Check")};
        Plan.Finding.Target.DisplayName = InTargetName;
        Plan.FixVerb = InVerb;
        Plan.CanApply = true;
        Plan.Changes = InChanges;

        return Plan;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_LogEntry(
            const FString& InVerb,
            const FString& InTarget,
            bool InSucceeded)
        -> FCkOptimizationDebugger_FixLogEntry
    {
        auto Entry = FCkOptimizationDebugger_FixLogEntry{};
        Entry.CheckId = FName{TEXT("Spec.Check")};
        Entry.FixVerb = InVerb;
        Entry.TargetLabel = InTarget;
        Entry.Succeeded = InSucceeded;
        Entry.Message = TEXT("Spec message.");

        return Entry;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_FixPlan_IncludedWork,
    "Ck.OptimizationDebugger.FixPlan.IncludedWork",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_FixPlan_IncludedWork::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_fixplan;
    using namespace ck_optimization_debugger_fixplan_spec;

    auto Plan = Build_Plan(TEXT("T_Bark"), TEXT("Disable sRGB"),
        {Build_Change(TEXT("T_Bark"), TEXT("sRGB"), TEXT("On"), TEXT("Off")),
         Build_Change(TEXT("T_Bark"), TEXT("Compression Settings"), TEXT("TC_Default"), TEXT("TC_Normalmap"))});

    TestTrue(TEXT("A fresh plan is entirely ticked"), Get_HasIncludedWork(Plan));
    TestEqual(TEXT("...and every change is included"), Get_IncludedChanges(Plan).Num(), 2);

    // The tick is looked up by (object, property), never by index — the apply path carries ticks across a re-plan,
    // where an index would be meaningless.
    TestTrue(TEXT("A change is found by object and property"),
        Get_IsChangeIncluded(Plan, Plan.Changes[0].ObjectPath, TEXT("sRGB")));
    TestFalse(TEXT("A property the plan never listed is never included"),
        Get_IsChangeIncluded(Plan, Plan.Changes[0].ObjectPath, TEXT("Never Planned")));

    Set_ChangeIncluded(Plan, 0, false);

    TestFalse(TEXT("An unticked change reads as excluded"),
        Get_IsChangeIncluded(Plan, Plan.Changes[0].ObjectPath, TEXT("sRGB")));
    TestTrue(TEXT("...and the plan still has work while one remains"), Get_HasIncludedWork(Plan));

    Set_AllChangesIncluded(Plan, false);
    TestFalse(TEXT("A wholly unticked plan has no work"), Get_HasIncludedWork(Plan));

    // A destructive or review fix is described entirely by its EFFECTS and has no property rows at all. Reading
    // "no changes" as "nothing to do" would silently disable every one of them.
    auto EffectOnly = Build_Plan(TEXT("SM_Rock"), TEXT("Convert To Instances"), {});
    TestTrue(TEXT("A plan with no property rows still has work"), Get_HasIncludedWork(EffectOnly));

    EffectOnly.CanApply = false;
    TestFalse(TEXT("A refused plan never has work"), Get_HasIncludedWork(EffectOnly));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_FixPlan_Summary,
    "Ck.OptimizationDebugger.FixPlan.Summary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_FixPlan_Summary::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_fixplan;
    using namespace ck_optimization_debugger_fixplan_spec;

    auto Applicable = Build_Plan(TEXT("T_Bark"), TEXT("Disable sRGB"),
        {Build_Change(TEXT("T_Bark"), TEXT("sRGB"), TEXT("On"), TEXT("Off")),
         Build_Change(TEXT("T_Bark"), TEXT("Compression Settings"), TEXT("TC_Default"), TEXT("TC_Normalmap"))});

    Set_ChangeIncluded(Applicable, 1, false);

    auto Shared = Build_Plan(TEXT("SM_Rock"), TEXT("Flag Materials For Nanite"),
        {Build_Change(TEXT("M_Rock"), TEXT("Used With Nanite"), TEXT("Off"), TEXT("On"))});

    auto Risk = FCkOptimizationDebugger_PlannedEffect{};
    Risk.Description = TEXT("M_Rock is also used by 37 other package(s)");
    Risk.IsRisk = true;
    Shared.Effects.Add(Risk);

    auto Refused = Build_Plan(TEXT("SM_Old"), TEXT("Enable Nanite"), {});
    Refused.CanApply = false;
    Refused.RefusalReason = TEXT("SM_Old: Nanite is already enabled.");

    const auto Summary = Build_PlanSummary({Applicable, Shared, Refused});

    TestEqual(TEXT("Applicable fixes are counted"), Summary.ApplicableFixCount, 2);
    TestEqual(TEXT("Refused ones are counted separately"), Summary.RefusedFixCount, 1);
    TestEqual(TEXT("Ticked changes are counted"), Summary.IncludedChangeCount, 2);
    TestEqual(TEXT("Unticked ones are counted, not dropped"), Summary.ExcludedChangeCount, 1);
    TestEqual(TEXT("Risk effects are counted"), Summary.RiskEffectCount, 1);

    // Two ticked changes on ONE object is one affected object. A header that said "2 assets" over a single texture
    // would be a count the reader cannot reconcile with the list under it.
    TestEqual(TEXT("Affected objects are distinct"), Summary.AffectedObjectCount, 2);

    const auto Paths = Get_AffectedObjectPaths({Applicable, Shared, Refused});

    TestEqual(TEXT("Only ticked changes contribute a path"), Paths.Num(), 2);

    // Sorted, because a list of affected assets that reordered itself between two identical previews is one the
    // reader stops reading top-down.
    TestTrue(TEXT("Paths are sorted"),
        Paths[0].ToString().Compare(Paths[1].ToString(), ESearchCase::CaseSensitive) < 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_FixPlan_Drift,
    "Ck.OptimizationDebugger.FixPlan.Drift",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_FixPlan_Drift::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_fixplan;
    using namespace ck_optimization_debugger_fixplan_spec;

    const auto Previewed = Build_Plan(TEXT("T_Bark"), TEXT("Disable sRGB"),
        {Build_Change(TEXT("T_Bark"), TEXT("sRGB"), TEXT("On"), TEXT("Off"))});

    TestFalse(TEXT("An identical plan has not drifted"), Get_HasDrifted(Previewed, Previewed));

    // A ticked change is not a drift: the reader's decision is what the ticks record, and carrying them over is the
    // whole point of comparing the two plans in the first place.
    auto Unticked = Previewed;
    Set_ChangeIncluded(Unticked, 0, false);
    TestFalse(TEXT("A cleared tick is not drift"), Get_HasDrifted(Previewed, Unticked));

    // The BEFORE value is the world. Applying the reader's decision to a value they were never shown is the silent
    // wrong write the preview exists to prevent.
    auto DifferentBefore = Previewed;
    DifferentBefore.Changes[0].BeforeText = TEXT("Off");
    TestTrue(TEXT("A changed before-value is drift"), Get_HasDrifted(Previewed, DifferentBefore));

    auto DifferentProperty = Previewed;
    DifferentProperty.Changes[0].PropertyLabel = TEXT("Mip Gen Settings");
    TestTrue(TEXT("A different property is drift"), Get_HasDrifted(Previewed, DifferentProperty));

    auto ExtraChange = Previewed;
    ExtraChange.Changes.Add(Build_Change(TEXT("T_Bark"), TEXT("Mip Gen Settings"), TEXT("NoMipmaps"), TEXT("FromTextureGroup")));
    TestTrue(TEXT("A different change count is drift"), Get_HasDrifted(Previewed, ExtraChange));

    auto NowRefused = Previewed;
    NowRefused.CanApply = false;
    TestTrue(TEXT("A fix that has started refusing is drift"), Get_HasDrifted(Previewed, NowRefused));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_FixPlan_CommitMessage,
    "Ck.OptimizationDebugger.FixPlan.CommitMessage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_FixPlan_CommitMessage::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_fixplan;
    using namespace ck_optimization_debugger_fixplan_spec;

    TestTrue(TEXT("An empty log produces no message"), Build_CommitMessage({}).IsEmpty());

    // A failed fix wrote nothing, so it has no place in a message describing what a commit contains.
    TestTrue(TEXT("A log of only failures produces no message"),
        Build_CommitMessage({Build_LogEntry(TEXT("Enable Nanite"), TEXT("SM_Rock"), false)}).IsEmpty());

    const auto Message = Build_CommitMessage({
        Build_LogEntry(TEXT("Enable Nanite"), TEXT("SM_Rock"), true),
        Build_LogEntry(TEXT("Enable Nanite"), TEXT("SM_Cliff"), true),
        Build_LogEntry(TEXT("Disable sRGB"), TEXT("T_Mask"), true),
        Build_LogEntry(TEXT("Disable sRGB"), TEXT("T_Broken"), false)});

    TestTrue(TEXT("The message counts what was applied"), Message.Contains(TEXT("Applied 3 optimization fix(es)")));

    // Grouped by the fix's own verb: "Enable Nanite" over two meshes is ONE decision a reader wants stated once,
    // not two lines that say the same thing.
    TestTrue(TEXT("Fixes are grouped by verb with a count"), Message.Contains(TEXT("- Enable Nanite (2)")));
    TestTrue(TEXT("...naming the targets"), Message.Contains(TEXT("SM_Rock, SM_Cliff")));
    TestTrue(TEXT("...and the second verb gets its own line"), Message.Contains(TEXT("- Disable sRGB (1)")));
    TestFalse(TEXT("A failed target is not named"), Message.Contains(TEXT("T_Broken")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
