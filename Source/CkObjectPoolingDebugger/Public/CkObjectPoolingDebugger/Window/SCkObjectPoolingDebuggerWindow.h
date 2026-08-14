#pragma once

#include "CkObjectPoolingDebugger/Data/CkObjectPoolingDebugger_Snapshot.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CommandBar.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

class STextBlock;
class SBox;
class ITableRow;
class STableViewBase;
class FCkDebuggerModel_WorldSelector;
class SCkDebug_StatPair;
class SCkDebug_StatusPill;
class SCkDebug_Sparkline;
class SCkDebug_DualSearchBar;

// ====================================================================================================================
// CK Object Pooling Debugger window.
//
// Layout: toolbar (world selector · subsystem pill · filter/highlight · refresh) → overview strip
// (stat tiles + all-pools in-use sparkline) → sortable pool table (policy pill, occupancy bar,
// per-pool trend sparkline, counters) → right inspector rail for the selected pool (occupancy
// graph with high-water marker, churn rates, pool params, lifecycle counters).
//
// Rendering policy (SCkDebugger_WindowBase): the tree is built once; stats flow through TAttribute
// bindings reading the shared row structs, which are mutated in place on the gated tick. Row
// objects are rebuilt only when the pool SET changes (keys); the inspector graph is rebuilt only on
// selection change. Per-pool history rings live window-side and are sampled on the gated tick.
// ====================================================================================================================

class SCkObjectPoolingDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    using ItemPtr = TSharedPtr<FCkObjectPoolingDebugger_PoolRow>;

    SLATE_BEGIN_ARGS(SCkObjectPoolingDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("Object Pooling")); }

private:
    // sortable table columns
    enum class ESortColumn : uint8
    {
        Class,
        Free,
        InUse,
        Live,
        High,
        HitRate,
        Miss,
        Prewarm,
    };

    // per-pool sample rings + rate tracking, keyed by Get_PoolKeyString()
    struct FPoolHistory
    {
        TSharedPtr<TArray<float>> InUseSamples;
        TSharedPtr<TArray<float>> LiveSamples;

        int32  LastTotalAcquires = 0;
        int32  LastNumInUse = 0;
        double LastSampleTime = 0.0;
        float  AcquiresPerSec = 0.0f;
        float  ReleasesPerSec = 0.0f;
    };

    auto BuildCommandGroups() -> TArray<FCkDebug_CommandGroup>;
    auto BuildOverviewStrip() -> TSharedRef<SWidget>;
    auto BuildHeaderRow() -> TSharedRef<SWidget>;
    auto BuildInspectorRail() -> TSharedRef<SWidget>;
    auto BuildStatusBar() -> TSharedRef<SWidget>;

    auto OnGenerateRow(ItemPtr InItem, const TSharedRef<STableViewBase>& InTable) -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(ItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto OnSortColumnClicked(ESortColumn InColumn) -> FReply;

    // gather a fresh snapshot, write its JSON report to Saved/CkReports/, toast the resulting path
    auto DoExport_JsonReport() -> FReply;

    auto MakeSortableHeader(const FString& InLabel, ESortColumn InColumn, float InFillWidth, bool InRightAlign, const FString& InTooltip = {}) -> TSharedRef<SWidget>;

    auto DoSample_Histories(const FCkObjectPoolingDebugger_Snapshot& InSnapshot, double InCurrentTime) -> void;
    auto DoRefresh_VisibleItems() -> void;
    auto DoRefresh_OverviewStats(const FCkObjectPoolingDebugger_Snapshot& InSnapshot) -> void;
    auto DoRebuild_InspectorGraph() -> void;

    auto Get_HistoryFor(const FString& InPoolKey) -> FPoolHistory&;
    auto Get_SelectedHistory() const -> const FPoolHistory*;

    static auto MakeSignature(const FCkObjectPoolingDebugger_Snapshot& InSnapshot) -> FString;

    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;

    // toolbar
    TSharedPtr<SCkDebug_StatusPill>  _PillSubsystemLive;
    TSharedPtr<SCkDebug_StatusPill>  _PillNoSubsystem;
    TSharedPtr<STextBlock>           _PinnedUniqueText;
    TSharedPtr<SCkDebug_DualSearchBar> _SearchBar;

    // overview strip
    TSharedPtr<SCkDebug_StatPair> _StatPools;
    TSharedPtr<SCkDebug_StatPair> _StatLive;
    TSharedPtr<SCkDebug_StatPair> _StatInUse;
    TSharedPtr<SCkDebug_StatPair> _StatParked;
    TSharedPtr<SCkDebug_StatPair> _StatMisses;
    TSharedPtr<SCkDebug_StatPair> _StatHitRate;
    TSharedPtr<STextBlock>        _HeroNowText;
    TSharedPtr<TArray<float>>     _TotalInUseSamples;

    // table
    TSharedPtr<SListView<ItemPtr>> _ListView;
    TArray<ItemPtr> _Items;        // all pools, snapshot order
    TArray<ItemPtr> _VisibleItems; // filter + sort applied — the list source
    TSharedPtr<float> _SharedMaxLive; // occupancy-bar scale shared across rows

    // inspector rail
    TSharedPtr<STextBlock> _SelectedNameText;
    TSharedPtr<STextBlock> _SelectedSubText;
    TSharedPtr<SBox>       _InspectorGraphSlot;
    TSharedPtr<SCkDebug_StatPair> _RateAcquires;
    TSharedPtr<SCkDebug_StatPair> _RateReleases;
    TSharedPtr<SCkDebug_StatPair> _RateHitRate;

    // status bar
    TSharedPtr<STextBlock> _GatherMsText;
    TSharedPtr<STextBlock> _SortDescText;

    TMap<FString, FPoolHistory> _Histories;

    ItemPtr _SelectedItem;
    FString _SelectedKey;

    FString _FilterText;
    FString _HighlightText;

    ESortColumn _SortColumn = ESortColumn::InUse;
    bool        _SortAscending = false;
    bool        _ShowInUseOnly = false;

    FString _LastSignature;
    int32   _NumPinnedUnique = 0;
    bool    _HasSubsystem = false;
    double  _LastGatherMs = 0.0;
};

// ====================================================================================================================
