#include "CkGoapDebugger/Window/SCkGoapDebugger_Sidebar.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

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

    auto HistoryEventColor_Sidebar(ECkGoapDebugger_HistoryEventKind InKind) -> FLinearColor
    {
        switch (InKind)
        {
        case ECkGoapDebugger_HistoryEventKind::PlanFailed:        return FCkGoapDebuggerStyle::Color_Status_Failed;
        case ECkGoapDebugger_HistoryEventKind::PlanFound:         return FCkGoapDebuggerStyle::Color_Status_PlanFound;
        case ECkGoapDebugger_HistoryEventKind::ActionSetDisabled: return FCkGoapDebuggerStyle::Color_Text_Faint;
        case ECkGoapDebugger_HistoryEventKind::ActionSetEnabled:  return FCkGoapDebuggerStyle::Color_Status_PlanFound;
        default:                                                  return FCkGoapDebuggerStyle::Color_Status_Planning;
        }
    }

    auto MakeStatusDot_Sidebar(const FLinearColor& InColor, float InSize = 8.0f) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(InSize)
            .HeightOverride(InSize)
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(InColor)
                    .Padding(FMargin(0.0f))
                    [
                        SNew(SSpacer)
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

        constexpr auto DotRadius = 4.0f;
        constexpr auto SelectedDotRadius = 6.0f;
        constexpr auto OutlineThickness = 1.5f;

        for (auto i = 0; i < Hist.Num(); ++i)
        {
            const auto& Ev = Hist[i];
            const auto CenterX = TrackPx[i];
            const auto IsSelected = (i == SelectedIdx);
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
        .SelectionMode(ESelectionMode::Single);

    auto ScrubTrack = SAssignNew(_ScrubTrack, SCkGoapDebugger_ScrubTrack,
                                 TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel))
        .OnEventClicked(SCkGoapDebugger_ScrubTrack::FOnEventClicked::CreateSP(
            this, &SCkGoapDebugger_Sidebar::SelectHistoryEvent));

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
            .Padding(FMargin(0.0f))
            [
                // Vertical splitter: tree (top) | history block (bottom).
                // User can drag the divider to resize either pane.
                SNew(SSplitter)
                    .Orientation(Orient_Vertical)

                    // ---- TOP : PLANNER TREE -------------------------------------
                    + SSplitter::Slot()
                        .Value(0.62f)
                        .MinSize(80.0f)
                        [
                            SNew(SVerticalBox)

                                + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Medium,
                                             FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small)
                                    [
                                        SNew(STextBlock)
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

                    // ---- BOTTOM : HISTORY (header + scrub + list) ---------------
                    + SSplitter::Slot()
                        .Value(0.38f)
                        .MinSize(80.0f)
                        [
                            SNew(SVerticalBox)

                                + SVerticalBox::Slot()
                                    .AutoHeight()
                                    [
                                        SNew(SSeparator)
                                            .Thickness(1.0f)
                                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Border_Strong))
                                    ]

                                + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small,
                                             FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small)
                                    [
                                        SNew(STextBlock)
                                            .Text_Lambda([this]() { return GetHistoryHeaderText(); })
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
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
                        ]
            ]
    ];
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
    return FText::FromString(FString::Printf(
        TEXT("HISTORY · %d events"), _HistoryItems.Num()));
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
                .Tone(ECkDebug_Tone::Accent)
                .ShowDot(false)
        ];

    if (InItem->IsActionRole)
    {
        BadgeBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(TEXT("ACTION")))
                    .Tone(ECkDebug_Tone::Info)
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
                                        return FSlateColor(ResolveStatusColor_Sidebar(Item->PlanStatus));
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

    // The new selection state is the Planner handle. We also synthesize the
    // legacy Planner selection so the existing PrimaryPane / Breadcrumb /
    // Graph (which still read SelectedActionSet) keep working until they get
    // migrated in U11.7-C/D.
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
        { _TreeView->SetItemSelection(*Found, true, ESelectInfo::Direct); }
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

    if (NewHash == _LastHistoryHash) { return; }
    _LastHistoryHash = NewHash;

    static constexpr auto MaxRowsToShow = 200;

    // Stable-identity rebuild: reuse existing TSharedPtr entries by key so
    // SListView selection / hover state survives every tick.
    // Key = (FrameNumber, EventKind) — unique per recorded event because events
    // fire at different frames; Kind disambiguates same-frame edge cases.
    const auto Start = FMath::Max(0, Hist.Num() - MaxRowsToShow);

    // Build the next map from the live history window.
    auto NextByKey = TMap<FHistoryKey, FHistoryItemPtr>{};
    NextByKey.Reserve(Hist.Num() - Start);

    _HistoryItems.Empty(Hist.Num() - Start);
    for (auto Idx = Hist.Num() - 1; Idx >= Start; --Idx)
    {
        const auto& Ev  = Hist[Idx];
        const auto  Key = FHistoryKey{ Ev.FrameNumber, Ev.Kind };

        FHistoryItemPtr Item;
        if (auto* Found = _HistoryItemsByKey.Find(Key))
        {
            Item = *Found;
            // Update in-place so any changed metadata (Title, Meta, Snapshot)
            // is reflected without reallocating the TSharedPtr.
            *Item = Ev;
        }
        else
        {
            Item = MakeShared<FCkGoapDebugger_HistoryEvent>(Ev);
        }

        NextByKey.Add(Key, Item);
        _HistoryItems.Add(Item);
    }

    // Evict keys no longer in the visible window.
    _HistoryItemsByKey = MoveTemp(NextByKey);

    if (_HistoryListView.IsValid()) { _HistoryListView->RequestListRefresh(); }
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

    const auto DotColor = HistoryEventColor_Sidebar(InItem->Kind);
    const auto TitleText = InItem->Title.IsEmpty()
        ? HistoryKindLabel_Sidebar(InItem->Kind)
        : InItem->Title;

    return SNew(FRowType, InOwnerTable)
        .Padding(FMargin(2.0f, 1.0f))
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4.0f, 0.0f)
                    [
                        MakeStatusDot_Sidebar(DotColor, 8.0f)
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .Padding(2.0f, 0.0f)
                    [
                        SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TitleText))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                                ]
                            + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(InItem->Meta))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
                                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                                ]
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4.0f, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FormatTimestamp_Sidebar(InItem->WorldTimeSeconds)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
                    ]
        ];
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
    if (NOT InItem.IsValid())   { return; }
    if (NOT _ViewModel.IsValid()) { return; }

    const auto RowIdx = _HistoryItems.IndexOfByKey(InItem);
    if (RowIdx == INDEX_NONE) { return; }

    const auto Entity = _ViewModel->GetSelectedEntity();
    if (NOT ck::IsValid(Entity)) { return; }

    const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(Entity);
    const auto HistIdx = Hist.Num() - 1 - RowIdx;
    if (NOT Hist.IsValidIndex(HistIdx)) { return; }

    SelectHistoryEvent(HistIdx);
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

    const auto RowIdx = Hist.Num() - 1 - HistIdx;
    if (NOT _HistoryItems.IsValidIndex(RowIdx))
    { _HistoryListView->ClearSelection(); return; }

    _HistoryListView->SetSelection(_HistoryItems[RowIdx], ESelectInfo::Direct);
    _HistoryListView->RequestScrollIntoView(_HistoryItems[RowIdx]);
}

// ====================================================================================================================
