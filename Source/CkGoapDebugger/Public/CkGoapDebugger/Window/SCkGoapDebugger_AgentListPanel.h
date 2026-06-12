#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FCkGoapDebugger_ViewModel;

// ====================================================================================================================
// SCkGoapDebugger_AgentListPanel — list of every GOAP-bearing entity in the
// world, replacing the old toolbar combo-box picker. Mirrors the Crowd / ECS
// debugger list pattern:
//   - SListView with stable TSharedPtr row identity (reuse by entity handle).
//   - SCkDebug_DualSearchBar: Filter narrows rows, Highlight dims non-matches.
//   - Rows: SCkDebug_EntityRef pill + debug name + planner count.
//   - Selection writes ViewModel::SetSelectedEntity and broadcasts on the
//     cross-debugger selection-sync bus; receives sync broadcasts and selects
//     the snapshot whose lineage matches.
//
// The window calls RefreshFromViewModel() each gated Tick and
// Reset_ForWorldChange() on PIE teardown (rows hold FCk_Handle copies).
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_AgentListPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_AgentListPanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkGoapDebugger_AgentListPanel() override;

    // Identity-stable refresh off the ViewModel's snapshot batch. Cheap when
    // the entity set is unchanged.
    auto RefreshFromViewModel() -> void;

    // Drop all handle-bearing rows — must run while the registry is alive.
    auto Reset_ForWorldChange() -> void;

    // Mirror the ViewModel's selected entity into the list highlight without
    // firing a selection echo. Call ONLY from discrete external-selection
    // events (inspector gateway, set rebuild) — NEVER per tick, or in-flight
    // clicks get stamped over before mouse-up commits them.
    auto RestoreSelectionFromViewModel() -> void;

private:
    struct FAgentRow
    {
        FCk_Handle Handle;
        FString    Name;
        int32      PlannerCount = 0;
        bool       IsHighlightMatch = true;
    };
    using ItemPtr = TSharedPtr<FAgentRow>;

    auto OnGenerateRow(ItemPtr InItem, const TSharedRef<STableViewBase>& InTable) -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(ItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;
    auto OnGlobalSelectionSync(const FCk_Handle& InSelected, FName InSource) -> void;

    // Rebuild _Visible from _AllItems using the filter string, re-stamp
    // highlight flags, refresh the list, and restore the ViewModel selection.
    auto ApplySearchAndRefresh() -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    TArray<ItemPtr> _AllItems;   // one per entity snapshot, stable by handle
    TArray<ItemPtr> _Visible;    // filter-surviving subset (list source)

    TSharedPtr<SListView<ItemPtr>> _ListView;

    FString _FilterString;
    FString _HighlightString;

    FDelegateHandle _OnSelectionSyncHandle;
};

// ====================================================================================================================
