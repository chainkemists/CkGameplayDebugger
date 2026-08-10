#pragma once

#include "CkDebuggerPage_Base.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_ArchetypeAggregation.h"

class SCkEcsDebugger_Sparkline;
class STextBlock;
class SVerticalBox;
class SUniformGridPanel;
class SBox;
class FCkEcsDebuggerDashboard_ContentRebuildInvalidatesPresentation;

// --------------------------------------------------------------------------------------------------------------------
// Overview dashboard (redesign spec §3.6, mockup parity): hero count + live sparkline,
// population-by-family bars, top archetype cards (icon, count, badges, family, per-card
// sparkline), singleton list with entity pills. The hierarchy graph moved to its own
// "Graph" tab.
//
// All widget caches keep stable identity: counts/bars/sparklines update in place at 1 Hz
// while visible; containers only re-slot when their key set changes (spec §4).
// --------------------------------------------------------------------------------------------------------------------

class FCkDebuggerPage_Dashboard : public ICkDebuggerPage_Base
{
public:
    auto Get_PageName() const -> FText override;
    auto Get_PageIcon() const -> const FSlateBrush* override;
    auto Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget> override;
    auto Tick(float InDeltaTime) -> void override;
    auto IsActive() const -> bool override;
    auto Set_IsActive(bool InIsActive) -> void override;

private:
    friend class FCkEcsDebuggerDashboard_ContentRebuildInvalidatesPresentation;

    struct FCardEntry
    {
        TSharedPtr<SWidget> CardWidget;
        TSharedPtr<STextBlock> CountText;
        TSharedPtr<TArray<float>> Samples;
        int32 LastCount = 0;
    };

    struct FFamilyEntry
    {
        TSharedPtr<SWidget> RowWidget;
        TSharedPtr<SBox> BarBox;
        TSharedPtr<STextBlock> CountText;
        int32 LastCount = 0;
    };

    auto DoRefresh() -> void;
    auto DoCreateCard(
        const ck::ecs_debugger_aggregation::FArchetypeBucket& InBucket,
        FCardEntry& OutEntry) -> void;
    auto DoCreateFamilyRow(FName InFamily, FFamilyEntry& OutEntry) -> void;
    auto RefreshActiveArchTokens() -> void;

    bool IsActivePage = false;
    float TimeSinceRefresh = 0.0f;

    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TFunction<void(const FString&)> RequestEntityFilter;
    TFunction<FString()> GetEntityFilter;

    TSharedPtr<STextBlock> HeroCountText;
    TSharedPtr<STextBlock> HeroSubText;
    TSharedPtr<TArray<float>> TotalSamples;
    TSharedPtr<SVerticalBox> FamilyBox;
    TSharedPtr<SUniformGridPanel> CardsGrid;
    TSharedPtr<SVerticalBox> SingletonsBox;

    TMap<FString, FCardEntry> CardCache;
    TArray<FString> PresentedCardKeys;
    TMap<FName, FFamilyEntry> FamilyCache;
    TArray<FName> PresentedFamilies;
    TArray<FString> PresentedSingletonKeys;

    /**
     * How many Count==1 archetypes the MaxSingletons cap hid at the last re-slot. Part of the
     * container's change key: the visible keys can be identical while the hidden remainder moves,
     * and a stale "+N more" is exactly the silent miscount the indicator exists to prevent.
     */
    int32 PresentedSingletonOverflow = 0;

    /** Lowercased arch: term values currently in the filter — card checked state. */
    TSet<FString> ActiveArchTokens;
    TOptional<FString> LastSeenFilterText;
};
