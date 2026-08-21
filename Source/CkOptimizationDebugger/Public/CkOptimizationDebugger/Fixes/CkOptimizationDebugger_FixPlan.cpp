#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_FixPlan.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Containers/Set.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_fixplan
{
    auto
        Get_IncludedChanges(
            const FCkOptimizationDebugger_FixPlan& InPlan)
        -> TArray<FCkOptimizationDebugger_PlannedChange>
    {
        return InPlan.Changes.FilterByPredicate([](const FCkOptimizationDebugger_PlannedChange& InChange) -> bool
        {
            return InChange.Included;
        });
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_HasIncludedWork(
            const FCkOptimizationDebugger_FixPlan& InPlan)
        -> bool
    {
        if (NOT InPlan.CanApply)
        { return false; }

        // A fix with no property rows at all — deleting an actor, selecting lights, converting placements — is
        // entirely described by its effects, and there is nothing for the reader to untick. Treating "no changes"
        // as "nothing to do" would silently disable every destructive fix in the catalogue.
        if (InPlan.Changes.IsEmpty())
        { return true; }

        return InPlan.Changes.ContainsByPredicate([](const FCkOptimizationDebugger_PlannedChange& InChange) -> bool
        {
            return InChange.Included;
        });
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_IsChangeIncluded(
            const FCkOptimizationDebugger_FixPlan& InPlan,
            const FSoftObjectPath& InObjectPath,
            const FString& InPropertyLabel)
        -> bool
    {
        const auto* Found = InPlan.Changes.FindByPredicate(
            [&InObjectPath, &InPropertyLabel](const FCkOptimizationDebugger_PlannedChange& InChange) -> bool
            {
                return InChange.ObjectPath == InObjectPath && InChange.PropertyLabel == InPropertyLabel;
            });

        // A property the plan never listed cannot be written — that is the construction that keeps an apply from
        // writing something the preview did not show.
        return Found != nullptr && Found->Included;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Set_ChangeIncluded(
            FCkOptimizationDebugger_FixPlan& InOutPlan,
            int32 InChangeIndex,
            bool InIncluded)
        -> void
    {
        if (NOT InOutPlan.Changes.IsValidIndex(InChangeIndex))
        { return; }

        InOutPlan.Changes[InChangeIndex].Included = InIncluded;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Set_AllChangesIncluded(
            FCkOptimizationDebugger_FixPlan& InOutPlan,
            bool InIncluded)
        -> void
    {
        for (auto& Change : InOutPlan.Changes)
        { Change.Included = InIncluded; }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_PlanSummary(
            const TArray<FCkOptimizationDebugger_FixPlan>& InPlans)
        -> FCkOptimizationDebugger_PlanSummary
    {
        auto Summary = FCkOptimizationDebugger_PlanSummary{};

        auto AffectedPaths = TSet<FSoftObjectPath>{};

        for (const auto& Plan : InPlans)
        {
            if (NOT Plan.CanApply)
            {
                ++Summary.RefusedFixCount;
                continue;
            }

            ++Summary.ApplicableFixCount;

            for (const auto& Change : Plan.Changes)
            {
                if (NOT Change.Included)
                {
                    ++Summary.ExcludedChangeCount;
                    continue;
                }

                ++Summary.IncludedChangeCount;
                AffectedPaths.Add(Change.ObjectPath);
            }

            for (const auto& Effect : Plan.Effects)
            {
                if (Effect.IsRisk)
                { ++Summary.RiskEffectCount; }
            }
        }

        Summary.AffectedObjectCount = AffectedPaths.Num();

        return Summary;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_AffectedObjectPaths(
            const TArray<FCkOptimizationDebugger_FixPlan>& InPlans)
        -> TArray<FSoftObjectPath>
    {
        auto Paths = TArray<FSoftObjectPath>{};

        for (const auto& Plan : InPlans)
        {
            if (NOT Plan.CanApply)
            { continue; }

            for (const auto& Change : Plan.Changes)
            {
                if (NOT Change.Included || Change.ObjectPath.IsNull())
                { continue; }

                Paths.AddUnique(Change.ObjectPath);
            }
        }

        // A `TSet`'s iteration order follows its hash layout, and a list of affected assets that reordered itself
        // between two identical previews is a list the reader stops reading top-down.
        Paths.Sort([](const FSoftObjectPath& InLhs, const FSoftObjectPath& InRhs)
        {
            return InLhs.ToString().Compare(InRhs.ToString(), ESearchCase::CaseSensitive) < 0;
        });

        return Paths;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_HasDrifted(
            const FCkOptimizationDebugger_FixPlan& InPreviewed,
            const FCkOptimizationDebugger_FixPlan& InFresh)
        -> bool
    {
        // A fix that has since started refusing has drifted by definition, and so has one whose change list is a
        // different shape — a texture that gained a second problem between preview and press is not the texture
        // the reader ticked.
        if (InFresh.CanApply != InPreviewed.CanApply)
        { return true; }

        if (InFresh.Changes.Num() != InPreviewed.Changes.Num())
        { return true; }

        for (auto Index = 0; Index < InPreviewed.Changes.Num(); ++Index)
        {
            const auto& Previewed = InPreviewed.Changes[Index];
            const auto& Fresh = InFresh.Changes[Index];

            if (Previewed.ObjectPath != Fresh.ObjectPath || Previewed.PropertyLabel != Fresh.PropertyLabel)
            { return true; }

            // The BEFORE value is the one that matters. The after is what this fix intends to write and cannot have
            // changed under it; the before is the world, and applying the reader's decision to a value they were
            // never shown is the silent wrong write the preview exists to prevent.
            if (Previewed.BeforeText != Fresh.BeforeText)
            { return true; }
        }

        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_CommitMessage(
            const TArray<FCkOptimizationDebugger_FixLogEntry>& InEntries)
        -> FString
    {
        const auto Applied = InEntries.FilterByPredicate([](const FCkOptimizationDebugger_FixLogEntry& InEntry) -> bool
        {
            return InEntry.Succeeded;
        });

        if (Applied.IsEmpty())
        { return FString{}; }

        auto Lines = TArray<FString>{};
        Lines.Add(ck::Format_UE(TEXT("Applied {} optimization fix(es):"), Applied.Num()));
        Lines.Add(FString{});

        // Grouped by the fix's own verb, because that is the sentence a reader wants once per group rather than
        // once per asset — "Enable Nanite" over eleven meshes is one decision, not eleven.
        auto SeenVerbs = TArray<FString>{};

        for (const auto& Entry : Applied)
        { SeenVerbs.AddUnique(Entry.FixVerb); }

        for (const auto& Verb : SeenVerbs)
        {
            auto Targets = TArray<FString>{};

            for (const auto& Entry : Applied)
            {
                if (Entry.FixVerb == Verb)
                { Targets.AddUnique(Entry.TargetLabel); }
            }

            Lines.Add(ck::Format_UE(TEXT("- {} ({}): {}"), Verb, Targets.Num(), FString::Join(Targets, TEXT(", "))));
        }

        return FString::Join(Lines, TEXT("\n"));
    }
}

// --------------------------------------------------------------------------------------------------------------------
