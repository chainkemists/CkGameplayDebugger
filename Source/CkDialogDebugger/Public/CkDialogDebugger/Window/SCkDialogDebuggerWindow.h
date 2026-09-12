#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkDialogDebugger/Data/CkDialogDebugger_DataCollector.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkUiCollection;
class FCkUiView;
class SBox;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------

/**
 * Retained authored Dialog debugger surface. The native window keeps its Chrome ownership while this class owns
 * collection projection, command routing, and every world-lifetime admission decision.
 */
class CKDIALOGDEBUGGER_API SCkDialogDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkDialogDebuggerWindow) {}
    SLATE_END_ARGS()

    virtual ~SCkDialogDebuggerWindow();

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("Dialog")); }

    /** Test-facing retained surface; callers may inspect but never mutate its bindings. */
    auto Get_DialogView() const -> TSharedPtr<FCkUiView> { return _DialogView; }
    /** Test-facing projection model; records are atomically published from the live collector snapshot. */
    auto Get_CooldownCollection() const -> TSharedPtr<const FCkUiCollection> { return _CooldownCollection; }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    auto DoGet_PieWorld() const -> UWorld*;
    auto DoExecCommand(const FString& InCommand) -> void;
    auto DoPassesFilter(const FString& InText) const -> bool;
    auto DoBuild_DiagnosticText() const -> FText;
    auto DoProject_Cooldowns() -> void;
    auto DoBuild_DialogView() -> void;
    auto DoInvalidate_DialogView() -> void;
    auto DoPoll_DialogFiles(double InCurrentTime) -> void;
    auto CanUse_DialogView(int64 InGeneration) const -> bool;
    auto CanDispatch_Dialog(int64 InGeneration) const -> bool;
    auto HandleSessionInvalidated() -> void;
    auto HandleWorldInvalidated(UWorld* InWorld) -> void;

    FCkDialogDebugger_DataCollector _Collector;
    TSharedPtr<FCkUiCollection> _CooldownCollection;
    TSharedPtr<FCkUiView> _DialogView;
    TSharedPtr<SBox> _DialogHost;
    TSharedPtr<SBox> _CooldownControlsHost;
    TSharedPtr<SBox> _RuntimeCommandsHost;
    TSharedPtr<SBox> _SearchHost;

    TWeakObjectPtr<UWorld> _ObservedWorld;
    int64 _DialogGeneration = 0;
    double _NextDialogPollSeconds = 0.0;
    int32 _VisibleCooldownCount = 0;

    FString _FilterString;
    FString _HighlightString;
    bool _ShowActiveCooldownsOnly = false;
    FDelegateHandle _SessionInvalidatedHandle;
    FDelegateHandle _WorldInvalidatedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
