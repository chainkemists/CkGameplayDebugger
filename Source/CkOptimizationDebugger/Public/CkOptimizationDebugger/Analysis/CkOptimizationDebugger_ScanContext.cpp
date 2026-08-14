#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ScanContext.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_ScanAsset::
    Get_ReferencingLevelsText() const
    -> FString
{
    return FString::Join(ReferencingLevelNames, TEXT(", "));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_ScanContext::
    Get_ScannedLevelNames() const
    -> TArray<FString>
{
    auto Names = TArray<FString>{};
    Names.Reserve(Levels.Num());

    for (const auto& Level : Levels)
    {
        Names.Add(Level.LevelName);
    }

    return Names;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_scan
{
    auto
        Build_Finding(
            FName InCheckId,
            ECkOptimizationDebugger_Severity InSeverity,
            ECkOptimizationDebugger_Category InCategory,
            const FCkOptimizationDebugger_Target& InTarget,
            const FString& InTitle,
            const FString& InExplanation,
            const FString& InRecommendation)
        -> FCkOptimizationDebugger_FindingRow
    {
        auto Finding = FCkOptimizationDebugger_FindingRow{};

        Finding.CheckId = InCheckId;
        Finding.Severity = InSeverity;
        Finding.Category = InCategory;
        Finding.Target = InTarget;
        Finding.Title = InTitle;
        Finding.Explanation = InExplanation;
        Finding.Recommendation = InRecommendation;
        Finding.StableKey = ck_optimization_debugger_model::Build_StableKey(InCheckId, InTarget);

        return Finding;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_AssetTarget(
            const FSoftObjectPath& InPath,
            const FString& InDisplayName)
        -> FCkOptimizationDebugger_Target
    {
        auto Target = FCkOptimizationDebugger_Target{};

        Target.Kind = ECkOptimizationDebugger_TargetKind::Asset;
        Target.Path = InPath;
        Target.DisplayName = InDisplayName;

        return Target;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_ActorTarget(
            const FSoftObjectPath& InActorPath,
            const FString& InDisplayName,
            const FString& InLevelName)
        -> FCkOptimizationDebugger_Target
    {
        auto Target = FCkOptimizationDebugger_Target{};

        Target.Kind = ECkOptimizationDebugger_TargetKind::Actor;
        Target.Path = InActorPath;
        Target.DisplayName = InDisplayName;
        Target.LevelName = InLevelName;

        return Target;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_ProjectSettingsTarget(
            const FString& InSectionName,
            const FString& InDisplayName)
        -> FCkOptimizationDebugger_Target
    {
        auto Target = FCkOptimizationDebugger_Target{};

        Target.Kind = ECkOptimizationDebugger_TargetKind::ProjectSettings;
        Target.SettingsSectionName = InSectionName;
        Target.DisplayName = InDisplayName;

        return Target;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_UsageSentence(
            const FCkOptimizationDebugger_ScanAsset& InAsset)
        -> FString
    {
        if (InAsset.ReferencingLevelNames.IsEmpty())
        { return FString{}; }

        return ck::Format_UE(TEXT(" Used {} time(s) in: {}."),
            InAsset.UsageCount,
            InAsset.Get_ReferencingLevelsText());
    }
}

// --------------------------------------------------------------------------------------------------------------------
