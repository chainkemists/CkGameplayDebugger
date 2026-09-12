#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CommandBar.h"

// ====================================================================================================================

class FCkDebug_ViewportPicker;
class FCkGoapDebugger_ViewModel;
class SCkGoapDebugger_Sidebar;
class SCkGoapDebugger_WorldStateRail;
class SCkGoapDebugger_GraphPane;
class SCkGoapDebugger_AgentColumn;
class SCkGoapDebugger_DecisionPanel;
class SCkGoapDebugger_SearchTracePanel;
class SCkGoapDebugger_TimelineDock;
class SCkGoapDebugger_SquadTable;
class SCkGoapDebugger_CatalogPanel;
class FCkUiView;
class STextBlock;
class STextComboBox;
class SBox;

// ====================================================================================================================
// SCkGoapDebuggerWindow — top-level Slate widget hosted by the GoapDebugger
// NomadTab. Owns:
//
//   - The ViewModel (selection state, mode, snapshot batch).
//   - The Sidebar (Planner tree + history list).
//   - Center column (breadcrumb + primary + graph stub) and the WS rail.
//   - Toolbar (entity picker, Live/Scrub toggle, Force replan).
//   - Mode bar (Standalone vs ECS-Inspector — only Standalone implemented in D2).
//   - Legend (action-color descriptive row).
//
// Lifetime:
//   - Tick polls for the PIE world, then ticks the ViewModel.
//   - PIE Begin/End delegate paths clear handle-bearing state through the
//     ViewModel + Sidebar so we don't leak FCk_Handle copies past the
//     registry death.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkGoapDebuggerWindow) {}
    SLATE_END_ARGS()

    ~SCkGoapDebuggerWindow();

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("GOAP")); }

    // -----------------------------------------------------------------------------------------------------------------
    // External entry point — used by the inspector gateway (D7) to invoke the
    // standalone tab AND drive the live window's ViewModel to select the given
    // entity. Safe to call before the tab exists: it spawns it via
    // FGlobalTabmanager::TryInvokeTab and then walks the active instance to
    // set selection. No-op if InEntity is invalid.
    // -----------------------------------------------------------------------------------------------------------------
    static auto OpenForEntity(const FCk_Handle& InEntity) -> void;

    // Used by the static OpenForEntity above to push selection into a live window.
    auto Set_SelectedEntityExternal(const FCk_Handle& InEntity) -> void;

    // Mission Control top-level views (underline tabs in the chrome).
    static const FName Tab_Squad;
    static const FName Tab_Inspector;
    static const FName Tab_Catalog;

    // Center-column tabs inside the Inspector view (mockup ".ctabs").
    static const FName CTab_Decision;
    static const FName CTab_Graph;
    static const FName CTab_Search;

    // Nerd mode — reveals search internals (nerd strip, Search-trace tab,
    // entity handles). Read by child panels via the window.
    auto Get_NerdMode() const -> bool { return _NerdMode; }

    // Test-only observability for the production-owned authored stable shell.
    auto Get_AuthoredShellView() const -> TSharedPtr<FCkUiView> { return _AuthoredShellView; }
    auto Get_AuthoredShellLoadFailure() const -> const FString& { return _AuthoredShellLoadFailure; }

protected:
    // Structural style axes changed — drop each panel's rebuild debounce and re-run its refresh.
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    friend class FCkGoapDebugger_WindowSelectionSyncPie;

    // -----------------------------------------------------------------------------------------------------------------
    // Build helpers — called once from Construct; result subtrees are cached.
    // -----------------------------------------------------------------------------------------------------------------

    auto BuildCommandGroups() -> TArray<FCkDebug_CommandGroup>;
    auto BuildMenuActions() -> TSharedRef<SWidget>;
    auto BuildNerdStrip() -> TSharedRef<SWidget>;
    auto BuildAlertStrip() -> TSharedRef<SWidget>;
    auto BuildSquadView()     -> TSharedRef<SWidget>;
    auto BuildCatalogView()   -> TSharedRef<SWidget>;
    auto BuildInspectorPorts() -> void;
    auto BuildNativeTopTabs() -> TSharedRef<SWidget>;
    auto BuildNativeInspectorView() -> TSharedRef<SWidget>;
    auto BuildNativeCenterColumn() -> TSharedRef<SWidget>;
    auto BuildAuthoredShell(const TSharedRef<SWidget>& InNerdStrip, const TSharedRef<SWidget>& InAlertStrip) -> void;
    auto PollAuthoredShell() -> void;
    auto ActivateNativeShellFallback(const TSharedRef<SWidget>& InNerdStrip, const TSharedRef<SWidget>& InAlertStrip) -> void;

    // Rebuild the chrome picker option lists from the snapshot batch and sync
    // their selected items to the ViewModel (echo-guarded via Direct).
    auto RefreshPickers() -> void;
    auto HandleAgentPicked(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto HandlePlannerPicked(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo) -> void;

    // PIE lifecycle — drop handle-bearing state.
    auto HandleWorldTornDown() -> void;
    auto HandleWorldChanged(UWorld* InWorld) -> void;
    auto HandleGlobalSelectionSync(const FCk_Handle& InSelected, FName InSource) -> void;
    auto Request_PauseExecution() -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel>   _ViewModel;
    TSharedPtr<SCkGoapDebugger_Sidebar>     _Sidebar;
    TSharedPtr<SCkGoapDebugger_AgentColumn> _AgentColumn;
    TSharedPtr<SCkGoapDebugger_WorldStateRail> _WorldStateRail;
    TSharedPtr<SCkGoapDebugger_GraphPane>   _GraphPane;
    TSharedPtr<SCkGoapDebugger_DecisionPanel>    _DecisionPanel;
    TSharedPtr<SCkGoapDebugger_SearchTracePanel> _SearchTracePanel;
    TSharedPtr<SCkGoapDebugger_TimelineDock>     _TimelineDock;
    TSharedPtr<SCkGoapDebugger_SquadTable>       _SquadTable;
    TSharedPtr<SCkGoapDebugger_CatalogPanel>     _CatalogPanel;
    TSharedPtr<FCkUiView> _AuthoredShellView;
    TSharedPtr<SBox> _AuthoredShellHost;
    FString _AuthoredShellLoadFailure;
    bool _AuthoredShellMounted = false;

    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TWeakObjectPtr<UWorld> _CachedWorld;

    // Shared viewport picker (CkDebuggerCommon), specialized to GOAP: only
    // roster entities (and their owner chain up to the NPC representative)
    // are previewed and pickable.
    TSharedPtr<FCkDebug_ViewportPicker> _ViewportPicker;

    // Chrome pickers — combo labels + parallel handle arrays (index-mapped).
    // Handles cleared on world teardown.
    TArray<TSharedPtr<FString>>     _AgentPickerLabels;
    TArray<FCk_Handle>              _AgentPickerHandles;
    TSharedPtr<STextComboBox>       _AgentPicker;
    TArray<TSharedPtr<FString>>     _PlannerPickerLabels;
    TArray<FCk_Handle_Goap_Planner> _PlannerPickerHandles;
    TSharedPtr<STextComboBox>       _PlannerPicker;

    // Mission Control chrome state.
    FName _ActiveTab;
    FName _CenterTab;
    bool  _NerdMode = false;
    bool  _PauseOnReplan = false;
    bool  _PauseOnPlanFailed = false;

    // Entity value only: unlike FCk_Handle this does not retain the PIE registry.
    TOptional<FCk_Entity> _PendingExternalEntity;

    // Common debugger-session boundary.
    FDelegateHandle _SessionInvalidatedHandle;
    FDelegateHandle _WorldChangedHandle;
    FDelegateHandle _SelectionSyncHandle;
};

// ====================================================================================================================
