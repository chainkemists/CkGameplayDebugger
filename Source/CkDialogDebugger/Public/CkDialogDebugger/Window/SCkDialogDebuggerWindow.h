#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkDialogDebugger/Data/CkDialogDebugger_DataCollector.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class STextBlock;
class SVerticalBox;
class UWorld;
struct FCkDialogDebugger_CooldownInfo;

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

private:
    auto
    DoRebuildContent() -> void;

    // The cooldown section is real Slate rather than part of the text dump: a meter is the whole point of it, and a
    // monospace bar cannot show sub-second progress legibly.
    auto
    DoRebuildCooldowns() -> void;

    static auto
    DoMake_CooldownRow(
        const FCkDialogDebugger_CooldownInfo& InCooldown) -> TSharedRef<SWidget>;

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
    FString                         _FilterString;
    FString                         _HighlightString;
};

// --------------------------------------------------------------------------------------------------------------------
