#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Lighting.h"

#if WITH_EDITOR

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Engine/EngineTypes.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_checks_lighting
{
    auto
        Run_Checks(
            const FCkOptimizationDebugger_ScanContext& InContext,
            const FCkOptimizationDebugger_Thresholds& InThresholds,
            TArray<FCkOptimizationDebugger_FindingRow>& OutFindings)
        -> void
    {
        using namespace ck_optimization_debugger_scan;
        using namespace ck_optimization_debugger_thresholds;

        // ---- Lighting.MovableLightCount ----
        // A per-LEVEL finding, targeted at the level ASSET: the problem is not any one light, it is how many of
        // them share a room. Targeting the level also keeps the stable key distinct per sub-level, which a finding
        // with no target at all would not be.
        for (const auto& Level : InContext.Levels)
        {
            if (Level.MovableLightCount <= InThresholds.MaxMovableLights)
            { continue; }

            const auto Graded = Get_Graded(Level.MovableLightCount, InThresholds.MaxMovableLights,
                ECkOptimizationDebugger_Severity::Major);

            auto Finding = Build_Finding(FName{TEXT("Lighting.MovableLightCount")},
                Graded.Severity,
                ECkOptimizationDebugger_Category::Lighting,
                Build_AssetTarget(Level.LevelPath, Level.LevelName),
                TEXT("Too many movable lights in one level"),
                ck::Format_UE(TEXT("{} movable lights against a budget of {}. Dynamic shadow cost scales with how many of them OVERLAP, not with the count alone, so this number is a ceiling on the worst view rather than an average."),
                    Level.MovableLightCount, InThresholds.MaxMovableLights),
                TEXT("Make the lights that never move Stationary or Static, and bake what can be baked."));

            Finding.BudgetRatio = Graded.BudgetRatio;

            Finding.HasAutoFix = true;

            // States the verb the button actually performs. It SELECTS the movable-light actors in the outliner and
            // changes nothing — a light that genuinely moves must stay Movable, and no offline rule can tell which
            // those are. Wording it as "review light mobility" left the reader expecting a mobility change.
            Finding.FixDescription = TEXT("Select every movable-light actor in this level so you can judge them together. This changes NOTHING on its own — set the ones that never move to Stationary or Static yourself, because a light that genuinely moves must stay Movable.");

            OutFindings.Add(MoveTemp(Finding));
        }

        // ---- Lighting.LightmapResolution ----
        // The override lives on the COMPONENT, so the finding belongs to the actor that carries it, not to the mesh
        // asset — two placements of one mesh can disagree about their lightmap budget.
        //
        // AGGREGATED PER ACTOR, and that is load-bearing rather than cosmetic. A stable key is
        // `check id | target path`, and the target here is the ACTOR — so one actor carrying two over-budget static
        // mesh components would emit two findings sharing a key. The window reuses list rows BY that key: a
        // duplicate makes every rebuild allocate a fresh row for the second one, which fires `RequestListRefresh`
        // on every keystroke in the filter box and leaves that row unable to hold a selection. One actor, one row.
        struct FActorOverride
        {
            const FCkOptimizationDebugger_ScanMeshUsage* Worst = nullptr;
            int32 ComponentCount = 0;
        };

        auto OverridesByActor = TMap<FString, FActorOverride>{};

        // First-appearance order, so the emitted sequence does not depend on the map's hash layout. The findings are
        // re-sorted on store with a total tie-break anyway; this keeps the check itself deterministic in isolation,
        // which is what a spec over it can assert.
        auto ActorOrder = TArray<FString>{};

        for (const auto& Usage : InContext.MeshUsages)
        {
            if (NOT Usage.OverridesLightMapRes)
            { continue; }

            if (Usage.OverriddenLightMapRes <= InThresholds.MaxLightmapResolution)
            { continue; }

            const auto ActorKey = Usage.ActorPath.ToString();

            auto* Existing = OverridesByActor.Find(ActorKey);

            if (Existing == nullptr)
            {
                auto Entry = FActorOverride{};
                Entry.Worst = &Usage;
                Entry.ComponentCount = 1;

                OverridesByActor.Add(ActorKey, MoveTemp(Entry));
                ActorOrder.Add(ActorKey);
                continue;
            }

            ++Existing->ComponentCount;

            // The worst override is what the finding is graded and worded by — reporting the first one found would
            // make the severity depend on component iteration order.
            if (Existing->Worst == nullptr ||
                Usage.OverriddenLightMapRes > Existing->Worst->OverriddenLightMapRes)
            { Existing->Worst = &Usage; }
        }

        for (const auto& ActorKey : ActorOrder)
        {
            const auto* Entry = OverridesByActor.Find(ActorKey);

            if (Entry == nullptr || Entry->Worst == nullptr)
            { continue; }

            const auto& Usage = *Entry->Worst;

            const auto MeshWord = Usage.MeshDisplayName.IsEmpty()
                ? FString{TEXT("its mesh")}
                : Usage.MeshDisplayName;

            // One component is the ordinary case and reads as one sentence; several is a different fact about the
            // actor and says so rather than silently reporting only the worst.
            const auto Explanation = Entry->ComponentCount == 1
                ? ck::Format_UE(TEXT("This placement of {} overrides its lightmap resolution to {} against a budget of {}. Lightmap memory and bake time both scale with the square of that number, so it should track the surface's screen area rather than its importance."),
                    MeshWord,
                    Usage.OverriddenLightMapRes,
                    InThresholds.MaxLightmapResolution)
                : ck::Format_UE(TEXT("{} static mesh components on this actor override their lightmap resolution past the budget of {}; the worst ({}) is set to {}. Lightmap memory and bake time both scale with the square of that number, so it should track the surface's screen area rather than its importance."),
                    Entry->ComponentCount,
                    InThresholds.MaxLightmapResolution,
                    MeshWord,
                    Usage.OverriddenLightMapRes);

            const auto Graded = Get_Graded(Usage.OverriddenLightMapRes, InThresholds.MaxLightmapResolution,
                ECkOptimizationDebugger_Severity::Major);

            auto Finding = Build_Finding(FName{TEXT("Lighting.LightmapResolution")},
                Graded.Severity,
                ECkOptimizationDebugger_Category::Lighting,
                Build_ActorTarget(Usage.ActorPath, Usage.ActorDisplayName, Usage.LevelName),
                TEXT("Lightmap resolution override over budget"),
                Explanation,
                TEXT("Lower the override, or clear it and let the mesh asset's own lightmap resolution apply."));

            Finding.BudgetRatio = Graded.BudgetRatio;

            Finding.HasAutoFix = true;
            Finding.FixDescription = TEXT("Clamp the override to the budget on every over-budget component of this actor.");

            OutFindings.Add(MoveTemp(Finding));
        }
    }
}

#endif

// --------------------------------------------------------------------------------------------------------------------
