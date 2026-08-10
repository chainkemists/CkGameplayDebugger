#pragma once

#include "CkDebuggerPage_Base.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_ArchetypeAggregation.h"

class SVerticalBox;
class SUniformGridPanel;
class STextBlock;

// --------------------------------------------------------------------------------------------------------------------
// Archetype-map lens (redesign spec §3.6 Overview): hero counts + one card per archetype
// (registered game archetypes tagged GAME and shown first; unregistered populations fall
// back to inferred base-name + signature keys). Card click pushes `arch:<name>` into the
// entity list's Filter bar. Rebuilds only while active and only on cache churn.
//
// v1 deviations (recorded in PROGRESS.md): no sparklines, no population-by-family bars —
// counts + signature badges only.
//
// Card widgets keep stable identity across refreshes (spec §4): counts update in place
// on the cached text block; the wrap box is only re-slotted when the archetype key set
// (or its order) changes — recreating cards every tick flickers.
// --------------------------------------------------------------------------------------------------------------------

class FCkDebuggerPage_Archetypes : public ICkDebuggerPage_Base
{
public:
    ~FCkDebuggerPage_Archetypes();

    auto Get_PageName() const -> FText override;
    auto Get_PageIcon() const -> const FSlateBrush* override;
    auto Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget> override;
    auto Tick(float InDeltaTime) -> void override;
    auto IsActive() const -> bool override;
    auto Set_IsActive(bool InIsActive) -> void override;
    auto OnStyleRevisionChanged() -> void override;

private:
    struct FCardCacheEntry
    {
        TSharedPtr<SWidget> CardWidget;
        TSharedPtr<STextBlock> CountText;
        int32 LastCount = 0;
    };

    auto RebuildCards() -> void;
    auto DoCreateCard(
        const ck::ecs_debugger_aggregation::FArchetypeBucket& InBucket,
        FCardCacheEntry& OutEntry) -> void;

    /** Adds/removes `arch:<token>` in the entity list's filter (cards multi-select by ORing). */
    auto Toggle_ArchFilterToken(const FString& InToken, bool InEnable) -> void;

    /** Re-derives ActiveArchTokens from the live filter text (no-op while unchanged). */
    auto RefreshActiveArchTokens() -> void;

    /** Persisted grid density (settings-backed, header chips mutate it). */
    static auto Get_GridColumns() -> int32;
    auto Set_GridColumns(int32 InColumns) -> void;

    bool IsActivePage = false;
    float TimeSinceRebuild = 0.0f;

    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TFunction<void(const FString&)> RequestEntityFilter;
    TFunction<FString()> GetEntityFilter;

    TSharedPtr<STextBlock> HeroText;
    TSharedPtr<SUniformGridPanel> CardsBox;

    TMap<FString, FCardCacheEntry> CardCache;
    TArray<FString> PresentedKeys;
    int32 LastSlottedColumns = 0;

    /** Lowercased arch: term values currently in the filter — drives card checked state. */
    TSet<FString> ActiveArchTokens;
    TOptional<FString> LastSeenFilterText;
};
