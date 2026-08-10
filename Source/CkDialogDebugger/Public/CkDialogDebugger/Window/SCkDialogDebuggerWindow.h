#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkDialogDebugger/Data/CkDialogDebugger_DataCollector.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkDebug_MeterBar;
class STextBlock;
class SVerticalBox;
class UWorld;
struct FCkDialogDebugger_CooldownInfo;

// --------------------------------------------------------------------------------------------------------------------

// One cooling line's retained row. Updated IN PLACE every gated tick; only a change in WHICH lines are cooling
// rebuilds the widgets (see _LastCooldownSignature).
//
// The meter is attribute-bound rather than setter-driven — that is what SCkDebug_MeterBar is built for ("both
// attribute-bound so live data animates without rebuilds"). The bound lambdas read these shared cells, so the values
// can be refreshed without touching the widget at all. Shared cells rather than indices into the slot array because
// the array reallocates as rows are added during a rebuild.
struct FCkDialogDebugger_CooldownSlot
{
    TSharedPtr<float>        Fraction;
    TSharedPtr<FLinearColor> FillColor;
    TSharedPtr<STextBlock>   RemainingText;
};

// --------------------------------------------------------------------------------------------------------------------

// Read-only inspector window for the Dialog system: registry lines + banks, and every emitter's tags, active
// cooldowns, and last-query pass/fail summary. Rebuilds its content each gated refresh tick. No graph (deferred).
class SCkDialogDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkDialogDebuggerWindow) {}
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
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Dialog Debugger")); }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    auto
    DoRebuildContent() -> void;

    // The cooldown section is real Slate rather than part of the text dump: a meter is the whole point of it, and a
    // monospace bar cannot show sub-second progress legibly.
    //
    // Split structure from values, the way SCkInputDebuggerWindow does. The refresh gate defaults to Unlimited, so
    // this runs EVERY FRAME — rebuilding the subtree that often tears down and recreates live widgets mid-layout,
    // which reads on screen as violent flicker and text drawn over itself.
    auto
    DoRebuildCooldowns_Structure() -> void;

    auto
    DoUpdateCooldowns_LiveValues() -> void;

    // Identity of the CURRENT cooling set (emitter + line ids). Structure is rebuilt only when this changes.
    auto
    DoBuild_CooldownSignature() const -> FString;

    auto
    DoMake_CooldownRow(
        const FCkDialogDebugger_CooldownInfo& InCooldown,
        FCkDialogDebugger_CooldownSlot& OutSlot) const -> TSharedRef<SWidget>;

    static auto
    DoGet_CooldownFraction(
        const FCkDialogDebugger_CooldownInfo& InCooldown) -> float;

    auto
    DoPassesFilter(
        const FString& InText) const -> bool;

    auto
    DoGet_PieWorld() const -> UWorld*;

    // Runs a console command (e.g. Ck_Save / Ck_Load) in the PIE world as if typed in the console — routes through
    // the local PlayerController so AS UFUNCTION(Exec) commands are reached.
    auto
    DoExecCommand(
        const FString& InCommand) -> void;

    FCkDialogDebugger_DataCollector _Collector;
    TSharedPtr<STextBlock>          _ContentText;
    TSharedPtr<SVerticalBox>        _CooldownBox;
    TSharedPtr<STextBlock>          _CooldownCountText;

    // Retained rows, parallel to the flattened cooling set the signature describes.
    TArray<FCkDialogDebugger_CooldownSlot> _CooldownSlots;
    FString                                _LastCooldownSignature;

    FString                         _FilterString;
    FString                         _HighlightString;
    bool                            _ShowActiveCooldownsOnly = false;
};

// --------------------------------------------------------------------------------------------------------------------
