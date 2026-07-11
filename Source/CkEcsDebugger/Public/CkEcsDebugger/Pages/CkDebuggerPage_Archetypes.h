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
        FString DisplayName;
        FString FilterToken;    // pushed as arch:<token> on click
        int32 Count = 0;
        uint64 SignatureBits = 0;
        bool IsRegistered = false;
        FName IconName;         // registered archetypes may carry a bespoke glyph id
    };

    auto RebuildCards() -> void;

    bool IsActivePage = false;
    float TimeSinceRebuild = 0.0f;

    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TFunction<void(const FString&)> RequestEntityFilter;

    TSharedPtr<STextBlock> HeroText;
    TSharedPtr<SWrapBox> CardsBox;
};
