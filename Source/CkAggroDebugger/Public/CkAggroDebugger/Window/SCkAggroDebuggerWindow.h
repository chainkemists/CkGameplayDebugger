#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkAggroDebugger/Data/CkAggroDebugger_DataCollector.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkUiCollection;
class FCkUiView;
class SBox;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------

/** Retained authored Aggro surface. Native Chrome remains the window owner while this class owns
 * read-only collection projection, whole-owner filtering, and world-lifetime admission. */
class CKAGGRODEBUGGER_API SCkAggroDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkAggroDebuggerWindow) {}
    SLATE_END_ARGS()

    virtual ~SCkAggroDebuggerWindow();

    auto
    Construct(
        const FArguments& InArgs) -> void;

    auto
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("Aggro")); }

    /** Test-facing retained surface; callers may inspect but never mutate its bindings. */
    auto Get_AggroView() const -> TSharedPtr<FCkUiView> { return _AggroView; }
    /** Test-facing projection model; records are atomically published from the live collector snapshot. */
    auto Get_AggroCollection() const -> TSharedPtr<const FCkUiCollection> { return _AggroCollection; }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    auto DoGet_PieWorld() const -> UWorld*;
    auto DoPassesFilter(const FString& InText) const -> bool;
    auto DoPassesFilter(const FCkAggroDebugger_OwnerInfo& InOwner) const -> bool;

    auto DoPassesEngagedFilter(const FCkAggroDebugger_OwnerInfo& InOwner) const -> bool;
    auto DoProject_Aggro() -> void;
    auto DoBuild_AggroView() -> void;
    auto DoInvalidate_AggroView() -> void;
    auto DoPoll_AggroFiles(double InCurrentTime) -> void;
    auto CanUse_AggroView(int64 InGeneration) const -> bool;
    auto HandleSessionInvalidated() -> void;
    auto HandleWorldInvalidated(UWorld* InWorld) -> void;

    static auto DoGet_ThreatColor(const FCkAggroDebugger_TargetInfo& InTarget) -> FLinearColor;
    static auto DoBuild_StateText(const FCkAggroDebugger_TargetInfo& InTarget) -> FString;
    static auto DoBuild_DetailText(const FCkAggroDebugger_TargetInfo& InTarget) -> FString;

    FCkAggroDebugger_DataCollector _Collector;

    TSharedPtr<FCkUiCollection> _AggroCollection;
    TSharedPtr<FCkUiView> _AggroView;
    TSharedPtr<SBox> _AggroHost;
    TSharedPtr<SBox> _ControlsHost;
    TSharedPtr<SBox> _OverviewHost;
    TSharedPtr<SBox> _SearchHost;

    TWeakObjectPtr<UWorld> _ObservedWorld;
    int64 _AggroGeneration = 0;
    double _NextAggroPollSeconds = 0.0;

    FString _FilterString;
    FString _HighlightString;
    bool _ShowEngagedOwnersOnly = false;
    FDelegateHandle _SessionInvalidatedHandle;
    FDelegateHandle _WorldInvalidatedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
