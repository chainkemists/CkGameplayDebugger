#include "CkGoapDebugger/Window/SCkGoapDebugger_SquadTable.h"

#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DecisionModel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkEditorTools/Style/CkStyle.h"

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

    auto Compute_Initials(const FString& InName) -> FString
    {
        auto Parts = TArray<FString>{};
        InName.ParseIntoArrayWS(Parts);
        if (Parts.Num() >= 2) { return (Parts[0].Left(1) + Parts.Last().Left(1)).ToUpper(); }
        return InName.Left(2).ToUpper();
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

    // Active-chain display: walk Plan[0] names down the sub-planner chain.
    // Names run through the shared name-depth tuner.
    auto Compute_ChainText(const FCkGoapDebugger_PlannerInfo& InPlanner, int32 InNameDepth) -> FString
    {
        auto Names = TArray<FString>{};
        const auto* Cursor = &InPlanner;
        while (Cursor != nullptr && NOT Cursor->PlanClassNames.IsEmpty())
        {
            Names.Add(SCkDebug_NameLabel::Get_ShortName(Cursor->PlanClassNames[0], InNameDepth));

            const FCkGoapDebugger_PlannerInfo* Next = nullptr;
            if (Cursor->PlanHandles.IsValidIndex(0))
            {
                const auto& Step = Cursor->PlanHandles[0];
                Next = Cursor->ChildPlanners.FindByPredicate(
                    [&Step](const FCkGoapDebugger_PlannerInfo& In)
                    { return static_cast<FCk_Handle>(In.PlannerHandle) == static_cast<FCk_Handle>(Step); });
            }
            Cursor = Next;
        }
        return Names.Num() > 0 ? FString::Join(Names, TEXT(" › ")) : FString(TEXT("\x2014"));
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

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
            .Padding(FMargin(CkStyle::SpaceL))
            [
                SAssignNew(_ListView, SListView<ItemPtr>)
                    .ListItemsSource(&_Visible)
                    .SelectionMode(ESelectionMode::None)
                    .OnGenerateRow(this, &SCkGoapDebugger_SquadTable::OnGenerateRow)
            ]
    ];

    RefreshFromViewModel();
}

auto
    SCkGoapDebugger_SquadTable::
    Reset_ForWorldChange()
    -> void
{
    _RowsByPlanner.Reset();
    _Visible.Reset();
    _LastHash = 0;

    if (_ListView.IsValid())
    { _ListView->RequestListRefresh(); }
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
    using namespace ck_goap_debugger_decision_model;

    if (NOT _ViewModel.IsValid() || NOT _ListView.IsValid()) { return; }

    const auto& Snapshots = _ViewModel->GetAllEntitySnapshots();

    // Hash — planner set + per-planner surface state.
    auto NewHash = uint32{0};
    for (const auto& Snapshot : Snapshots)
    {
        for (const auto& Planner : Snapshot.TopLevelPlanners)
        {
            auto RowHash = GetTypeHash(Planner.PlannerHandle);
            RowHash = HashCombine(RowHash, ::GetTypeHash(static_cast<uint8>(Planner.PlanStatus)));
            RowHash = HashCombine(RowHash, ::GetTypeHash(static_cast<uint8>(Planner.EnableToggle)));
            RowHash = HashCombine(RowHash, ::GetTypeHash(Planner.PlanCost));
            RowHash = HashCombine(RowHash, ::GetTypeHash(Planner.PlanAttemptCount));
            for (const auto& StepName : Planner.PlanClassNames)
            { RowHash = HashCombine(RowHash, GetTypeHash(StepName)); }
            NewHash ^= RowHash;
        }
        NewHash = HashCombine(NewHash, ::GetTypeHash(Snapshot.FrameNumber));
    }
    NewHash = HashCombine(NewHash, GetTypeHash(_ViewModel->GetSelectedActionSet()));
    // Chain names run through the shared name-depth tuner.
    NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth()));

    if (NewHash == _LastHash) { return; }
    _LastHash = NewHash;

    // ---- Rebuild rows (stable identity by planner handle) ----------------------
    auto NewVisible = TArray<ItemPtr>{};
    auto SeenPlanners = TSet<FCk_Handle_Goap_Planner>{};

    for (const auto& Snapshot : Snapshots)
    {
        const auto& History = FCkGoapDebugger_DataCollector::GetHistory(Snapshot.EntityHandle);
        const auto NowSeconds = Snapshot.WorldTimeSeconds;

        for (const auto& Planner : Snapshot.TopLevelPlanners)
        {
            SeenPlanners.Add(Planner.PlannerHandle);

            auto& Row = _RowsByPlanner.FindOrAdd(Planner.PlannerHandle);
            if (NOT Row.IsValid())
            {
                Row = MakeShared<FSquadRow>();
                Row->SparkSamples = MakeShared<TArray<float>>();
            }

            Row->EntityHandle  = Snapshot.EntityHandle;
            Row->PlannerHandle = Planner.PlannerHandle;
            Row->AgentName     = Snapshot.DebugName;
            Row->Avatar        = Compute_Initials(Snapshot.DebugName);
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

            // Alerts.
            Row->AlertTags.Reset();
            {
                const auto ChainLeafIsFallback =
                    NOT Planner.PlanClassNames.IsEmpty() &&
                    Planner.ChildActions.ContainsByPredicate(
                        [&Planner](const FCkGoapDebugger_ActionInfo& In)
                        {
                            return In.ClassName == Planner.PlanClassNames[0] &&
                                   In.Cost >= k_FallbackCostFloor;
                        });
                if (ChainLeafIsFallback)
                { Row->AlertTags.Add(TEXT("fallback")); }

                if (NOT Planner.HasUnconditionalFallback && NOT Planner.AllowPlanFailed)
                { Row->AlertTags.Add(TEXT("no fallback")); }

                if (Planner.PlanStatus == ECk_GoapPlanStatus::CostThresholdReached)
                { Row->AlertTags.Add(TEXT("threshold")); }
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
            }

            NewVisible.Add(Row);
        }
    }

    // Drop rows whose planner vanished.
    for (auto It = _RowsByPlanner.CreateIterator(); It; ++It)
    {
        if (NOT SeenPlanners.Contains(It->Key)) { It.RemoveCurrent(); }
    }

    _Visible = MoveTemp(NewVisible);
    _ListView->RequestListRefresh();
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
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(IsBad ? CkStyle::Err() : CkStyle::Warn()))
                    ]
            ];
    }

    const auto EntityHandle  = Row.EntityHandle;
    const auto PlannerHandle = Row.PlannerHandle;

    return SNew(STableRow<ItemPtr>, InTable)
        .Padding(FMargin(0.0f, 1.0f))
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
                                                        .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                                                        .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                                                ]
                                        ]

                                    + SHorizontalBox::Slot()
                                        .FillWidth(1.0f)
                                        .VAlign(VAlign_Center)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(Row.AgentName))
                                                .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                                                .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                                                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                                        ]
                            ]

                        // Planner label
                        + SHorizontalBox::Slot()
                            .FillWidth(0.12f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(Row.PlannerLabel))
                                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
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
                                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
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
                                            .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
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
                                            .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
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
                                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                                            .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                                    ]
                            ]
                ]
        ];
}

// ====================================================================================================================
