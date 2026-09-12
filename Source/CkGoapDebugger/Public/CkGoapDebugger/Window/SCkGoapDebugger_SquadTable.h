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
// Rows expose authored EntityRef navigation plus Inspect. The native fallback keeps the same
// entity navigation and inspect behavior when the authored resource cannot be loaded.
// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class FCkUiCollection;
class FCkUiFloatSeries;
class FCkUiView;
class SBox;

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
    /**
     * Drop the rebuild debounce so the next refresh re-emits structure. The window calls this on a
     * style-revision bump: colours / fonts / paddings are attribute-bound and already live, but a
     * panel's STRUCTURE (which rows and slots exist at all) is composed once against the axes, so a
     * structural axis change needs the re-emit. Flipping the hash rather than zeroing it keeps a
     * genuine zero hash from swallowing the invalidation.
     */
    auto Invalidate_StyleCache() -> void;


    // Rows hold FCk_Handle copies — drop them while the registry is alive.
    auto Reset_ForWorldChange() -> void;

    /** Retained authored projection, when the resource and registry loaded successfully. */
    auto Get_AuthoredView() const -> TSharedPtr<FCkUiView> { return _AuthoredView; }
    auto Get_AuthoredCollection() const -> TSharedPtr<const FCkUiCollection> { return _AuthoredCollection; }
    /** Last authored activation rejection; retained when native fallback replaces the candidate. */
    auto Get_AuthoredLoadFailure() const -> const FString& { return _AuthoredLoadFailure; }

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

        // Highlight pass — false dims the row's name/chain text without hiding
        // the row. True whenever the highlight query is empty.
        bool IsHighlightMatch = true;
    };
    using ItemPtr = TSharedPtr<FSquadRow>;

    auto OnGenerateRow(ItemPtr InItem, const TSharedRef<STableViewBase>& InTable) -> TSharedRef<ITableRow>;
    auto Publish_AuthoredRows() -> void;
    auto TryActivate_AuthoredView() -> void;
    auto Activate_NativeFallback() -> void;
    auto On_AuthoredInspect(const FString& InKey) -> void;
    auto On_AuthoredEntityNavigate(const FString& InKey) -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
    FOnCkGoapDebug_SquadInspect _OnInspect;

    TSharedPtr<SListView<ItemPtr>> _ListView;
    TSharedPtr<class SCkDebug_DualSearchBar> _SearchBar;
    TSharedPtr<SBox> _ContentHost;
    TSharedPtr<SWidget> _NativeContent;

    // The authored projection uses planner-derived keys and this separate full-handle map. UI
    // collection fields remain presentation-only, so never reduce the inspector transport to an
    // entity number or display string.
    struct FAuthoredHandles { FCk_Handle Entity; FCk_Handle_Goap_Planner Planner; };
    TSharedPtr<FCkUiCollection> _AuthoredCollection;
    TSharedPtr<FCkUiView> _AuthoredView;
    FString _AuthoredLoadFailure;
    bool _AuthoredProjectionReady = false;
    TMap<FString, FAuthoredHandles> _AuthoredHandles;
    /** Planner-keyed owner of the series; collection records expose only weak handles. */
    TMap<FString, TSharedPtr<FCkUiFloatSeries>> _SparkSeriesByPlanner;

    // Stable row identity by planner handle (list-row contract).
    TMap<FCk_Handle_Goap_Planner, ItemPtr> _RowsByPlanner;
    TArray<ItemPtr> _Visible;

    // Dual search state. Both feed the rebuild hash so a keystroke re-runs
    // RefreshFromViewModel instead of being swallowed by the early-out.
    FString _FilterString;
    FString _HighlightString;

    uint32 _LastHash = 0;
};

// ====================================================================================================================
