#include "CkGoapDebugger/Window/SCkGoapDebugger_SquadTable.h"

#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/CkUiFloatSeries.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

// ====================================================================================================================

namespace ck_goap_debugger_squad_table
{
    constexpr auto Spark_WindowSeconds = 60.0;
    constexpr auto Spark_BinSeconds    = 5.0;

    // Identity debug names arrive dot-separated ("Npc.CrankHank"), so whitespace
    // parsing alone yielded "NP" for every agent in a town. Reduce to the last
    // '.'-segment first, then keep the original whitespace / two-letter fallback:
    //   "Npc.CrankHank" → "CR",  "Npc.Tourist" → "TO",  "Crank Hank" → "CH".
    auto Compute_Initials(const FString& InName) -> FString
    {
        auto Leaf = InName;
        auto DotIndex = int32{INDEX_NONE};
        if (Leaf.FindLastChar(TEXT('.'), DotIndex) && DotIndex < Leaf.Len() - 1)
        { Leaf = Leaf.RightChop(DotIndex + 1); }

        auto Parts = TArray<FString>{};
        Leaf.ParseIntoArrayWS(Parts);
        if (Parts.Num() >= 2) { return (Parts[0].Left(1) + Parts.Last().Left(1)).ToUpper(); }
        return Leaf.Left(2).ToUpper();
    }

    auto Get_StatusText(ECk_GoapPlanStatus InStatus, bool InDisabled) -> FString
    {
        if (InDisabled) { return TEXT("Disabled"); }
        switch (InStatus)
        {
            case ECk_GoapPlanStatus::Planning:             return TEXT("Planning\x2026");
            case ECk_GoapPlanStatus::PlanFound:            return TEXT("Plan Found");
            case ECk_GoapPlanStatus::PlanFailed:           return TEXT("Plan Failed");
            case ECk_GoapPlanStatus::CostThresholdReached: return TEXT("Cost Threshold");
            default:                                       return TEXT("Idle");
        }
    }

    auto Get_StatusTone(ECk_GoapPlanStatus InStatus, bool InDisabled) -> ECk_Tone
    {
        if (InDisabled) { return ECk_Tone::Neutral; }
        switch (InStatus)
        {
            case ECk_GoapPlanStatus::Planning:             return ECk_Tone::Info;
            case ECk_GoapPlanStatus::PlanFound:            return ECk_Tone::Ok;
            case ECk_GoapPlanStatus::PlanFailed:           return ECk_Tone::Err;
            case ECk_GoapPlanStatus::CostThresholdReached: return ECk_Tone::Warn;
            default:                                       return ECk_Tone::Neutral;
        }
    }

    // Active-chain display: the Plan[0] descent, already flattened by the
    // roster pass (FCkGoapDebugger_RosterPlannerRow::ChainStepClassNames — the
    // fragment-level twin of the PlannerInfo walk this used to do here).
    // Names run through the shared name-depth tuner.
    auto Compute_ChainText(const FCkGoapDebugger_RosterPlannerRow& InPlanner, int32 InNameDepth) -> FString
    {
        auto Names = TArray<FString>{};
        Names.Reserve(InPlanner.ChainStepClassNames.Num());

        for (const auto& StepName : InPlanner.ChainStepClassNames)
        {
            Names.Add(SCkDebug_NameLabel::Get_ShortName(StepName, InNameDepth));
        }
        return Names.Num() > 0 ? FString::Join(Names, TEXT(" › ")) : FString(TEXT("\x2014"));
    }

    // Case-insensitive substring match across every column the user can see.
    // Empty needle matches everything (so an empty Filter shows the full squad
    // and an empty Highlight dims nothing).
    auto Matches_Row(
        const FString& InNeedle,
        const FString& InAgentName,
        const FString& InPlannerLabel,
        const FString& InChainText,
        const FString& InStatusText,
        const FString& InEntityIdText) -> bool
    {
        if (InNeedle.IsEmpty()) { return true; }

        return InAgentName.Contains(InNeedle, ESearchCase::IgnoreCase)
            || InPlannerLabel.Contains(InNeedle, ESearchCase::IgnoreCase)
            || InChainText.Contains(InNeedle, ESearchCase::IgnoreCase)
            || InStatusText.Contains(InNeedle, ESearchCase::IgnoreCase)
            || InEntityIdText.Contains(InNeedle, ESearchCase::IgnoreCase);
    }

    auto TextField(const FString& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }

    auto BoolField(const bool InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Bool, .Bool = InValue}; }

    auto ColorField(const FLinearColor& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::Color, .Color = InValue}; }

    auto FloatSeriesField(const TSharedPtr<FCkUiFloatSeries>& InValue) -> FCkUiFieldValue
    { return {.Kind = ECkUiFieldKind::FloatSeries, .FloatSeries = InValue}; }

    auto StyleTokens() -> FCkUiView::FTokens
    {
        return {
            {TEXT("--space-xs"), FString::SanitizeFloat(CkStyle::SpaceXS)},
            {TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS)},
            {TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM)},
            {TEXT("--space-l"), FString::SanitizeFloat(CkStyle::SpaceL)},
            {TEXT("--goap-squad-row-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeSmall()))},
            {TEXT("--goap-squad-micro-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeMicro()))},
        };
    }

    auto PlannerKey(const FCk_Handle_Goap_Planner& InPlanner) -> FString
    {
        const FCk_Entity& Entity = InPlanner.Get_Entity();
        return FString::Printf(TEXT("goap-squad:%d:%d"), static_cast<int32>(Entity.Get_EntityNumber()),
            static_cast<int32>(Entity.Get_VersionNumber()));
    }

    auto AuthoredSchema() -> FCkUiCollectionSchema
    {
        return {
            {
                {TEXT("squad-avatar"), ECkUiFieldKind::Text},
                {TEXT("squad-agent-name"), ECkUiFieldKind::Text},
                {TEXT("squad-agent-color"), ECkUiFieldKind::Color},
                {TEXT("squad-entity-id"), ECkUiFieldKind::Text},
                {TEXT("squad-planner"), ECkUiFieldKind::Text},
                {TEXT("squad-status"), ECkUiFieldKind::Text},
                {TEXT("squad-status-foreground"), ECkUiFieldKind::Color},
                {TEXT("squad-status-background"), ECkUiFieldKind::Color},
                {TEXT("squad-chain"), ECkUiFieldKind::Text},
                {TEXT("squad-chain-color"), ECkUiFieldKind::Color},
                {TEXT("squad-cost"), ECkUiFieldKind::Text},
                {TEXT("squad-attempts"), ECkUiFieldKind::Text},
                {TEXT("squad-replan-samples"), ECkUiFieldKind::FloatSeries},
                {TEXT("squad-alert-fallback-visible"), ECkUiFieldKind::Bool},
                {TEXT("squad-alert-fallback"), ECkUiFieldKind::Text},
                {TEXT("squad-alert-no-fallback-visible"), ECkUiFieldKind::Bool},
                {TEXT("squad-alert-no-fallback"), ECkUiFieldKind::Text},
                {TEXT("squad-alert-threshold-visible"), ECkUiFieldKind::Bool},
                {TEXT("squad-alert-threshold"), ECkUiFieldKind::Text},
                {TEXT("squad-alert-warning-foreground"), ECkUiFieldKind::Color},
                {TEXT("squad-alert-warning-background"), ECkUiFieldKind::Color},
                {TEXT("squad-alert-error-foreground"), ECkUiFieldKind::Color},
                {TEXT("squad-alert-error-background"), ECkUiFieldKind::Color},
            }
        };
    }
}

// ====================================================================================================================
// CONSTRUCT / LIFECYCLE
// ====================================================================================================================

auto
    SCkGoapDebugger_SquadTable::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;
    _OnInspect = InArgs._OnInspect;

    // This collection is keyed by the full planner entity identity. Its records are retained by
    // FCkUiCollection, while _AuthoredHandles transports the full typed handles for Inspect.
    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreateHierarchical(
        ck_goap_debugger_squad_table::AuthoredSchema(), _AuthoredCollection);
    _AuthoredProjectionReady = CollectionResult.Succeeded;
    if (NOT _AuthoredProjectionReady) { _AuthoredLoadFailure = FString::Join(CollectionResult.Errors, TEXT("\n")); }

    ChildSlot
    [
        SAssignNew(_ContentHost, SBox)
        [
        SAssignNew(_NativeContent, SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
            .Padding(FMargin(CkStyle::SpaceL))
            [
                SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
                        [
                            SAssignNew(_SearchBar, SCkDebug_DualSearchBar)
                                .FilterHintText(FText::FromString(TEXT("Filter squad\x2026")))
                                .HighlightHintText(FText::FromString(TEXT("Highlight\x2026")))
                                .OnFilterTextChanged_Lambda([this](const FString& InText)
                                {
                                    if (_FilterString == InText) { return; }
                                    _FilterString = InText;
                                    RefreshFromViewModel();
                                })
                                .OnHighlightTextChanged_Lambda([this](const FString& InText)
                                {
                                    if (_HighlightString == InText) { return; }
                                    _HighlightString = InText;
                                    RefreshFromViewModel();
                                })
                        ]

                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SAssignNew(_ListView, SListView<ItemPtr>)
                                .ListItemsSource(&_Visible)
                                .SelectionMode(ESelectionMode::None)
                                .OnGenerateRow(this, &SCkGoapDebugger_SquadTable::OnGenerateRow)
                        ]
            ]
        ]
        ];

    RefreshFromViewModel();
    TryActivate_AuthoredView();
}

auto
    SCkGoapDebugger_SquadTable::
    Reset_ForWorldChange()
    -> void
{
    _RowsByPlanner.Reset();
    _Visible.Reset();
    _AuthoredHandles.Reset();
    _SparkSeriesByPlanner.Reset();
    if (_AuthoredCollection.IsValid()) { _AuthoredCollection->TrySetRecords({}); }
    _LastHash = 0;

    if (_ListView.IsValid())
    { _ListView->RequestListRefresh(); }
}

auto
    SCkGoapDebugger_SquadTable::
    Invalidate_StyleCache()
    -> void
{
    _LastHash = ~_LastHash;
    RefreshFromViewModel();
    if (_AuthoredView.IsValid()) { _AuthoredView->PollFiles(ck_goap_debugger_squad_table::StyleTokens()); }
}

// ====================================================================================================================
// REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_SquadTable::
    RefreshFromViewModel()
    -> void
{
    using namespace ck_goap_debugger_squad_table;

    if (NOT _ViewModel.IsValid() || NOT _ListView.IsValid()) { return; }

    const auto& Roster = _ViewModel->Get_Roster();

    // Hash — planner set + per-planner surface state.
    //
    // NOTE: deliberately does NOT combine any frame counter. The old
    // Snapshot.FrameNumber combine changed this hash every tick, so the
    // early-out below never fired and the whole squad list rebuilt on every
    // refresh regardless of whether anything visible had changed.
    auto NewHash = uint32{0};
    for (const auto& Entry : Roster)
    {
        // Aging a 5-second bucket changes the visual projection at most once per bin, never once
        // per frame. Event timing also admits a newly recorded replan before the next bucket edge.
        NewHash = HashCombine(NewHash, ::GetTypeHash(FMath::FloorToInt64(Entry.WorldTimeSeconds / Spark_BinSeconds)));
        for (const auto& Event : FCkGoapDebugger_DataCollector::GetHistory(Entry.EntityHandle))
        {
            const bool IsReplanKind = Event.Kind == ECkGoapDebugger_HistoryEventKind::Replanned
                || Event.Kind == ECkGoapDebugger_HistoryEventKind::PlanFound
                || Event.Kind == ECkGoapDebugger_HistoryEventKind::PlanFailed;
            if (IsReplanKind)
            {
                NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<uint8>(Event.Kind)));
                NewHash = HashCombine(NewHash, ::GetTypeHash(Event.WorldTimeSeconds));
            }
        }
        for (const auto& Planner : Entry.Planners)
        {
            auto RowHash = GetTypeHash(Planner.PlannerHandle);
            RowHash = HashCombine(RowHash, GetTypeHash(Entry.EntityHandle));
            RowHash = HashCombine(RowHash, GetTypeHash(Entry.DebugName));
            RowHash = HashCombine(RowHash, GetTypeHash(Planner.DisplayName));
            RowHash = HashCombine(RowHash, ::GetTypeHash(static_cast<uint8>(Planner.PlanStatus)));
            RowHash = HashCombine(RowHash, ::GetTypeHash(static_cast<uint8>(Planner.EnableToggle)));
            RowHash = HashCombine(RowHash, ::GetTypeHash(Planner.PlanCost));
            RowHash = HashCombine(RowHash, ::GetTypeHash(Planner.PlanAttemptCount));
            RowHash = HashCombine(RowHash, ::GetTypeHash(Planner.ChainLeafIsFallback));
            RowHash = HashCombine(RowHash, ::GetTypeHash(Planner.HasUnconditionalFallback));
            RowHash = HashCombine(RowHash, ::GetTypeHash(Planner.AllowPlanFailed));
            for (const auto& StepName : Planner.ChainStepClassNames)
            { RowHash = HashCombine(RowHash, GetTypeHash(StepName)); }
            NewHash ^= RowHash;
        }
    }
    NewHash = HashCombine(NewHash, GetTypeHash(_ViewModel->GetSelectedActionSet()));
    // Chain names run through the shared name-depth tuner.
    NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth()));
    // Search state participates in the hash so a keystroke re-runs the rebuild
    // instead of being swallowed by the early-out below. GetTypeHash(FString) is
    // a hidden friend — reachable only by ADL, so it must stay unqualified.
    NewHash = HashCombine(NewHash, GetTypeHash(_FilterString));
    NewHash = HashCombine(NewHash, GetTypeHash(_HighlightString));

    if (NewHash == _LastHash) { return; }
    _LastHash = NewHash;

    // ---- Rebuild rows (stable identity by planner handle) ----------------------
    auto NewVisible = TArray<ItemPtr>{};
    auto SeenPlanners = TSet<FCk_Handle_Goap_Planner>{};
    auto SeenPlannerKeys = TSet<FString>{};

    for (const auto& Entry : Roster)
    {
        const auto& History = FCkGoapDebugger_DataCollector::GetHistory(Entry.EntityHandle);
        const auto NowSeconds = Entry.WorldTimeSeconds;

        for (const auto& Planner : Entry.Planners)
        {
            SeenPlanners.Add(Planner.PlannerHandle);
            const FString PlannerStableKey = PlannerKey(Planner.PlannerHandle);
            SeenPlannerKeys.Add(PlannerStableKey);

            auto& Row = _RowsByPlanner.FindOrAdd(Planner.PlannerHandle);
            if (NOT Row.IsValid())
            {
                Row = MakeShared<FSquadRow>();
                Row->SparkSamples = MakeShared<TArray<float>>();
            }
            auto& SparkSeries = _SparkSeriesByPlanner.FindOrAdd(PlannerStableKey);
            if (NOT SparkSeries.IsValid())
            {
                auto InitialSamples = TArray<float>{};
                InitialSamples.Init(0.0f, static_cast<int32>(Spark_WindowSeconds / Spark_BinSeconds));
                const FCkUiLoadResult CreateSeriesResult = FCkUiFloatSeries::TryCreate(MoveTemp(InitialSamples), SparkSeries);
                if (NOT CreateSeriesResult.Succeeded || NOT SparkSeries.IsValid())
                {
                    Activate_NativeFallback();
                    return;
                }
            }

            Row->EntityHandle  = Entry.EntityHandle;
            Row->PlannerHandle = Planner.PlannerHandle;
            Row->AgentName     = Entry.DebugName;
            Row->Avatar        = Compute_Initials(Entry.DebugName);
            Row->PlannerLabel  = Planner.DisplayName;
            Row->PlanStatus    = Planner.PlanStatus;
            Row->IsDisabled    = Planner.EnableToggle == ECk_EnableDisable::Disable;
            Row->Attempts      = Planner.PlanAttemptCount;
            Row->IsSelected    = static_cast<FCk_Handle>(_ViewModel->GetSelectedActionSet())
                              == static_cast<FCk_Handle>(Planner.PlannerHandle);

            if (Row->IsDisabled)
            { Row->ChainText = TEXT("planner disabled — skips planning & activation"); }
            else if (Planner.PlanStatus == ECk_GoapPlanStatus::Planning)
            { Row->ChainText = TEXT("searching\x2026"); }
            else
            { Row->ChainText = Compute_ChainText(Planner, _ViewModel->Get_NameDepth()); }

            Row->CostText = Planner.PlanStatus == ECk_GoapPlanStatus::PlanFound
                ? FString::Printf(TEXT("%.1f"), Planner.PlanCost)
                : FString(TEXT("\x2014"));

            // Alerts. The three inputs are pre-derived by the roster pass —
            // the direct-children cost scan this used to do inline now runs
            // once per planner in the collector.
            Row->AlertTags.Reset();
            {
                if (Planner.ChainLeafIsFallback)
                { Row->AlertTags.Add(TEXT("fallback")); }

                if (NOT Planner.HasUnconditionalFallback && NOT Planner.AllowPlanFailed)
                { Row->AlertTags.Add(TEXT("no fallback")); }

                if (Planner.PlanStatus == ECk_GoapPlanStatus::CostThresholdReached)
                { Row->AlertTags.Add(TEXT("threshold")); }
            }

            // ---- Search pipeline ---------------------------------------------
            // Filter hides, Highlight dims. Both run over the same visible-column
            // set, including the entity reference exactly as the row's EntityRef pill renders it.
            //
            // Composed through the axes lib rather than hand-formatted: the pill is a ShowName(false)
            // site, so it feeds an empty name and every EntityIdStyle option degrades to the bare id
            // — the same text this search sees, whichever option is selected. Hand-rolling the id
            // string here is what let the two drift apart in the first place.
            //
            // NOTE the planner stays in SeenPlanners above even when filtered out,
            // so its cached row TSharedPtr survives and comes back with the same
            // identity when the query is cleared.
            {
                const auto StatusText = Get_StatusText(Row->PlanStatus, Row->IsDisabled);
                const auto EntityIdText = ck::debug_axes::Make_EntityIdText(
                    UCkDebuggerStyleSettings::Get_Selection(),
                    FString{},
                    ck::Format_UE(TEXT("{}"), Row->EntityHandle.Get_Entity())).ToString();

                if (NOT Matches_Row(_FilterString, Row->AgentName, Row->PlannerLabel,
                        Row->ChainText, StatusText, EntityIdText))
                { continue; }

                Row->IsHighlightMatch = Matches_Row(_HighlightString, Row->AgentName,
                    Row->PlannerLabel, Row->ChainText, StatusText, EntityIdText);
            }

            // Replans-per-5s sparkline over the last 60s of this planner's
            // history (replan-kind events only).
            {
                const auto BinCount = static_cast<int32>(Spark_WindowSeconds / Spark_BinSeconds);
                Row->SparkSamples->Init(0.0f, BinCount);

                for (const auto& Event : History)
                {
                    const auto IsReplanKind =
                        Event.Kind == ECkGoapDebugger_HistoryEventKind::Replanned ||
                        Event.Kind == ECkGoapDebugger_HistoryEventKind::PlanFound ||
                        Event.Kind == ECkGoapDebugger_HistoryEventKind::PlanFailed;
                    if (NOT IsReplanKind) { continue; }

                    if (static_cast<FCk_Handle>(Event.ActionSetHandle)
                        != static_cast<FCk_Handle>(Planner.PlannerHandle)) { continue; }

                    const auto Age = NowSeconds - Event.WorldTimeSeconds;
                    if (Age < 0.0 || Age >= Spark_WindowSeconds) { continue; }

                    const auto Bin = BinCount - 1 - static_cast<int32>(Age / Spark_BinSeconds);
                    if (Row->SparkSamples->IsValidIndex(Bin))
                    { (*Row->SparkSamples)[Bin] += 1.0f; }
                }

                if (SparkSeries.IsValid()) { SparkSeries->TrySetSamples(*Row->SparkSamples); }
            }

            NewVisible.Add(Row);
        }
    }

    // Drop rows whose planner vanished.
    for (auto It = _RowsByPlanner.CreateIterator(); It; ++It)
    {
        if (NOT SeenPlanners.Contains(It->Key)) { It.RemoveCurrent(); }
    }
    for (auto It = _SparkSeriesByPlanner.CreateIterator(); It; ++It)
    {
        if (NOT SeenPlannerKeys.Contains(It->Key)) { It.RemoveCurrent(); }
    }

    _Visible = MoveTemp(NewVisible);
    _ListView->RequestListRefresh();
    Publish_AuthoredRows();
}

auto
    SCkGoapDebugger_SquadTable::
    Publish_AuthoredRows()
    -> void
{
    using namespace ck_goap_debugger_squad_table;

    if (NOT _AuthoredCollection.IsValid()) { return; }

    auto Records = TArray<FCkUiRecordData>{};
    auto NextHandles = TMap<FString, FAuthoredHandles>{};
    Records.Reserve(_Visible.Num());
    NextHandles.Reserve(_Visible.Num());
    for (const ItemPtr& Row : _Visible)
    {
        if (NOT Row.IsValid()) { continue; }

        FCkUiRecordData Record;
        Record.Key = PlannerKey(Row->PlannerHandle);
        const TSharedPtr<FCkUiFloatSeries> SparkSeries = _SparkSeriesByPlanner.FindRef(Record.Key);
        if (NOT SparkSeries.IsValid())
        {
            Activate_NativeFallback();
            return;
        }
        const ECk_Tone StatusTone = Get_StatusTone(Row->PlanStatus, Row->IsDisabled);
        const bool HasFallbackAlert = Row->AlertTags.Contains(TEXT("fallback"));
        const bool HasNoFallbackAlert = Row->AlertTags.Contains(TEXT("no fallback"));
        const bool HasThresholdAlert = Row->AlertTags.Contains(TEXT("threshold"));
        Record.Fields.Add(TEXT("squad-avatar"), TextField(Row->Avatar));
        Record.Fields.Add(TEXT("squad-agent-name"), TextField(Row->AgentName));
        Record.Fields.Add(TEXT("squad-agent-color"), ColorField(Row->IsHighlightMatch ? CkStyle::Text() : CkStyle::TextMute()));
        Record.Fields.Add(TEXT("squad-entity-id"), TextField(ck::Format_UE(TEXT("{}"), Row->EntityHandle.Get_Entity())));
        Record.Fields.Add(TEXT("squad-planner"), TextField(Row->PlannerLabel));
        Record.Fields.Add(TEXT("squad-status"), TextField(Get_StatusText(Row->PlanStatus, Row->IsDisabled)));
        Record.Fields.Add(TEXT("squad-status-foreground"), ColorField(CkStyle::GetToneColor(StatusTone)));
        Record.Fields.Add(TEXT("squad-status-background"), ColorField(CkStyle::GetToneDimColor(StatusTone)));
        Record.Fields.Add(TEXT("squad-chain"), TextField(Row->ChainText));
        Record.Fields.Add(TEXT("squad-chain-color"), ColorField(Row->IsHighlightMatch ? CkStyle::TextDim() : CkStyle::TextMute()));
        Record.Fields.Add(TEXT("squad-cost"), TextField(Row->CostText));
        Record.Fields.Add(TEXT("squad-attempts"), TextField(FString::FromInt(Row->Attempts)));
        Record.Fields.Add(TEXT("squad-replan-samples"), FloatSeriesField(SparkSeries));
        Record.Fields.Add(TEXT("squad-alert-fallback-visible"), BoolField(HasFallbackAlert));
        Record.Fields.Add(TEXT("squad-alert-fallback"), TextField(TEXT("fallback")));
        Record.Fields.Add(TEXT("squad-alert-no-fallback-visible"), BoolField(HasNoFallbackAlert));
        Record.Fields.Add(TEXT("squad-alert-no-fallback"), TextField(TEXT("no fallback")));
        Record.Fields.Add(TEXT("squad-alert-threshold-visible"), BoolField(HasThresholdAlert));
        Record.Fields.Add(TEXT("squad-alert-threshold"), TextField(TEXT("threshold")));
        Record.Fields.Add(TEXT("squad-alert-warning-foreground"), ColorField(CkStyle::GetToneColor(ECk_Tone::Warn)));
        Record.Fields.Add(TEXT("squad-alert-warning-background"), ColorField(CkStyle::GetToneDimColor(ECk_Tone::Warn)));
        Record.Fields.Add(TEXT("squad-alert-error-foreground"), ColorField(CkStyle::GetToneColor(ECk_Tone::Err)));
        Record.Fields.Add(TEXT("squad-alert-error-background"), ColorField(CkStyle::GetToneDimColor(ECk_Tone::Err)));

        Records.Add(MoveTemp(Record));
        NextHandles.Add(PlannerKey(Row->PlannerHandle), {Row->EntityHandle, Row->PlannerHandle});
    }

    // Publish handles only after collection admission, preventing a rejected projection from
    // dispatching Inspect against data that was never rendered.
    const FCkUiLoadResult PublishResult = _AuthoredCollection->TrySetRecords(MoveTemp(Records));
    const bool Published = PublishResult.Succeeded;
    CK_ENSURE_IF_NOT(Published, TEXT("GOAP Squad authored collection publication failed: {}"),
        FString::Join(PublishResult.Errors, TEXT("\n")))
    {}
    if (NOT Published)
    {
        _AuthoredHandles.Reset();
        Activate_NativeFallback();
        return;
    }
    _AuthoredHandles = MoveTemp(NextHandles);
}

auto
    SCkGoapDebugger_SquadTable::
    On_AuthoredInspect(const FString& InKey)
    -> void
{
    const FAuthoredHandles* Handles = _AuthoredHandles.Find(InKey);
    if (Handles == nullptr || NOT ck::IsValid(Handles->Entity) || NOT ck::IsValid(Handles->Planner)) { return; }
    _OnInspect.ExecuteIfBound(Handles->Entity, Handles->Planner);
}

auto
    SCkGoapDebugger_SquadTable::
    TryActivate_AuthoredView()
    -> void
{
    if (_AuthoredView.IsValid() || NOT _ContentHost.IsValid() || NOT _AuthoredProjectionReady || NOT _AuthoredCollection.IsValid()) { return; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(RegistryResult.Errors, TEXT("\n"));
        Activate_NativeFallback();
        return;
    }
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid())
    {
        _AuthoredLoadFailure = TEXT("CkDebugger plugin is unavailable.");
        Activate_NativeFallback();
        return;
    }

    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkGoapDebugger_SquadTable> WeakPanel{SharedThis(this)};
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPanel]() { return WeakPanel.IsValid(); });
    Data.Text.Add(TEXT("goap-squad-filter"), TAttribute<FText>::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkGoapDebugger_SquadTable> Panel = WeakPanel.Pin();
        return Panel.IsValid() ? FText::FromString(Panel->_FilterString) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("goap-squad-highlight"), TAttribute<FText>::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkGoapDebugger_SquadTable> Panel = WeakPanel.Pin();
        return Panel.IsValid() ? FText::FromString(Panel->_HighlightString) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("goap-squad-empty"), TAttribute<FText>::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkGoapDebugger_SquadTable> Panel = WeakPanel.Pin();
        if (!Panel.IsValid() || !Panel->_AuthoredCollection.IsValid() || !Panel->_AuthoredCollection->GetRecords().IsEmpty())
        { return FText::GetEmpty(); }
        return Panel->_RowsByPlanner.IsEmpty()
            ? FText::FromString(TEXT("No top-level planners in the active world."))
            : FText::FromString(TEXT("No planners match the current filter."));
    }));
    Data.Visibility.Add(TEXT("goap-squad-empty-visible"), TAttribute<bool>::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkGoapDebugger_SquadTable> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->_AuthoredCollection.IsValid() && Panel->_AuthoredCollection->GetRecords().IsEmpty();
    }));
    Data.TextChanged.Add(TEXT("goap-squad-filter"), FOnTextChanged::CreateLambda([WeakPanel](const FText& InText)
    {
        const TSharedPtr<SCkGoapDebugger_SquadTable> Panel = WeakPanel.Pin();
        if (Panel.IsValid() && Panel->_FilterString != InText.ToString())
        { Panel->_FilterString = InText.ToString(); Panel->RefreshFromViewModel(); }
    }));
    Data.TextChanged.Add(TEXT("goap-squad-highlight"), FOnTextChanged::CreateLambda([WeakPanel](const FText& InText)
    {
        const TSharedPtr<SCkGoapDebugger_SquadTable> Panel = WeakPanel.Pin();
        if (Panel.IsValid() && Panel->_HighlightString != InText.ToString())
        { Panel->_HighlightString = InText.ToString(); Panel->RefreshFromViewModel(); }
    }));
    Data.Collections.Add(TEXT("goap-squad-records"), _AuthoredCollection);
    Data.ItemActions.Add(TEXT("goap-squad-inspect"), FCkUiOnItemAction::CreateLambda([WeakPanel](const FString& InKey)
    {
        if (const TSharedPtr<SCkGoapDebugger_SquadTable> Panel = WeakPanel.Pin()) { Panel->On_AuthoredInspect(InKey); }
    }));
    Data.ItemActions.Add(TEXT("squad-entity-navigate"), FCkUiOnItemAction::CreateLambda([WeakPanel](const FString& InKey)
    {
        if (const TSharedPtr<SCkGoapDebugger_SquadTable> Panel = WeakPanel.Pin()) { Panel->On_AuthoredEntityNavigate(InKey); }
    }));
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create({}, {}, ck_goap_debugger_squad_table::StyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = Candidate->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Directory, TEXT("GoapDebuggerSquad.ui.html")),
        FPaths::Combine(Directory, TEXT("GoapDebuggerSquad.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        Activate_NativeFallback();
        return;
    }

    _AuthoredLoadFailure.Reset();
    _AuthoredView = Candidate;
    _ContentHost->SetContent(Main);
}

auto
    SCkGoapDebugger_SquadTable::
    Activate_NativeFallback()
    -> void
{
    _AuthoredView.Reset();
    if (_ContentHost.IsValid() && _NativeContent.IsValid()) { _ContentHost->SetContent(_NativeContent.ToSharedRef()); }
}

auto
    SCkGoapDebugger_SquadTable::
    On_AuthoredEntityNavigate(const FString& InKey)
    -> void
{
    const FAuthoredHandles* Handles = _AuthoredHandles.Find(InKey);
    if (Handles == nullptr || NOT ck::IsValid(Handles->Entity)) { return; }
    ck::DebugNav::Goto_Entity(Handles->Entity);
}

// ====================================================================================================================
// ROW GENERATION
// ====================================================================================================================

auto
    SCkGoapDebugger_SquadTable::
    OnGenerateRow(
        ItemPtr InItem,
        const TSharedRef<STableViewBase>& InTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_goap_debugger_squad_table;

    const auto& Row = *InItem;

    auto AlertsBox = SNew(SHorizontalBox);
    for (const auto& Alert : Row.AlertTags)
    {
        const auto IsBad = Alert == TEXT("no fallback");
        AlertsBox->AddSlot()
            .AutoWidth()
            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f))
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(FSlateColor(IsBad ? CkStyle::ErrDim() : CkStyle::WarnDim()))
                    .Padding(FMargin(CkStyle::SpaceS, 0.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Alert))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
                            .ColorAndOpacity(FSlateColor(IsBad ? CkStyle::Err() : CkStyle::Warn()))
                    ]
            ];
    }

    const auto EntityHandle  = Row.EntityHandle;
    const auto PlannerHandle = Row.PlannerHandle;

    return SNew(STableRow<ItemPtr>, InTable)
        .Padding(ck_goap_debugger_axes::Live_RowDensity(FMargin{0.0f, 1.0f}))
        [
            SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush())
                .BorderBackgroundColor(FSlateColor(Row.IsSelected ? CkStyle::AccentDim() : CkStyle::Bg2()))
                .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
                [
                    SNew(SHorizontalBox)

                        // Avatar + agent name
                        + SHorizontalBox::Slot()
                            .FillWidth(0.22f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(SHorizontalBox)

                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Center)
                                        .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
                                        [
                                            SNew(SBorder)
                                                .BorderImage(CkStyle::GetRoundedBrush())
                                                .BorderBackgroundColor(FSlateColor(CkStyle::Bg3()))
                                                .Padding(FMargin(CkStyle::SpaceS, 2.0f))
                                                [
                                                    SNew(STextBlock)
                                                        .Text(FText::FromString(Row.Avatar))
                                                        .Font_Lambda([]() -> FSlateFontInfo
                                                        { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
                                                        .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                                                ]
                                        ]

                                    + SHorizontalBox::Slot()
                                        .FillWidth(1.0f)
                                        .VAlign(VAlign_Center)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(Row.AgentName))
                                                .Font_Lambda([]() -> FSlateFontInfo
                                                { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeSmall()); })
                                                .ColorAndOpacity(FSlateColor(Row.IsHighlightMatch ? CkStyle::Text() : CkStyle::TextMute()))
                                                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                                        ]
                            ]

                        // Entity ID pill — gives the row an ID display,
                        // right-click-copy, and click-to-jump into the ECS
                        // debugger. ShowName(false): the adjacent name column
                        // already carries the agent name. Safe inside the row
                        // because the list is SelectionMode::None, so the pill's
                        // internal button traps no row selection.
                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, 0.0f))
                            [
                                SNew(SCkDebug_EntityRef)
                                    .Entity(Row.EntityHandle)
                                    .ShowName(false)
                            ]

                        // Planner label
                        + SHorizontalBox::Slot()
                            .FillWidth(0.12f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(Row.PlannerLabel))
                                    .Font_Lambda([]() -> FSlateFontInfo
                                    { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                            ]

                        // Status pill
                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, 0.0f))
                            [
                                SNew(SBox)
                                    .MinDesiredWidth(96.0f)
                                    [
                                        SNew(SCkDebug_StatusPill)
                                            .Text(FText::FromString(Get_StatusText(Row.PlanStatus, Row.IsDisabled)))
                                            .Tone(Get_StatusTone(Row.PlanStatus, Row.IsDisabled))
                                    ]
                            ]

                        // Active chain
                        + SHorizontalBox::Slot()
                            .FillWidth(0.34f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(Row.ChainText))
                                    .Font_Lambda([]() -> FSlateFontInfo
                                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                                    .ColorAndOpacity(FSlateColor(Row.IsHighlightMatch ? CkStyle::TextDim() : CkStyle::TextMute()))
                                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                            ]

                        // Cost
                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, 0.0f))
                            [
                                SNew(SBox)
                                    .MinDesiredWidth(44.0f)
                                    .HAlign(HAlign_Right)
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(Row.CostText))
                                            .Font_Lambda([]() -> FSlateFontInfo
                                            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); })
                                            .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                                    ]
                            ]

                        // Attempts
                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, 0.0f))
                            [
                                SNew(SBox)
                                    .MinDesiredWidth(28.0f)
                                    .HAlign(HAlign_Right)
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::AsNumber(Row.Attempts))
                                            .Font_Lambda([]() -> FSlateFontInfo
                                            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); })
                                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                    ]
                            ]

                        // Replans-60s sparkline
                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, 0.0f))
                            [
                                SNew(SBox)
                                    .WidthOverride(96.0f)
                                    .HeightOverride(20.0f)
                                    .ToolTipText(FText::FromString(TEXT("Replans over the last 60s (5s bins). A busy line = churn — check the replan policy / min interval.")))
                                    [
                                        SNew(SCkDebug_Sparkline)
                                            .Samples(Row.SparkSamples)
                                            .Color(CkStyle::Accent())
                                            .ShowEndDot(true)
                                    ]
                            ]

                        // Alert tags
                        + SHorizontalBox::Slot()
                            .FillWidth(0.16f)
                            .VAlign(VAlign_Center)
                            [
                                AlertsBox
                            ]

                        // Inspect
                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SButton)
                                    .ContentPadding(FMargin(CkStyle::SpaceS, 1.0f))
                                    .ToolTipText(FText::FromString(TEXT("Open this planner in the Agent Inspector.")))
                                    .OnClicked_Lambda([this, EntityHandle, PlannerHandle]() -> FReply
                                    {
                                        if (_OnInspect.IsBound())
                                        { _OnInspect.Execute(EntityHandle, PlannerHandle); }
                                        return FReply::Handled();
                                    })
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("Inspect \x203A")))
                                            .Font_Lambda([]() -> FSlateFontInfo
                                            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
                                            .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                                    ]
                            ]
                ]
        ];
}

// ====================================================================================================================
