#pragma once

#include "CkAiDebugger/Model/CkAiDebugger_EvidenceModel.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"

class FCkDebug_ViewportPicker;
class FCkCrowdDebugger_ViewModel;
class SCkDebug_EventLog;
class SCkDebug_EntityHealthList;
class SCkDebug_EvidenceList;
class SCkCrowdDebugger_3dViewport;

/** Composition-only AI overview; all rows and controls are supplied by Common or the overlay model. */
class SCkAiDebuggerWindow final : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkAiDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkAiDebuggerWindow() override;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("AI Overview")); }

    static auto Is_AiEntity(const FCk_Handle& InEntity) -> bool;
    static auto OpenForEntity(const FCk_Handle& InEntity) -> void;
    auto Select_Entity(const FCk_Handle& InEntity, bool InBroadcast) -> void;

    /** Pure policy seam covered by module specs; presentation never reads feature fragments. */
    static auto Is_AiModel(const FCk_DebugOverlay_EntityModel& InModel) -> bool;

private:
    auto Build_Model(const FCk_Handle& InEntity, double InNow) -> FCk_DebugOverlay_EntityModel;
    auto Build_Body() -> TSharedRef<SWidget>;
    auto Build_OverviewPane() -> TSharedRef<SWidget>;
    auto Build_IdentityPanel() -> TSharedRef<SWidget>;
    auto Build_StagePanel() -> TSharedRef<SWidget>;
    auto Get_StatusText() const -> FText;
    auto Refresh_Roster() -> void;
    auto Refresh_Diagnostics(double InNow) -> void;
    auto Clear_Diagnostics() -> void;
    auto HandleSessionInvalidated() -> void;
    auto HandleWorldInvalidated(UWorld* InWorld) -> void;
    auto Get_TargetWorld() const -> UWorld*;

    FCk_Handle _SelectedEntity;
    TArray<FCk_Handle> _TrackedRoster;
    FCk_DebugOverlay_EntityModel _Model;
    FCk_DebugOverlay_History _History;
    TSharedPtr<FCkDebug_ViewportPicker> _ViewportPicker;
    TSharedPtr<FCkCrowdDebugger_ViewModel> _CrowdViewModel;
    TSharedPtr<SCkCrowdDebugger_3dViewport> _SpatialViewport;
    TSharedPtr<SCkDebug_EventLog> _EventLog;
    TSharedPtr<SCkDebug_EntityHealthList> _HealthList;
    TSharedPtr<SCkDebug_EvidenceList> _CurrentEvidenceList;
    TSharedPtr<SCkDebug_EvidenceList> _GoapTopologyList;
    TSharedPtr<SCkDebug_EvidenceList> _StateMachineTopologyList;
    FCkAiDebugger_EvidenceDeltaTracker _EvidenceDeltaTracker;
    double _LastRefreshTime = -1.0;
    FDelegateHandle _SessionInvalidatedHandle;
    FDelegateHandle _WorldInvalidatedHandle;
    TWeakObjectPtr<UWorld> _ActiveWorld;
};
