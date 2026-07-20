#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// ====================================================================================================================
// Mission Control — "Squad" tab: one row per TOP-LEVEL Planner world-wide.
//
//   Agent (initials avatar + name) · planner tag · status pill (Plan Found /
//   Planning… / Disabled / Plan Failed / Cost Threshold) · active chain ·
//   plan cost · attempts · replans-60s sparkline · alert tags · Inspect ▸
//
// Sparkline samples derive client-side from the entity's history ring
// (replan-kind events bucketed into 5s bins) — no collector support needed.
//
// Rows are informational; the ONLY interactive element is the Inspect button
// (list SelectionMode = None, so no click-trap concerns). Inspect reports
// through OnInspect — the window sets ViewModel selection and flips to the
// Agent Inspector tab.
// ====================================================================================================================

class FCkGoapDebugger_ViewModel;

DECLARE_DELEGATE_TwoParams(FOnCkGoapDebug_SquadInspect, FCk_Handle /* Entity */, FCk_Handle_Goap_Planner /* Planner */);

class CKGOAPDEBUGGER_API SCkGoapDebugger_SquadTable : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_SquadTable) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
        SLATE_EVENT(FOnCkGoapDebug_SquadInspect, OnInspect)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Called by the window each gated Tick / on ViewModel change.
    auto RefreshFromViewModel() -> void;

    // Rows hold FCk_Handle copies — drop them while the registry is alive.
    auto Reset_ForWorldChange() -> void;

private:
    struct FSquadRow
    {
        FCk_Handle              EntityHandle;
        FCk_Handle_Goap_Planner PlannerHandle;

        FString Avatar;         // initials
        FString AgentName;
        FString PlannerLabel;   // tag leaf / display name
        ECk_GoapPlanStatus PlanStatus = ECk_GoapPlanStatus::Idle;
        bool    IsDisabled = false;
        FString ChainText;
        FString CostText;
        int32   Attempts = 0;
        bool    IsSelected = false;

        // fallback-active / no-fallback / threshold notes.
        TArray<FString> AlertTags;

        TSharedPtr<TArray<float>> SparkSamples;
    };
    using ItemPtr = TSharedPtr<FSquadRow>;

    auto OnGenerateRow(ItemPtr InItem, const TSharedRef<STableViewBase>& InTable) -> TSharedRef<ITableRow>;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
    FOnCkGoapDebug_SquadInspect _OnInspect;

    TSharedPtr<SListView<ItemPtr>> _ListView;

    // Stable row identity by planner handle (list-row contract).
    TMap<FCk_Handle_Goap_Planner, ItemPtr> _RowsByPlanner;
    TArray<ItemPtr> _Visible;

    uint32 _LastHash = 0;
};

// ====================================================================================================================
