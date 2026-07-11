#pragma once

#include "CkDebuggerPage_Base.h"
#include "CkEcs/Handle/CkHandle.h"

class SVerticalBox;
class SWrapBox;
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

private:
    struct FArchetypeBucket
    {
        FString Key;            // aggregation key — also the card-cache identity
        FString DisplayName;
        FString FilterToken;    // pushed as arch:<token> on click
        int32 Count = 0;
        uint64 SignatureBits = 0;
        bool IsRegistered = false;
        FName IconName;         // registered archetypes may carry a bespoke glyph id
    };

    struct FCardCacheEntry
    {
        TSharedPtr<SWidget> CardWidget;
        TSharedPtr<STextBlock> CountText;
        int32 LastCount = 0;
    };

    auto RebuildCards() -> void;
    auto DoCreateCard(const FArchetypeBucket& InBucket, FCardCacheEntry& OutEntry) -> void;

    bool IsActivePage = false;
    float TimeSinceRebuild = 0.0f;

    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TFunction<void(const FString&)> RequestEntityFilter;

    TSharedPtr<STextBlock> HeroText;
    TSharedPtr<SWrapBox> CardsBox;

    TMap<FString, FCardCacheEntry> CardCache;
    TArray<FString> PresentedKeys;
};
