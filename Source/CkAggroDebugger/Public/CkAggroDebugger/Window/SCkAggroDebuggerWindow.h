#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkAggroDebugger/Data/CkAggroDebugger_DataCollector.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkDebug_MeterBar;
class SCkDebug_StatPair;
class SCkDebug_StatusPill;
class STextBlock;
class SVerticalBox;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------

// One tracked target's retained row. Everything that changes per frame is a shared cell the bound lambdas read, so a
// refresh writes cells and never touches the widget tree — the meters animate without a rebuild.
//
// Shared cells rather than indices into the slot array because the array reallocates while rows are added during a
// structure pass.
struct FCkAggroDebugger_TargetSlot
{
    TSharedPtr<float>        ThreatFraction;
    TSharedPtr<float>        ScoreFraction;
    TSharedPtr<FLinearColor> ThreatColor;
    TSharedPtr<FLinearColor> ScoreColor;

    TSharedPtr<STextBlock> ThreatText;
    TSharedPtr<STextBlock> ScoreText;
    TSharedPtr<STextBlock> StateText;
    TSharedPtr<STextBlock> DetailText;
};

// --------------------------------------------------------------------------------------------------------------------

// One owner's retained header cells (the per-owner line above its target rows).
struct FCkAggroDebugger_OwnerSlot
{
    TSharedPtr<STextBlock> ActiveText;
    TSharedPtr<STextBlock> MetaText;
};

// --------------------------------------------------------------------------------------------------------------------

// Read-only inspector for the CkAggro threat model: every owner's threat table, rendered as live meters so relative
// threat and the switch margin are readable at a glance rather than reconstructed from numbers.
//
// Aggro is authority-only — on a client there are no owners to show, and the window says that explicitly rather than
// rendering an empty list that looks like a bug.
class SCkAggroDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkAggroDebuggerWindow) {}
    SLATE_END_ARGS()

    auto
    Construct(
        const FArguments& InArgs) -> void;

    auto
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Aggro Debugger")); }

private:
    // Structure is rebuilt ONLY when the owner/target SET changes; values are written every gated tick. The refresh
    // gate can run every frame, and rebuilding this subtree that often tears down live widgets mid-layout — which
    // reads on screen as violent flicker and text drawn over itself (the lesson SCkDialogDebuggerWindow records).
    auto
    DoRebuild_Structure() -> void;

    auto
    DoUpdate_LiveValues() -> void;

    // Identity of the current owner/target set — deliberately excludes threat/score, which change every frame and
    // would defeat the gate entirely.
    auto
    DoBuild_Signature() const -> FString;

    auto
    DoMake_TargetRow(
        const FCkAggroDebugger_TargetInfo& InTarget,
        FCkAggroDebugger_TargetSlot& OutSlot) const -> TSharedRef<SWidget>;

    auto
    DoPassesFilter(
        const FString& InText) const -> bool;

    auto
    DoPassesFilter(
        const FCkAggroDebugger_OwnerInfo& InOwner) const -> bool;

    auto DoPassesEngagedFilter(const FCkAggroDebugger_OwnerInfo& InOwner) const -> bool;

    auto
    DoGet_PieWorld() const -> UWorld*;

    static auto
    DoGet_ThreatColor(
        const FCkAggroDebugger_TargetInfo& InTarget) -> FLinearColor;

    static auto
    DoBuild_StateText(
        const FCkAggroDebugger_TargetInfo& InTarget) -> FString;

    static auto
    DoBuild_DetailText(
        const FCkAggroDebugger_TargetInfo& InTarget) -> FString;

    FCkAggroDebugger_DataCollector _Collector;

    TSharedPtr<SVerticalBox>    _OwnerBox;
    TSharedPtr<SCkDebug_StatPair> _StatOwners;
    TSharedPtr<SCkDebug_StatPair> _StatEngaged;
    TSharedPtr<SCkDebug_StatPair> _StatTargets;
    TSharedPtr<STextBlock>      _StatusText;

    // Flat and parallel to the rows the structure pass emitted, in the same walk order. The signature guarantees
    // that correspondence: any change to the set rebuilds before the value pass runs.
    TArray<FCkAggroDebugger_TargetSlot> _TargetSlots;
    TArray<FCkAggroDebugger_OwnerSlot>  _OwnerSlots;

    FString _LastSignature;
    FString _FilterString;
    FString _HighlightString;
    bool _ShowEngagedOwnersOnly = false;
};

// --------------------------------------------------------------------------------------------------------------------
