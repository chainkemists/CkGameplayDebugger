#pragma once

#include "CkEqsDebugger/Data/CkEqsDebugger_Types.h"
#include "CkEqsDebugger/Overlay/CkEqsDebugger_OverlayManager.h"

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

#include "CoreMinimal.h"
#include "Editor.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkEqsDebugger_ViewModel;
class SCkEqsDebugger_QueryList;
class SCkEqsDebugger_CandidatePanel;
class SCkEqsDebugger_TestBreakdownPanel;
class SCkDebug_SelectableLabel;

// --------------------------------------------------------------------------------------------------------------------
// Top-level debugger window. Three-pane layout (QueryList | CandidatePanel | TestBreakdownPanel) under a toolbar
// with icon toggles for the overlay, pause button, and refresh-rate controls. Also owns the in-world overlay manager
// that draws PMG sphere markers + querier marker + best-location line for the selected query.
// --------------------------------------------------------------------------------------------------------------------

class SCkEqsDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkEqsDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkEqsDebuggerWindow();

    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK EQS Debugger")); }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto BuildMenuActions() -> TSharedRef<SWidget>;

    auto OnEndPIE(const bool InWasSimulating) -> void;
    auto OnBeginPIE(const bool InIsSimulating) -> void;

    TSharedPtr<FCkEqsDebugger_ViewModel> _ViewModel;
    FCkEqsDebugger_OverlayManager        _OverlayManager;

    TSharedPtr<SCkEqsDebugger_QueryList>           _QueryList;
    TSharedPtr<SCkEqsDebugger_CandidatePanel>      _CandidatePanel;
    TSharedPtr<SCkEqsDebugger_TestBreakdownPanel>  _TestBreakdownPanel;

    TSharedPtr<SCkDebug_SelectableLabel> _StatusLabel;

    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    UWorld* _CachedWorld = nullptr;

    FDelegateHandle _EndPIEHandle;
    FDelegateHandle _BeginPIEHandle;
};

// --------------------------------------------------------------------------------------------------------------------
