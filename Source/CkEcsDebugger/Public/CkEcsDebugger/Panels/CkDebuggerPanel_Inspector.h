#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "Widgets/SCompoundWidget.h"

class SBox;
class SScrollBox;

namespace ck_debugger_panel_inspector
{
    auto Should_TickInspector(bool InCanInspect, bool InWantsTickWhenNotInspectable) -> bool;
}

enum class ECkInspectorDisplayMode : uint8
{
    GroupByInspector,
    GroupByEntity
};

class SCkDebuggerPanel_Inspector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerPanel_Inspector) {}
    SLATE_END_ARGS()

    ~SCkDebuggerPanel_Inspector();

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel) -> void;
    auto Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

    auto Get_CurrentInspectedEntity() const -> FCk_Handle;

private:
    auto DeactivateAllInspectors() -> void;
    auto RebuildInspectors() -> void;

    /**
     * The one entry point every rebuild TRIGGER goes through (selection change, panel filter,
     * display-mode flip, a deferred structural request). While an interactive row reports an active
     * edit the request is parked on the edit guard — deferred, never dropped — and Tick performs it
     * the moment the edit ends. Construct calls RebuildInspectors directly: there is nothing to eat.
     */
    auto Request_RebuildInspectors() -> void;
    auto Build_NoSelectionWidget() -> TSharedRef<SWidget>;
    auto Build_SingleEntityInspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    auto Build_InspectorSection(const FCk_Handle& Entity, const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector, int32 InspectorIndex) -> TSharedRef<SWidget>;

    auto Build_ModeToggle() -> TSharedRef<SWidget>;

    // ----- Multi-select DIFF mode --------------------------------------------
    // Inspector rows are authored against ONE handle, so cross-entity comparison cannot live inside
    // a row. It lives here: the panel replays each inspector's own Build path against the other
    // selected handles into a label -> value snapshot (FCkInspector_RowCaptureScope), diffs the
    // snapshots, and installs the resulting label set around the real build
    // (FCkInspector_DiffMarkScope). Recomputed once per gated rebuild, never per paint.
    //
    // Off by default and only reachable while more than one entity is selected, so the single-entity
    // inspector is untouched.
    auto Build_DiffModeControls() -> TSharedRef<SWidget>;
    auto Rebuild_DiffLabels(const TArray<FCk_Handle>& InEntities) -> void;

    auto Build_MultiEntityInspector_GroupByInspector(const TArray<FCk_Handle>& Entities) -> TSharedRef<SWidget>;
    auto Build_MultiEntityInspector_GroupByEntity(const TArray<FCk_Handle>& Entities) -> TSharedRef<SWidget>;
    auto Build_EntitySubSection(const FCk_Handle& Entity, const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector, int32 OuterIndex, int32 InnerIndex) -> TSharedRef<SWidget>;
    auto OnDisplayModeChanged(ECkInspectorDisplayMode NewMode) -> void;
    auto Format_EntityDisplayName(const FCk_Handle& Entity) const -> FText;

    auto RegisterDefaultInspectors() -> void;
    auto OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void;
    auto OnInspectorFilterChanged(int32 InspectorIndex, const FString& InFilterText) -> void;

    // Panel-level search (distinct from the per-inspector SCkDebuggerWidget_SearchBars,
    // which filter rows INSIDE an inspector). This pair filters whole inspector SECTIONS
    // by Get_ComponentName: _PanelFilterString hides non-matching sections outright,
    // _PanelHighlightString dims them via RenderOpacity instead of hiding them.
    auto Matches_PanelFilter(const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector) const -> bool;
    auto Get_PanelHighlightOpacity(const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector) const -> float;

    // Owner-chain breadcrumb (root › … › selected) pinned above the sections —
    // rebuilt only on selection change (stable-identity rule).
    auto RebuildBreadcrumb() -> void;

    TSharedPtr<SScrollBox> ScrollBox;
    TSharedPtr<SBox> _ModeToggleContainer;
    TSharedPtr<SBox> _BreadcrumbContainer;
    TSharedPtr<class SCkDebug_DualSearchBar> _PanelSearchBar;
    FString _PanelFilterString;
    FString _PanelHighlightString;
    TArray<TSharedPtr<ICkDebuggerComponentInspector_Base>> Inspectors;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;

    // Per-PANEL, never global: two ECS debugger windows must not block each other's rebuilds.
    TSharedPtr<class FCkInspectorEditGuard> _EditGuard;

    ECkInspectorDisplayMode _DisplayMode = ECkInspectorDisplayMode::GroupByInspector;
    TMap<int32, FString> InspectorFilters;
    TMap<TPair<int32, int32>, TSharedPtr<SBox>> _InspectorContentContainers;
    TArray<FCk_Handle> _CurrentInspectedEntities;

    // Diff-mode state. The label sets hold STRINGS only — nothing here retains a PIE handle, so the
    // session-invalidation boundary has nothing extra to clear.
    bool _DiffMode = false;
    TMap<int32, TSet<FString>> _DiffLabelsByInspector;
    int32 _DiffSkippedCount = 0;
};
