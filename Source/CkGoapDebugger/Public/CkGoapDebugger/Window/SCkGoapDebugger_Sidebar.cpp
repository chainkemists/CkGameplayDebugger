#include "CkGoapDebugger/Window/SCkGoapDebugger_Sidebar.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_HistoryModel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Rendering/DrawElements.h"

// ====================================================================================================================
// Internal helpers (file-prefixed to dodge anonymous-namespace unity collisions
// — recurring across debugger .cpp files when names like MakeBadge, MakeRow,
// Duration_OneFrame are reused).
// ====================================================================================================================

namespace
{
    // ---- Per-Planner status colour (drives left dot) -------------------------

    auto ResolveStatusColor_Sidebar(ECk_GoapPlanStatus InStatus) -> FLinearColor
    {
        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:  return FCkGoapDebuggerStyle::Color_Status_PlanFound;
        case ECk_GoapPlanStatus::Planning:   return FCkGoapDebuggerStyle::Color_Status_Planning;
        case ECk_GoapPlanStatus::PlanFailed: return FCkGoapDebuggerStyle::Color_Status_Failed;
        default:                             return FCkGoapDebuggerStyle::Color_Text_Muted;
        }
    }

    // Variant that honours the per-Planner _AllowPlanFailed opt-out: when the
    // Planner explicitly tolerates PlanFailed, we render the status dot in
    // amber (Color_Status_Selected) instead of error red, so users don't read
    // the row as a misconfiguration.
    auto ResolveStatusColorWithOptOut_Sidebar(
        ECk_GoapPlanStatus InStatus,
        bool InAllowPlanFailed) -> FLinearColor
    {
        if (InStatus == ECk_GoapPlanStatus::PlanFailed && InAllowPlanFailed)
        { return FCkGoapDebuggerStyle::Color_Status_Selected; }
        return ResolveStatusColor_Sidebar(InStatus);
    }

    auto HistoryEventColor_Sidebar(ECkGoapDebugger_HistoryEventKind InKind) -> FLinearColor
    {
        switch (InKind)
        {
        case ECkGoapDebugger_HistoryEventKind::PlanFailed:        return FCkGoapDebuggerStyle::Color_Status_Failed;
        case ECkGoapDebugger_HistoryEventKind::PlanFound:         return FCkGoapDebuggerStyle::Color_Status_PlanFound;
        case ECkGoapDebugger_HistoryEventKind::ActionSetDisabled: return FCkGoapDebuggerStyle::Color_Text_Faint;
        case ECkGoapDebugger_HistoryEventKind::ActionSetEnabled:  return FCkGoapDebuggerStyle::Color_Status_PlanFound;
        case ECkGoapDebugger_HistoryEventKind::ActionDeactivated: return FCkGoapDebuggerStyle::Color_Text_Muted;
        default:                                                  return FCkGoapDebuggerStyle::Color_Status_Planning;
        }
    }

    auto HistoryKindShort_Sidebar(ECkGoapDebugger_HistoryEventKind InKind) -> FString
    {
        switch (InKind)
        {
        case ECkGoapDebugger_HistoryEventKind::ActionActivated:   return TEXT("ACT");
        case ECkGoapDebugger_HistoryEventKind::ActionDeactivated: return TEXT("DEACT");
        case ECkGoapDebugger_HistoryEventKind::PlanFound:         return TEXT("PLAN");
        case ECkGoapDebugger_HistoryEventKind::PlanFailed:        return TEXT("FAIL");
        case ECkGoapDebugger_HistoryEventKind::ChainReset:        return TEXT("RESET");
        case ECkGoapDebugger_HistoryEventKind::ActionSetEnabled:  return TEXT("ON");
        case ECkGoapDebugger_HistoryEventKind::ActionSetDisabled: return TEXT("OFF");
        case ECkGoapDebugger_HistoryEventKind::ChainActivated:    return TEXT("CHAIN");
        default:                                                  return TEXT("?");
        }
    }

    // Compact toned pill for the event kind (ACT / DEACT / PLAN / ...), so the kind is visually
    // separable from the action name. Plain SBorder + STextBlock — safe inside an STableRow.
    auto MakeKindPill_Sidebar(ECkGoapDebugger_HistoryEventKind InKind) -> TSharedRef<SWidget>
    {
        const auto Tone = HistoryEventColor_Sidebar(InKind);
        auto Bg = Tone;
        Bg.A = 0.18f;
        return SNew(SBox)
            .MinDesiredWidth(46.0f)
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(FSlateColor(Bg))
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(5.0f, 1.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(HistoryKindShort_Sidebar(InKind)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
                            .ColorAndOpacity(FSlateColor(Tone))
                    ]
            ];
    }

    auto MakeFlapPill_Sidebar() -> TSharedRef<SWidget>
    {
        const auto Tone = FCkGoapDebuggerStyle::Color_Status_Selected;   // amber accent for flap storms
        auto Bg = Tone;
        Bg.A = 0.18f;
        return SNew(SBox)
            .MinDesiredWidth(46.0f)
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(FSlateColor(Bg))
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(5.0f, 1.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("FLAP")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
                            .ColorAndOpacity(FSlateColor(Tone))
                    ]
            ];
    }

    auto FormatTimestamp_Sidebar(double InWorldTime) -> FString
    {
        const auto Total = FMath::Max(0.0, InWorldTime);
        const auto Minutes = static_cast<int32>(Total) / 60;
        const auto Seconds = static_cast<int32>(Total) % 60;
        const auto Ms = static_cast<int32>((Total - FMath::Floor(Total)) * 1000.0);
        return FString::Printf(TEXT("%02d:%02d.%03d"), Minutes, Seconds, Ms);
    }

    auto HistoryKindLabel_Sidebar(ECkGoapDebugger_HistoryEventKind InKind) -> FString
    {
        switch (InKind)
        {
        case ECkGoapDebugger_HistoryEventKind::ActionSetEnabled:  return TEXT("ActionSetEnabled");
        case ECkGoapDebugger_HistoryEventKind::ActionSetDisabled: return TEXT("ActionSetDisabled");
        case ECkGoapDebugger_HistoryEventKind::ChainActivated:    return TEXT("ChainActivated");
        case ECkGoapDebugger_HistoryEventKind::ActionActivated:   return TEXT("ActionActivated");
        case ECkGoapDebugger_HistoryEventKind::ActionDeactivated: return TEXT("ActionDeactivated");
        case ECkGoapDebugger_HistoryEventKind::PlanFound:         return TEXT("PlanFound");
        case ECkGoapDebugger_HistoryEventKind::PlanFailed:        return TEXT("PlanFailed");
        case ECkGoapDebugger_HistoryEventKind::ChainReset:        return TEXT("ChainReset");
        default:                                                  return TEXT("?");
        }
    }

    // ---- Recursive planner-set walker ----------------------------------------
    // Counts every Planner in the forest (top-level + descendants) so the
    // header can show "N top-level · M total Planners".
    auto CountAllPlanners_Sidebar(const TArray<FCkGoapDebugger_PlannerInfo>& InPlanners) -> int32
    {
        auto N = 0;
        for (const auto& P : InPlanners)
        {
            ++N;
            N += CountAllPlanners_Sidebar(P.ChildPlanners);
        }
        return N;
    }
}

// ====================================================================================================================
// SCRUB TRACK — custom leaf widget. Paints chain/failure dots positioned
// proportionally along a thin horizontal track. Same behaviour as pre-U11.7-B;
// only renamed where collisions might emerge.
// ====================================================================================================================

class SCkGoapDebugger_ScrubTrack : public SLeafWidget
{
public:
    DECLARE_DELEGATE_OneParam(FOnEventClicked, int32 /*HistIdx*/);

    SLATE_BEGIN_ARGS(SCkGoapDebugger_ScrubTrack) {}
        SLATE_EVENT(FOnEventClicked, OnEventClicked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TWeakPtr<FCkGoapDebugger_ViewModel> InViewModel) -> void
    {
        _ViewModel = InViewModel;
        _OnEventClicked = InArgs._OnEventClicked;
        SetCanTick(false);
    }

    virtual auto ComputeDesiredSize(float) const -> FVector2D override
    { return FVector2D(120.0f, 22.0f); }

    virtual auto OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const -> int32 override
    {
        const auto Local = AllottedGeometry.GetLocalSize();
        const auto TrackY = Local.Y * 0.5f;
        const auto TrackThickness = 2.0f;
        const auto* WhiteBrush = FAppStyle::GetBrush(TEXT("WhiteBrush"));

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(
                FVector2f(Local.X, TrackThickness),
                FSlateLayoutTransform(FVector2f(0.0f, TrackY - TrackThickness * 0.5f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            FCkGoapDebuggerStyle::Color_Border_Subtle);

        constexpr auto NowBarWidth = 2.0f;
        constexpr auto NowBarHeight = 14.0f;
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 1,
            AllottedGeometry.ToPaintGeometry(
                FVector2f(NowBarWidth, NowBarHeight),
                FSlateLayoutTransform(FVector2f(Local.X - NowBarWidth, TrackY - NowBarHeight * 0.5f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            FCkGoapDebuggerStyle::Color_Status_PlanFound);

        const auto VM = _ViewModel.Pin();
        if (NOT VM.IsValid())
        { return LayerId + 2; }

        const auto Entity = VM->GetSelectedEntity();
        if (NOT ck::IsValid(Entity))
        { return LayerId + 2; }

        const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(Entity);
        if (Hist.Num() == 0)
        { return LayerId + 2; }

        const auto SelectedIdx = (VM->GetMode() == FCkGoapDebugger_ViewModel::EMode::Scrub)
            ? VM->GetScrubEventIndex() : INDEX_NONE;

        const auto TrackPx = ComputeTrackPositions(Hist, Local.X);

        // Flap-run bands: a storm renders as one amber band over its true time window instead of
        // dozens of crushed dots. Computed from the same grouping the list uses.
        const auto Groups = ck_goap_debugger_history_model::BuildPlannerGroups(Hist,
            [](const FCk_Handle_Goap_Planner&) { return FString{}; });
        auto FlapSpans = TArray<TPair<double, double>>{};
        for (const auto& Group : Groups)
        {
            for (const auto& Row : Group.Rows)
            { if (Row.IsFlap) { FlapSpans.Emplace(Row.FlapTStart, Row.FlapTEnd); } }
        }

        const auto T0     = Hist[0].WorldTimeSeconds;
        const auto T1     = Hist.Last().WorldTimeSeconds;
        const auto TSpan  = FMath::Max(0.001, T1 - T0);
        constexpr auto EdgePad = 8.0f;
        const auto Usable = FMath::Max(1.0f, Local.X - EdgePad * 2.0f);
        const auto TimeToX = [&](double InT) -> float
        { return EdgePad + Usable * FMath::Clamp(static_cast<float>((InT - T0) / TSpan), 0.0f, 1.0f); };

        constexpr auto BandHeight = 9.0f;
        for (const auto& Sp : FlapSpans)
        {
            const auto X0 = TimeToX(Sp.Key);
            const auto W  = FMath::Max(2.0f, TimeToX(Sp.Value) - X0);
            FSlateDrawElement::MakeBox(
                OutDrawElements, LayerId + 2,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(W, BandHeight),
                    FSlateLayoutTransform(FVector2f(X0, TrackY - BandHeight * 0.5f))),
                WhiteBrush, ESlateDrawEffect::None, FCkGoapDebuggerStyle::Color_Status_Selected);
        }

        const auto IsInFlap = [&](double InT) -> bool
        {
            for (const auto& Sp : FlapSpans) { if (InT >= Sp.Key && InT <= Sp.Value) { return true; } }
            return false;
        };

        constexpr auto DotRadius = 4.0f;
        constexpr auto SelectedDotRadius = 6.0f;
        constexpr auto OutlineThickness = 1.5f;

        for (auto i = 0; i < Hist.Num(); ++i)
        {
            const auto& Ev = Hist[i];
            const auto CenterX = TrackPx[i];
            const auto IsSelected = (i == SelectedIdx);
            if (NOT IsSelected && IsInFlap(Ev.WorldTimeSeconds)) { continue; }   // covered by the band
            const auto R = IsSelected ? SelectedDotRadius : DotRadius;

            const auto Color = HistoryEventColor_Sidebar(Ev.Kind);

            if (IsSelected)
            {
                const auto Ring = R + OutlineThickness;
                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    LayerId + 2,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2f(Ring * 2.0f, Ring * 2.0f),
                        FSlateLayoutTransform(FVector2f(CenterX - Ring, TrackY - Ring))),
                    WhiteBrush,
                    ESlateDrawEffect::None,
                    FCkGoapDebuggerStyle::Color_Text_Primary);
            }

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 3,
                AllottedGeometry.ToPaintGeometry(
                    FVector2f(R * 2.0f, R * 2.0f),
                    FSlateLayoutTransform(FVector2f(CenterX - R, TrackY - R))),
                WhiteBrush,
                ESlateDrawEffect::None,
                Color);
        }

        return LayerId + 4;
    }

    virtual auto OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override
    {
        if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
        { return FReply::Unhandled(); }

        const auto VM = _ViewModel.Pin();
        if (NOT VM.IsValid())
        { return FReply::Unhandled(); }

        const auto Entity = VM->GetSelectedEntity();
        if (NOT ck::IsValid(Entity))
        { return FReply::Unhandled(); }

        const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(Entity);
        if (Hist.Num() == 0)
        { return FReply::Unhandled(); }

        const auto Local   = MyGeometry.GetLocalSize();
        const auto LocalP  = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
        const auto TrackPx = ComputeTrackPositions(Hist, Local.X);

        constexpr auto HitRadius = 10.0f;
        auto BestIdx  = int32{INDEX_NONE};
        auto BestDist = HitRadius;

        for (auto i = 0; i < TrackPx.Num(); ++i)
        {
            const auto Dist = FMath::Abs(static_cast<float>(LocalP.X) - TrackPx[i]);
            if (Dist < BestDist)
            {
                BestDist = Dist;
                BestIdx  = i;
            }
        }

        if (BestIdx != INDEX_NONE && _OnEventClicked.IsBound())
        {
            _OnEventClicked.Execute(BestIdx);
            return FReply::Handled();
        }

        return FReply::Unhandled();
    }

private:
    static auto ComputeTrackPositions(
        const TArray<FCkGoapDebugger_HistoryEvent>& InHist,
        float InWidth) -> TArray<float>
    {
        auto Out = TArray<float>{};
        Out.Reserve(InHist.Num());

        if (InHist.Num() == 0) { return Out; }

        constexpr auto EdgePad = 8.0f;
        const auto Usable = FMath::Max(1.0f, InWidth - EdgePad * 2.0f);

        if (InHist.Num() == 1)
        {
            Out.Add(EdgePad + Usable * 0.5f);
            return Out;
        }

        const auto T0 = InHist[0].WorldTimeSeconds;
        const auto T1 = InHist.Last().WorldTimeSeconds;
        const auto Span = FMath::Max(0.001, T1 - T0);

        for (const auto& Ev : InHist)
        {
            const auto Alpha = static_cast<float>((Ev.WorldTimeSeconds - T0) / Span);
            Out.Add(EdgePad + Usable * FMath::Clamp(Alpha, 0.0f, 1.0f));
        }

        return Out;
    }

    TWeakPtr<FCkGoapDebugger_ViewModel> _ViewModel;
    FOnEventClicked                     _OnEventClicked;
};

// ====================================================================================================================
// CONSTRUCT / DESTRUCT
// ====================================================================================================================

auto
    SCkGoapDebugger_Sidebar::
    Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkGoapDebugger_ViewModel> InViewModel)
    -> void
{
    _ViewModel = InViewModel;

    // Cache leaf widgets at Construct (never destructively reset ChildSlot in
    // refresh paths — call RequestTreeRefresh / RequestListRefresh instead).
    auto TreeView = SAssignNew(_TreeView, STreeView<FRowItemPtr>)
        .TreeItemsSource(&_RootNodes)
        .OnGenerateRow(this, &SCkGoapDebugger_Sidebar::GenerateRow)
        .OnGetChildren(this, &SCkGoapDebugger_Sidebar::GetTreeChildren)
        .OnSelectionChanged(this, &SCkGoapDebugger_Sidebar::OnSelectionChanged)
        .SelectionMode(ESelectionMode::Single);

    auto HistoryList = SAssignNew(_HistoryListView, SListView<FHistoryItemPtr>)
        .ListItemsSource(&_HistoryItems)
        .OnGenerateRow(this, &SCkGoapDebugger_Sidebar::GenerateHistoryRow)
        .OnSelectionChanged(this, &SCkGoapDebugger_Sidebar::OnHistoryRowSelectionChanged)
        .OnContextMenuOpening(this, &SCkGoapDebugger_Sidebar::OnHistoryContextMenu)
        .SelectionMode(ESelectionMode::Multi);

    auto ScrubTrack = SAssignNew(_ScrubTrack, SCkGoapDebugger_ScrubTrack,
                                 TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel))
        .OnEventClicked(SCkGoapDebugger_ScrubTrack::FOnEventClicked::CreateSP(
            this, &SCkGoapDebugger_Sidebar::SelectHistoryEvent));

    // History block is built here but parented by the WINDOW into a full-width bottom dock
    // (see Get_HistoryWidget). The sidebar's own ChildSlot holds only the planner tree.
    _HistorySection =
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small,
                                 FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small)
                        [
                            SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SCkDebug_SelectableLabel)
                                            .Text_Lambda([this]() { return GetHistoryHeaderText(); })
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SButton)
                                            .ToolTipText(NSLOCTEXT("CkGoapDebugger", "CopyAllTip", "Copy entire history to clipboard"))
                                            .ContentPadding(FMargin(6.0f, 1.0f))
                                            .OnClicked_Lambda([this]()
                                            {
                                                FPlatformApplicationMisc::ClipboardCopy(*BuildCopyText(_HistoryItems));
                                                return FReply::Handled();
                                            })
                                            [
                                                SNew(STextBlock)
                                                    .Text(NSLOCTEXT("CkGoapDebugger", "CopyAllBtn", "Copy"))
                                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                                            ]
                                    ]
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FCkGoapDebuggerStyle::Padding_Medium, 0.0f,
                                 FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small)
                        [
                            ScrubTrack
                        ]

                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            HistoryList
                        ]
            ];

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Medium,
                                 FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small)
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text_Lambda([this]() { return GetPlannerTreeHeaderText(); })
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SSeparator)
                                .Thickness(1.0f)
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Border_Subtle))
                        ]

                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            TreeView
                        ]
            ]
    ];
}

auto
    SCkGoapDebugger_Sidebar::
    Get_HistoryWidget()
    -> TSharedRef<SWidget>
{
    return _HistorySection.IsValid() ? _HistorySection.ToSharedRef() : SNullWidget::NullWidget;
}

SCkGoapDebugger_Sidebar::~SCkGoapDebugger_Sidebar() = default;

// ====================================================================================================================
// PUBLIC LIFECYCLE
// ====================================================================================================================

auto
    SCkGoapDebugger_Sidebar::
    Reset_ForWorldChange()
    -> void
{
    _RootNodes.Empty();
    _RowItemsByHandle.Empty();
    _HistoryItems.Empty();
    _HistoryItemsByKey.Empty();
    _MaterializedEntity     = FCk_Handle{};
    _LastHistoryEntity      = FCk_Handle{};
    _LastTreeStructureHash  = 0;
    _LastHistoryHash        = 0;

    if (_TreeView.IsValid())        { _TreeView->RequestTreeRefresh(); }
    if (_HistoryListView.IsValid()) { _HistoryListView->RebuildList(); }
}

auto
    SCkGoapDebugger_Sidebar::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const auto StructureChanged = RebuildTreeStructure();
    RebuildHistoryItems();

    if (StructureChanged && _TreeView.IsValid())
    {
        _TreeView->RequestTreeRefresh();
        ExpandAll(_RootNodes);
    }

    SyncTreeSelectionFromViewModel();
    SyncHistoryListSelectionFromViewModel();

    if (_ScrubTrack.IsValid())
    { _ScrubTrack->Invalidate(EInvalidateWidgetReason::Paint); }
}

// ====================================================================================================================
// PRIVATE — header text
// ====================================================================================================================

auto
    SCkGoapDebugger_Sidebar::
    GetPlannerTreeHeaderText() const
    -> FText
{
    if (NOT _ViewModel.IsValid())
    { return FText::FromString(TEXT("PLANNER TREE")); }

    const auto* Snap = _ViewModel->GetCurrentEntitySnapshot();
    if (Snap == nullptr)
    { return FText::FromString(TEXT("PLANNER TREE · 0")); }

    const auto TopCount   = Snap->TopLevelPlanners.Num();
    const auto TotalCount = CountAllPlanners_Sidebar(Snap->TopLevelPlanners);

    return FText::FromString(FString::Printf(
        TEXT("PLANNER TREE · %d top-level · %d total (%s)"),
        TopCount, TotalCount, *Snap->DebugName));
}

auto
    SCkGoapDebugger_Sidebar::
    GetHistoryHeaderText() const
    -> FText
{
    auto EventCount = 0;
    if (_ViewModel.IsValid())
    {
        const auto Entity = _ViewModel->GetSelectedEntity();
        if (ck::IsValid(Entity))
        { EventCount = FCkGoapDebugger_DataCollector::GetHistory(Entity).Num(); }
    }
    return FText::FromString(FString::Printf(TEXT("HISTORY · %d events"), EventCount));
}

// ====================================================================================================================
// PRIVATE — tree build
// ====================================================================================================================

namespace
{
    // Walk the snapshot's PlannerInfo forest and produce (or refresh) a parallel
    // FRowItemPtr tree, reusing existing items by PlannerHandle so STreeView
    // selection identity is preserved across refreshes.
    auto BuildOrUpdateRowTree_Sidebar(
        const TArray<FCkGoapDebugger_PlannerInfo>& InPlanners,
        int32 InDepth,
        TMap<FCk_Handle_Goap_Planner, SCkGoapDebugger_Sidebar::FRowItemPtr>& InExisting,
        TMap<FCk_Handle_Goap_Planner, SCkGoapDebugger_Sidebar::FRowItemPtr>& OutNext)
        -> TArray<SCkGoapDebugger_Sidebar::FRowItemPtr>
    {
        auto Out = TArray<SCkGoapDebugger_Sidebar::FRowItemPtr>{};
        Out.Reserve(InPlanners.Num());

        for (const auto& P : InPlanners)
        {
            // Dedupe defence: if the data collector ever surfaces the same
            // PlannerHandle twice within a single forest level (e.g. via a
            // future bug in the recursive walker), reusing the same
            // FRowItemPtr in Out would let STreeView treat both visual
            // positions as a single selection target — clicking one row
            // would highlight every occurrence. Skip duplicates here so each
            // PlannerHandle is rendered exactly once per level.
            if (OutNext.Contains(P.PlannerHandle)) { continue; }

            auto Item = SCkGoapDebugger_Sidebar::FRowItemPtr{};
            if (auto* Found = InExisting.Find(P.PlannerHandle))
            {
                Item = *Found;
            }
            else
            {
                Item = MakeShared<SCkGoapDebugger_Sidebar::FRowItem>();
            }

            Item->PlannerHandle    = P.PlannerHandle;
            Item->DisplayName      = P.DisplayName.IsEmpty()
                ? P.PlannerTag.ToString()
                : P.DisplayName;
            Item->PlannerTag       = P.PlannerTag;
            Item->IsActionRole     = P.IsActionRole;
            Item->IsInActiveChain  = P.IsInActiveChain;
            Item->AllowPlanFailed  = P.AllowPlanFailed;
            Item->PlanStatus       = P.PlanStatus;
            Item->Depth            = InDepth;
            Item->Children         = BuildOrUpdateRowTree_Sidebar(
                P.ChildPlanners, InDepth + 1, InExisting, OutNext);

            OutNext.Add(P.PlannerHandle, Item);
            Out.Add(Item);
        }
        return Out;
    }
}

auto
    SCkGoapDebugger_Sidebar::
    RebuildTreeStructure()
    -> bool
{
    if (NOT _ViewModel.IsValid())
    {
        if (_RootNodes.Num() == 0 && _RowItemsByHandle.Num() == 0) { return false; }
        _RootNodes.Empty();
        _RowItemsByHandle.Empty();
        _MaterializedEntity    = FCk_Handle{};
        _LastTreeStructureHash = 0;
        return true;
    }

    const auto* Snap = _ViewModel->GetCurrentEntitySnapshot();

    // Compute structural hash — entity + recursive PlannerHandle set + role
    // bits. Plan status is intentionally excluded so we don't rebuild every
    // tick; per-row visuals re-read it via TAttribute lambdas.
    auto NewHash = uint32{0};
    auto HashPlanners = static_cast<TFunction<void(const TArray<FCkGoapDebugger_PlannerInfo>&)>>(nullptr);
    HashPlanners = [&NewHash, &HashPlanners](const TArray<FCkGoapDebugger_PlannerInfo>& InP) -> void
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(InP.Num()));
        for (const auto& P : InP)
        {
            NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(P.PlannerHandle)));
            NewHash = HashCombine(NewHash, ::GetTypeHash(P.IsActionRole));
            NewHash = HashCombine(NewHash, ::GetTypeHash(P.IsInActiveChain));
            NewHash = HashCombine(NewHash, ::GetTypeHash(P.AllowPlanFailed));
            HashPlanners(P.ChildPlanners);
        }
    };

    if (Snap != nullptr)
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(Snap->EntityHandle));
        HashPlanners(Snap->TopLevelPlanners);
    }

    if (NewHash == _LastTreeStructureHash && (Snap == nullptr ? NOT ck::IsValid(_MaterializedEntity)
                                                              : Snap->EntityHandle == _MaterializedEntity))
    { return false; }

    _LastTreeStructureHash = NewHash;
    _MaterializedEntity    = (Snap != nullptr) ? Snap->EntityHandle : FCk_Handle{};

    // Reuse-by-handle: walk the snapshot, pulling existing FRowItemPtr entries
    // when their PlannerHandle still exists. Vanished entries fall out because
    // OutNext omits them.
    auto NextByHandle = TMap<FCk_Handle_Goap_Planner, FRowItemPtr>{};
    if (Snap != nullptr)
    {
        _RootNodes = BuildOrUpdateRowTree_Sidebar(
            Snap->TopLevelPlanners, /*Depth=*/0, _RowItemsByHandle, NextByHandle);
    }
    else
    {
        _RootNodes.Empty();
    }
    _RowItemsByHandle = MoveTemp(NextByHandle);

    return true;
}

// ====================================================================================================================
// PRIVATE — tree row generation
// ====================================================================================================================

auto
    SCkGoapDebugger_Sidebar::
    GenerateRow(
        FRowItemPtr InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using FRowType = STableRow<FRowItemPtr>;

    if (NOT InItem.IsValid())
    {
        return SNew(FRowType, InOwnerTable)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(invalid)")))
            ];
    }

    const auto WeakItem = TWeakPtr<FRowItem>(InItem);

    // Indent: STreeView already supplies a chevron-indent; add a small extra
    // shim so deeper nodes read clearly.
    const auto ExtraIndent = static_cast<float>(InItem->Depth) * 4.0f;

    // Role badge cluster. The mockup shows two states:
    //   - PLANNER          (planner-only)
    //   - PLANNER + ACTION (dual-role; mid-tier composite)
    auto BadgeBox = SNew(SHorizontalBox);

    BadgeBox->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
        [
            SNew(SCkDebug_StatusPill)
                .Text(FText::FromString(TEXT("PLANNER")))
                .Tone(ECk_Tone::Accent)
                .ShowDot(false)
        ];

    if (InItem->IsActionRole)
    {
        BadgeBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
            [
                SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(TEXT("ACTION")))
                    .Tone(ECk_Tone::Info)
                    .ShowDot(false)
            ];
    }

    // Opt-out badge — visible when the Planner has _AllowPlanFailed=true.
    // Surfaces "PlanFailed here is intentional, not a misconfiguration."
    // Positioned to the right of PLANNER/ACTION so it stands out.
    if (InItem->AllowPlanFailed)
    {
        BadgeBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(TEXT("OPT-OUT")))
                    .Tone(ECk_Tone::Warn)
                    .ShowDot(false)
            ];
    }

    return SNew(FRowType, InOwnerTable)
        .Padding(FMargin(2.0f, 2.0f))
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)

                // Optional depth shim
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SBox).WidthOverride(ExtraIndent)
                    ]

                // Status dot (live PlanStatus via lambda)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                    [
                        SNew(SBox)
                            .WidthOverride(8.0f)
                            .HeightOverride(8.0f)
                            [
                                SNew(SBorder)
                                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                    .BorderBackgroundColor_Lambda([WeakItem]() -> FSlateColor
                                    {
                                        const auto Item = WeakItem.Pin();
                                        if (NOT Item.IsValid())
                                        { return FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted); }
                                        return FSlateColor(ResolveStatusColorWithOptOut_Sidebar(
                                            Item->PlanStatus, Item->AllowPlanFailed));
                                    })
                                    .Padding(FMargin(0.0f))
                                    [
                                        SNew(SSpacer)
                                    ]
                            ]
                    ]

                // Planner display name (fills)
                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .Padding(2.0f, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(InItem->DisplayName.IsEmpty()
                                ? TEXT("(no name)") : InItem->DisplayName))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                    ]

                // Role badge cluster (right-aligned)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(6.0f, 0.0f, 4.0f, 0.0f)
                    [
                        BadgeBox
                    ]
        ];
}

auto
    SCkGoapDebugger_Sidebar::
    GetTreeChildren(
        FRowItemPtr InItem,
        TArray<FRowItemPtr>& OutChildren)
    -> void
{
    if (NOT InItem.IsValid()) { return; }
    OutChildren = InItem->Children;
}

auto
    SCkGoapDebugger_Sidebar::
    OnSelectionChanged(
        FRowItemPtr InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (_SuppressSelectionEcho)        { return; }
    if (InSelectInfo == ESelectInfo::Direct) { return; }
    if (NOT _ViewModel.IsValid() || NOT InItem.IsValid()) { return; }

    // The selection state is the Planner handle — every Mission Control pane
    // (AgentColumn, Decision, Catalog, Graph, WS rail) reads SelectedActionSet.
    _ViewModel->SetSelectedActionSet(InItem->PlannerHandle);

    // Clear any stale Action selection — the new tree is Planner-only.
    _ViewModel->SetSelectedAction(FCk_Handle_Goap_Action{});
}

auto
    SCkGoapDebugger_Sidebar::
    SyncTreeSelectionFromViewModel()
    -> void
{
    if (NOT _TreeView.IsValid())    { return; }
    if (NOT _ViewModel.IsValid())   { return; }

    const auto Target = _ViewModel->GetSelectedActionSet();
    auto Guard = TGuardValue<bool>(_SuppressSelectionEcho, true);

    if (NOT ck::IsValid(Target))
    {
        _TreeView->ClearSelection();
        return;
    }

    if (auto* Found = _RowItemsByHandle.Find(Target))
    {
        const auto Cur = _TreeView->GetSelectedItems();
        const auto AlreadySelected = (Cur.Num() == 1 && Cur[0] == *Found);
        if (NOT AlreadySelected)
        {
            // CAUTION: this uses SetItemSelection (additive), NOT SetSelection
            // (replacing). SetSelection IS structurally more correct for a
            // Single-mode tree, BUT switching to it unmasked a deeper bug: the
            // ViewModel's Tick validation periodically clears _SelectedActionSet
            // for sub-Planner handles (the recursive FindPlannerInfo walk works
            // on paper but something is returning null in practice), then auto-
            // restores to TopLevelPlanners[0]. With SetSelection, the next Sync
            // call snaps the visible selection from the user's click to the
            // first top-level planner — making sub-Planner inspection
            // impossible. SetItemSelection accidentally "rescues" the user's
            // click because Slate's internal selection set retains it on top of
            // the auto-restored first planner; the cosmetic side effect is the
            // multi-select-stuck symptom reported in 2026-05-27, but the
            // functional impact (can still inspect sub-Planners) is preferable
            // to the snap-back. Proper fix lives in the ViewModel's auto-
            // restore-first logic and/or the snapshot-rebuild path; deferred.
            _TreeView->SetItemSelection(*Found, true, ESelectInfo::Direct);
        }
    }
}

auto
    SCkGoapDebugger_Sidebar::
    ExpandAll(
        const TArray<FRowItemPtr>& InNodes)
    -> void
{
    if (NOT _TreeView.IsValid()) { return; }

    for (const auto& N : InNodes)
    {
        if (NOT N.IsValid()) { continue; }
        _TreeView->SetItemExpansion(N, true);
        if (N->Children.Num() > 0)
        { ExpandAll(N->Children); }
    }
}

// ====================================================================================================================
// PRIVATE — history list
// ====================================================================================================================

auto
    SCkGoapDebugger_Sidebar::
    RebuildHistoryItems()
    -> void
{
    if (NOT _ViewModel.IsValid())
    {
        if (_HistoryItems.Num() == 0 && NOT ck::IsValid(_LastHistoryEntity)) { return; }
        _HistoryItems.Empty();
        _HistoryItemsByKey.Empty();
        _LastHistoryEntity = FCk_Handle{};
        _LastHistoryHash   = 0;
        if (_HistoryListView.IsValid()) { _HistoryListView->RebuildList(); }
        return;
    }

    const auto Entity = _ViewModel->GetSelectedEntity();

    // Entity-switch hard reset. The stable-TSharedPtr-by-key scheme below is
    // safe within one entity's session, but row items keyed by FrameNumber+Kind
    // from the previous entity would otherwise be evicted only *after* SListView
    // had a chance to see a smaller source array containing keys it doesn't
    // know — tripping FWidgetGenerator::ValidateWidgetGeneration. RebuildList
    // (not RequestListRefresh) is what evicts the WidgetMap entries.
    if (Entity != _LastHistoryEntity)
    {
        _HistoryItems.Empty();
        _HistoryItemsByKey.Empty();
        _LastHistoryHash   = 0;
        _LastHistoryEntity = Entity;
        if (_HistoryListView.IsValid()) { _HistoryListView->RebuildList(); }
    }

    if (NOT ck::IsValid(Entity))
    {
        if (_HistoryItems.Num() == 0) { return; }
        _HistoryItems.Empty();
        _HistoryItemsByKey.Empty();
        if (_HistoryListView.IsValid()) { _HistoryListView->RebuildList(); }
        return;
    }

    const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(Entity);

    auto NewHash = uint32{0};
    // Fold entity handle into the hash so future same-count + same-last-frame
    // edge cases on a different entity don't masquerade as "no change".
    NewHash = HashCombine(NewHash, ::GetTypeHash(Entity));
    NewHash = HashCombine(NewHash, ::GetTypeHash(Hist.Num()));
    if (Hist.Num() > 0) { NewHash = HashCombine(NewHash, ::GetTypeHash(Hist.Last().FrameNumber)); }
    // Re-render rows when name-depth verbosity changes.
    NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth()));

    if (NewHash == _LastHistoryHash) { return; }
    _LastHistoryHash = NewHash;

    static constexpr auto MaxRowsToShow = 200;
    const auto Start = FMath::Max(0, Hist.Num() - MaxRowsToShow);

    // Window the live history (oldest..newest) and remember each event's history index so collapsed /
    // grouped rows can still map back to a scrub target (last writer of a frame wins — adequate since
    // a same-frame snapshot is interchangeable for scrub).
    auto WindowEvents = TArray<FCkGoapDebugger_HistoryEvent>{};
    auto FrameToIdx   = TMap<int64, int32>{};
    WindowEvents.Reserve(Hist.Num() - Start);
    for (auto Idx = Start; Idx < Hist.Num(); ++Idx)
    {
        WindowEvents.Add(Hist[Idx]);
        FrameToIdx.Add(Hist[Idx].FrameNumber, Idx);
    }

    const auto Groups = ck_goap_debugger_history_model::BuildPlannerGroups(WindowEvents,
        [this](const FCk_Handle_Goap_Planner& InPlanner) { return Get_PlannerDisplayName(InPlanner); });

    const auto CurrentNameDepth = _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1;
    const auto NameDepthChanged = (_LastHistoryNameDepth != CurrentNameDepth);
    _LastHistoryNameDepth = CurrentNameDepth;

    // Stable-identity rebuild: reuse existing entry pointers by key so SListView selection survives
    // refreshes where the set is unchanged. Header key = planner; single key = (frame, kind, action);
    // flap key = (planner, FIRST-event frame) so the entry stays stable while a storm extends it.
    auto NextByKey = TMap<FHistoryKey, FHistoryItemPtr>{};
    auto NewItems  = TArray<FHistoryItemPtr>{};

    const auto Reuse = [this](const FString& InKey) -> FHistoryItemPtr
    {
        if (auto* Found = _HistoryItemsByKey.Find(InKey)) { return *Found; }
        return MakeShared<FCkGoapDebugger_HistoryListEntry>();
    };

    for (const auto& Group : Groups)
    {
        const auto HeaderKey = FString::Printf(TEXT("H|%u"), ::GetTypeHash(Group.Planner));
        auto Header = Reuse(HeaderKey);
        Header->IsGroupHeader = true;
        Header->PlannerName   = Group.PlannerName;
        Header->Planner       = Group.Planner;
        Header->RepHistIdx    = INDEX_NONE;
        Header->Key           = HeaderKey;
        NextByKey.Add(HeaderKey, Header);
        NewItems.Add(Header);

        for (auto r = Group.Rows.Num() - 1; r >= 0; --r)   // newest-first within the group
        {
            const auto& Row = Group.Rows[r];

            FString Key;
            int64   RepFrame = 0;
            if (Row.IsFlap)
            {
                const auto FirstFrame = Row.RawEvents.Num() > 0 ? Row.RawEvents[0].FrameNumber : 0;
                RepFrame = Row.RawEvents.Num() > 0 ? Row.RawEvents.Last().FrameNumber : 0;
                Key = FString::Printf(TEXT("F|%u|%lld"), ::GetTypeHash(Group.Planner), static_cast<long long>(FirstFrame));
            }
            else
            {
                RepFrame = Row.Event.FrameNumber;
                Key = FString::Printf(TEXT("S|%lld|%d|%s"),
                    static_cast<long long>(Row.Event.FrameNumber), static_cast<int32>(Row.Event.Kind), *Row.Event.ActionClassName);
            }

            auto Item = Reuse(Key);
            Item->IsGroupHeader = false;
            Item->Planner       = Group.Planner;
            Item->Row           = Row;
            Item->Key           = Key;
            Item->RepHistIdx    = FrameToIdx.Contains(RepFrame) ? FrameToIdx[RepFrame] : INDEX_NONE;
            NextByKey.Add(Key, Item);
            NewItems.Add(Item);
        }
    }

    const auto MembershipChanged =
        NameDepthChanged
        || NextByKey.Num() != _HistoryItemsByKey.Num()
        || [&]() {
            for (const auto& Kv : NextByKey)
            { if (NOT _HistoryItemsByKey.Contains(Kv.Key)) { return true; } }
            return false;
        }();

    _HistoryItems      = MoveTemp(NewItems);
    _HistoryItemsByKey = MoveTemp(NextByKey);

    if (_HistoryListView.IsValid())
    {
        if (MembershipChanged) { _HistoryListView->RebuildList(); }
        else                   { _HistoryListView->RequestListRefresh(); }
    }
}

auto
    SCkGoapDebugger_Sidebar::
    GenerateHistoryRow(
        FHistoryItemPtr InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using FRowType = STableRow<FHistoryItemPtr>;

    if (NOT InItem.IsValid())
    { return SNew(FRowType, InOwnerTable); }

    const auto Depth = _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1;

    // ---- Planner-group header --------------------------------------------------------------------
    if (InItem->IsGroupHeader)
    {
        return SNew(FRowType, InOwnerTable)
            .Padding(FMargin(2.0f, 3.0f, 2.0f, 1.0f))
            .ShowSelection(false)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("▼")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
                        ]
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(SCkDebug_NameLabel::Get_ShortName(InItem->PlannerName, Depth)))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                        ]
            ];
    }

    const auto& Row = InItem->Row;

    // ---- Collapsed flap-run row ------------------------------------------------------------------
    if (Row.IsFlap)
    {
        const auto A    = SCkDebug_NameLabel::Get_ShortName(Row.FlapActionA, Depth);
        const auto B    = SCkDebug_NameLabel::Get_ShortName(Row.FlapActionB, Depth);
        const auto Body = FString::Printf(TEXT("%s  →  %s   x%d"), *A, *B, Row.FlapCount);
        const auto Span = FString::Printf(TEXT("%s–%s"),
            *FormatTimestamp_Sidebar(Row.FlapTStart), *FormatTimestamp_Sidebar(Row.FlapTEnd));

        return SNew(FRowType, InOwnerTable)
            .Padding(FMargin(2.0f, 1.0f))
            .ShowSelection(true)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f, 4.0f, 0.0f)
                        [ MakeFlapPill_Sidebar() ]
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(6.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Body))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                        ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Span))
                                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
                        ]
            ];
    }

    // ---- Single event row ------------------------------------------------------------------------
    const auto& Ev = Row.Event;
    auto NameText = FString{};
    if (NOT Ev.ActionClassName.IsEmpty())
    { NameText = SCkDebug_NameLabel::Get_ShortName(Ev.ActionClassName, Depth); }
    else
    { NameText = Ev.Title.IsEmpty() ? HistoryKindLabel_Sidebar(Ev.Kind) : Ev.Title; }

    return SNew(FRowType, InOwnerTable)
        .Padding(FMargin(2.0f, 1.0f))
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f, 4.0f, 0.0f)
                    [ MakeKindPill_Sidebar(Ev.Kind) ]

                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(6.0f, 0.0f)
                    [
                        SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(NameText))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                                ]
                            + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(Ev.Meta))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
                                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                                        .Visibility(Ev.Meta.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
                                ]
                    ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FormatTimestamp_Sidebar(Ev.WorldTimeSeconds)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
                    ]
        ];
}

// ====================================================================================================================
// PRIVATE — copy / export
// ====================================================================================================================

auto
    SCkGoapDebugger_Sidebar::
    Get_PlannerDisplayName(
        const FCk_Handle_Goap_Planner& InPlanner) const
    -> FString
{
    if (ck::Is_NOT_Valid(InPlanner))
    { return TEXT("<no planner>"); }
    return UCk_Utils_Handle_UE::Get_DebugName(InPlanner).ToString();
}

auto
    SCkGoapDebugger_Sidebar::
    BuildCopyText(
        const TArray<FHistoryItemPtr>& InItems) const
    -> FString
{
    auto Events = TArray<FCkGoapDebugger_HistoryEvent>{};
    for (const auto& It : InItems)
    {
        if (NOT It.IsValid() || It->IsGroupHeader) { continue; }
        if (It->Row.IsFlap)
        { for (const auto& Ev : It->Row.RawEvents) { Events.Add(Ev); } }   // expand flap to raw ticks
        else
        { Events.Add(It->Row.Event); }
    }
    const auto Header = FString::Printf(TEXT("GOAP history - %d events"), Events.Num());
    return ck_goap_debugger_history_model::SerializeHistory(Header, Events,
        [this](const FCk_Handle_Goap_Planner& InPlanner) { return Get_PlannerDisplayName(InPlanner); });
}

auto
    SCkGoapDebugger_Sidebar::
    OnHistoryContextMenu()
    -> TSharedPtr<SWidget>
{
    if (NOT _HistoryListView.IsValid())
    { return nullptr; }

    auto Menu = FMenuBuilder{true, nullptr};
    const auto Selected = _HistoryListView->GetSelectedItems();
    if (Selected.Num() > 0)
    {
        ck::DebugCopyMenu::AddCopyEntry(Menu,
            NSLOCTEXT("CkGoapDebugger", "CopySel", "Copy selected"),
            NSLOCTEXT("CkGoapDebugger", "CopySelTip", "Copy the selected history rows as text"),
            BuildCopyText(Selected));
    }
    ck::DebugCopyMenu::AddCopyEntry(Menu,
        NSLOCTEXT("CkGoapDebugger", "CopyAllMenu", "Copy all"),
        NSLOCTEXT("CkGoapDebugger", "CopyAllMenuTip", "Copy the entire history as text"),
        BuildCopyText(_HistoryItems));
    return Menu.MakeWidget();
}

// ====================================================================================================================
// PRIVATE — scrub interaction
// ====================================================================================================================

auto
    SCkGoapDebugger_Sidebar::
    OnHistoryRowSelectionChanged(
        FHistoryItemPtr InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (_SuppressSelectionEcho) { return; }
    if (InSelectInfo == ESelectInfo::Direct) { return; }
    if (NOT InItem.IsValid())     { return; }
    if (InItem->IsGroupHeader)    { return; }
    if (InItem->RepHistIdx == INDEX_NONE) { return; }

    SelectHistoryEvent(InItem->RepHistIdx);
}

auto
    SCkGoapDebugger_Sidebar::
    SelectHistoryEvent(
        int32 InHistIdx)
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const auto Entity = _ViewModel->GetSelectedEntity();
    if (NOT ck::IsValid(Entity)) { return; }

    const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(Entity);
    if (NOT Hist.IsValidIndex(InHistIdx)) { return; }

    const auto& Event = Hist[InHistIdx];
    if (ck::IsValid(Event.ActionSetHandle))
    { _ViewModel->SetSelectedActionSet(Event.ActionSetHandle); }

    _ViewModel->SetMode(FCkGoapDebugger_ViewModel::EMode::Scrub);
    _ViewModel->SetScrubEventIndex(InHistIdx);
}

auto
    SCkGoapDebugger_Sidebar::
    SyncHistoryListSelectionFromViewModel()
    -> void
{
    if (NOT _HistoryListView.IsValid()) { return; }
    if (NOT _ViewModel.IsValid())       { return; }

    const auto Mode    = _ViewModel->GetMode();
    const auto HistIdx = _ViewModel->GetScrubEventIndex();

    auto Guard = TGuardValue<bool>(_SuppressSelectionEcho, true);

    if (Mode != FCkGoapDebugger_ViewModel::EMode::Scrub || HistIdx == INDEX_NONE)
    {
        _HistoryListView->ClearSelection();
        return;
    }

    const auto Entity = _ViewModel->GetSelectedEntity();
    if (NOT ck::IsValid(Entity))
    { _HistoryListView->ClearSelection(); return; }

    const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(Entity);
    if (NOT Hist.IsValidIndex(HistIdx))
    { _HistoryListView->ClearSelection(); return; }

    FHistoryItemPtr Match;
    for (const auto& It : _HistoryItems)
    {
        if (It.IsValid() && NOT It->IsGroupHeader && It->RepHistIdx == HistIdx)
        { Match = It; break; }
    }
    if (NOT Match.IsValid())
    { _HistoryListView->ClearSelection(); return; }

    _HistoryListView->SetSelection(Match, ESelectInfo::Direct);
    _HistoryListView->RequestScrollIntoView(Match);
}

// ====================================================================================================================
