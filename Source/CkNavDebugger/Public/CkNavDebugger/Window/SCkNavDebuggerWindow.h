#pragma once

#include "CkNavDebugger/Data/CkNavDebugger_Types.h"

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkNavDebugger_ViewModel;
class SCkNavDebugger_AgentListPanel;
class SCkNavDebugger_FailureLogPanel;
class SCkDebug_StatusPill;
class SVerticalBox;

// --------------------------------------------------------------------------------------------------------------------
// Top-level CkNavDebugger window. Layout (built ONCE in Construct, content updates via
// TAttribute bindings — no per-tick widget tree rebuilds):
//
//   Toolbar
//   ┌──────────────────────────┬─────────────────────────────────────┐
//   │ Agent list               │ Navmesh status                       │
//   │                          ├─────────────────────────────────────┤
//   │                          │ Health check                         │
//   ├──────────────────────────┴─────────────────────────────────────┤
//   │ Selected-agent inspector                                        │
//   ├────────────────────────────────────────────────────────────────┤
//   │ Failure log                                                     │
//   └────────────────────────────────────────────────────────────────┘
// --------------------------------------------------------------------------------------------------------------------

class SCkNavDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkNavDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Navigation Debugger")); }

private:
    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto BuildNavmeshStatusBody() -> TSharedRef<SWidget>;
    auto BuildAgentInspectorBody() -> TSharedRef<SWidget>;
    auto BuildHealthCheckBody() -> TSharedRef<SWidget>;

    auto Rebuild_HealthCheck() -> void;

    TSharedPtr<FCkNavDebugger_ViewModel> _ViewModel;

    TSharedPtr<SCkNavDebugger_AgentListPanel>  _AgentListPanel;
    TSharedPtr<SCkNavDebugger_FailureLogPanel> _FailureLogPanel;

    // Health check rebuilds on demand (button click), not per tick — so a
    // managed SVerticalBox with ClearChildren+AddSlot is fine here.
    TSharedPtr<SVerticalBox> _HealthCheckBody;

    UWorld* _CachedWorld = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------
