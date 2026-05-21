#include "CkGoapDebugger/Window/SCkGoapDebugger_Sidebar.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

// ====================================================================================================================
// Internal helpers
// ====================================================================================================================

namespace
{
    // ---- Status badge helpers --------------------------------------------------

    struct FStatusBadge
    {
        FString        Label;
        FLinearColor   Color;
    };

    auto ResolveActionSetBadge(const FCkGoapDebugger_ActionSetInfo& InAs) -> FStatusBadge
    {
        if (InAs.EnableToggle == ECk_EnableDisable::Disable)
        { return {TEXT("Disabled"), FCkGoapDebuggerStyle::Color_Text_Faint}; }

        // Derive from the root Action's PlanStatus when available.
        auto Status = ECk_GoapPlanStatus::Idle;
        for (const auto& Action : InAs.Catalog)
        {
            if (Action.Handle == InAs.RootActionHandle)
            {
                Status = Action.PlanStatus;
                break;
            }
        }

        switch (Status)
        {
        case ECk_GoapPlanStatus::PlanFound:  return {TEXT("PlanFound"), FCkGoapDebuggerStyle::Color_Status_PlanFound};
        case ECk_GoapPlanStatus::Planning:   return {TEXT("Planning"),  FCkGoapDebuggerStyle::Color_Status_Planning};
        case ECk_GoapPlanStatus::PlanFailed: return {TEXT("Failed"),    FCkGoapDebuggerStyle::Color_Status_Failed};
        default:                             return {TEXT("Idle"),      FCkGoapDebuggerStyle::Color_Text_Muted};
        }
    }

    auto ResolveActionStatusColor(const FCkGoapDebugger_ActionInfo& InAction) -> FLinearColor
    {
        switch (InAction.PlanStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:  return FCkGoapDebuggerStyle::Color_Status_PlanFound;
        case ECk_GoapPlanStatus::Planning:   return FCkGoapDebuggerStyle::Color_Status_Planning;
        case ECk_GoapPlanStatus::PlanFailed: return FCkGoapDebuggerStyle::Color_Status_Failed;
        default:                             return FCkGoapDebuggerStyle::Color_Text_Muted;
        }
    }

    auto RoleLabel(ECkGoapDebugger_ActionRole InRole) -> FString
    {
        switch (InRole)
        {
        case ECkGoapDebugger_ActionRole::Root:    return TEXT("root");
        case ECkGoapDebugger_ActionRole::Mid:     return TEXT("mid");
        case ECkGoapDebugger_ActionRole::Leaf:    return TEXT("leaf");
        case ECkGoapDebugger_ActionRole::Catalog: return TEXT("catalog");
        default:                                  return TEXT("");
        }
    }

    auto HistoryEventColor(ECkGoapDebugger_HistoryEventKind InKind) -> FLinearColor
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

    // ---- Tiny rounded badge (text inside a tinted rounded border) -----------

    auto MakeBadge(const FString& InText, const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        const auto BgColor = FLinearColor(InColor.R, InColor.G, InColor.B, 0.13f);

        return SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BgColor)
            .Padding(FMargin(6.0f, 1.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(InText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FSlateColor(InColor))
            ];
    }

    // ---- Small colored dot ---------------------------------------------------

    auto MakeStatusDot(const FLinearColor& InColor, float InSize = 8.0f) -> TSharedRef<SWidget>
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

    // ---- Format a timestamp (seconds) into a short HH:MM:SS.ms-ish label ----

    auto FormatTimestamp(double InWorldTime) -> FString
    {
        const auto Total = FMath::Max(0.0, InWorldTime);
        const auto Minutes = static_cast<int32>(Total) / 60;
        const auto Seconds = static_cast<int32>(Total) % 60;
        const auto Ms = static_cast<int32>((Total - FMath::Floor(Total)) * 1000.0);
        return FString::Printf(TEXT("%02d:%02d.%03d"), Minutes, Seconds, Ms);
    }

    auto HistoryKindLabel(ECkGoapDebugger_HistoryEventKind InKind) -> FString
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
}

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

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                // ---- ACTIONSETS header --------------------------------------
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Medium,
                             FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([this]() { return GetActionSetsHeaderText(); })
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

                // ---- ActionSet / chain tree (top, fills) --------------------
                + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    .Padding(FMargin(0.0f))
                    [
                        SAssignNew(_TreeView, STreeView<TSharedPtr<FNode>>)
                            .TreeItemsSource(&_RootNodes)
                            .OnGenerateRow(this, &SCkGoapDebugger_Sidebar::GenerateRow)
                            .OnGetChildren(this, &SCkGoapDebugger_Sidebar::GetTreeChildren)
                            .OnSelectionChanged(this, &SCkGoapDebugger_Sidebar::OnSelectionChanged)
                            .SelectionMode(ESelectionMode::Single)
                    ]

                // ---- Divider between tree and history -----------------------
                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SSeparator)
                            .Thickness(1.0f)
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Border_Strong))
                    ]

                // ---- HISTORY header -----------------------------------------
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

                // ---- Placeholder scrub track (filled out in D6) -------------
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FCkGoapDebuggerStyle::Padding_Medium, 0.0f,
                             FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small)
                    [
                        SNew(SBox).HeightOverride(4.0f)
                            [
                                SNew(SBorder)
                                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                    .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Border_Subtle)
                                    .Padding(FMargin(0.0f))
                                    [
                                        SNew(SSpacer)
                                    ]
                            ]
                    ]

                // ---- History list (bottom, fixed height) --------------------
                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBox)
                            .HeightOverride(FCkGoapDebuggerStyle::SidebarBottomHeight)
                            [
                                SAssignNew(_HistoryListView, SListView<FHistoryItemPtr>)
                                    .ListItemsSource(&_HistoryItems)
                                    .OnGenerateRow(this, &SCkGoapDebugger_Sidebar::GenerateHistoryRow)
                                    .SelectionMode(ESelectionMode::None)
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
    // Drop shared-ptr tree/list items so embedded FCk_Handle copies release
    // their registry refs before the registry dies.
    _RootNodes.Empty();
    _HistoryItems.Empty();
    _MaterializedEntity = FCk_Handle{};
    _LastTreeStructureHash = 0;
    _LastHistoryHash       = 0;

    if (_TreeView.IsValid())  { _TreeView->RequestTreeRefresh(); }
    if (_HistoryListView.IsValid()) { _HistoryListView->RequestListRefresh(); }
}

auto
    SCkGoapDebugger_Sidebar::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const auto TreeChanged    = RebuildTreeStructure();
    RebuildHistoryItems();

    if (TreeChanged && _TreeView.IsValid())
    {
        _TreeView->RequestTreeRefresh();

        // Default-expand all ActionSet rows so the chain is visible.
        for (const auto& Root : _RootNodes)
        {
            if (Root.IsValid())
            { _TreeView->SetItemExpansion(Root, true); }
        }
    }
}

// ====================================================================================================================
// PRIVATE — header text
// ====================================================================================================================

auto
    SCkGoapDebugger_Sidebar::
    GetActionSetsHeaderText() const
    -> FText
{
    if (NOT _ViewModel.IsValid())
    { return FText::FromString(TEXT("ACTIONSETS")); }

    const auto* Snap = _ViewModel->GetCurrentEntitySnapshot();
    if (Snap == nullptr)
    { return FText::FromString(TEXT("ACTIONSETS · 0")); }

    return FText::FromString(FString::Printf(
        TEXT("ACTIONSETS · %d (%s)"), Snap->ActionSets.Num(), *Snap->DebugName));
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

auto
    SCkGoapDebugger_Sidebar::
    RebuildTreeStructure()
    -> bool
{
    if (NOT _ViewModel.IsValid())
    {
        if (_RootNodes.Num() == 0) { return false; }
        _RootNodes.Empty();
        _MaterializedEntity = FCk_Handle{};
        _LastTreeStructureHash = 0;
        return true;
    }

    const auto* Snap = _ViewModel->GetCurrentEntitySnapshot();

    // Compute structural hash — entity + per-ActionSet (handle, chain length,
    // catalog length, chain handles). This intentionally excludes per-frame
    // status values so we don't rebuild every tick.
    auto NewHash = uint32{0};

    if (Snap != nullptr)
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(Snap->EntityHandle));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Snap->ActionSets.Num()));

        for (const auto& As : Snap->ActionSets)
        {
            NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(As.Handle)));
            NewHash = HashCombine(NewHash, ::GetTypeHash(As.ActiveChainHandles.Num()));
            NewHash = HashCombine(NewHash, ::GetTypeHash(As.Catalog.Num()));

            for (const auto& ChainHandle : As.ActiveChainHandles)
            { NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(ChainHandle))); }
        }
    }

    if (NewHash == _LastTreeStructureHash && (Snap == nullptr ? NOT ck::IsValid(_MaterializedEntity)
                                                              : Snap->EntityHandle == _MaterializedEntity))
    { return false; }

    _LastTreeStructureHash = NewHash;
    _MaterializedEntity    = (Snap != nullptr) ? Snap->EntityHandle : FCk_Handle{};

    _RootNodes.Empty();
    if (Snap == nullptr) { return true; }

    for (const auto& As : Snap->ActionSets)
    {
        auto AsNode = MakeShared<FNode>();
        AsNode->Kind               = ENodeKind::ActionSet;
        AsNode->ActionSetHandle    = As.Handle;
        AsNode->ActionSetDebugName = As.DebugName;

        // Build chain children in order. ActiveChainHandles is empty when
        // there's no plan; we still always show the root if it exists in
        // the catalog so the user can pick it.
        auto HandlesToShow = TArray<FCk_Handle_Goap_Action>{};
        if (As.ActiveChainHandles.Num() > 0)
        {
            HandlesToShow = As.ActiveChainHandles;
        }
        else if (ck::IsValid(As.RootActionHandle))
        {
            HandlesToShow.Add(As.RootActionHandle);
        }

        for (auto ChainIdx = 0; ChainIdx < HandlesToShow.Num(); ++ChainIdx)
        {
            const auto& ChainHandle = HandlesToShow[ChainIdx];

            // Look up the matching ActionInfo from the catalog.
            const auto* ActionInfo = As.Catalog.FindByPredicate(
                [&](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == ChainHandle; });
            if (ActionInfo == nullptr) { continue; }

            auto ActNode = MakeShared<FNode>();
            ActNode->Kind            = ENodeKind::Action;
            ActNode->ActionHandle    = ActionInfo->Handle;
            ActNode->ActionClassName = ActionInfo->ClassName.IsEmpty()
                ? TEXT("(unknown class)")
                : ActionInfo->ClassName;
            ActNode->ActionTagText   = ActionInfo->ActionTag.IsValid()
                ? ActionInfo->ActionTag.ToString()
                : FString{};
            ActNode->Role            = ActionInfo->Role;
            ActNode->ChainDepth      = ActionInfo->ChainDepth >= 0 ? ActionInfo->ChainDepth : ChainIdx;

            AsNode->Children.Add(MoveTemp(ActNode));
        }

        _RootNodes.Add(MoveTemp(AsNode));
    }

    return true;
}

// ====================================================================================================================
// PRIVATE — tree row generation
// ====================================================================================================================

auto
    SCkGoapDebugger_Sidebar::
    GenerateRow(
        TSharedPtr<FNode> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using FRowType = STableRow<TSharedPtr<FNode>>;

    if (NOT InItem.IsValid())
    {
        return SNew(FRowType, InOwnerTable)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(invalid)")))
            ];
    }

    if (InItem->Kind == ENodeKind::ActionSet)
    {
        const auto WeakItem = TWeakPtr<FNode>(InItem);
        const auto WeakVM   = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel);

        const auto AsHandle = InItem->ActionSetHandle;

        auto BadgeBox = SNew(SHorizontalBox);

        // Bind badge text/color via a lambda over the catalog re-fetched each
        // paint. Keeps the badge live without rebuilding the row.
        BadgeBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor_Lambda([WeakVM, AsHandle]() -> FSlateColor
                    {
                        auto Color = FCkGoapDebuggerStyle::Color_Text_Muted;
                        if (const auto VM = WeakVM.Pin())
                        {
                            if (const auto* Snap = VM->GetCurrentEntitySnapshot())
                            {
                                if (const auto* As = Snap->ActionSets.FindByPredicate(
                                    [&](const FCkGoapDebugger_ActionSetInfo& In) { return In.Handle == AsHandle; }))
                                {
                                    Color = ResolveActionSetBadge(*As).Color;
                                }
                            }
                        }
                        return FSlateColor(FLinearColor(Color.R, Color.G, Color.B, 0.13f));
                    })
                    .Padding(FMargin(6.0f, 1.0f))
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([WeakVM, AsHandle]() -> FText
                            {
                                if (const auto VM = WeakVM.Pin())
                                {
                                    if (const auto* Snap = VM->GetCurrentEntitySnapshot())
                                    {
                                        if (const auto* As = Snap->ActionSets.FindByPredicate(
                                            [&](const FCkGoapDebugger_ActionSetInfo& In) { return In.Handle == AsHandle; }))
                                        {
                                            return FText::FromString(ResolveActionSetBadge(*As).Label);
                                        }
                                    }
                                }
                                return FText::FromString(TEXT("?"));
                            })
                            .ColorAndOpacity_Lambda([WeakVM, AsHandle]() -> FSlateColor
                            {
                                if (const auto VM = WeakVM.Pin())
                                {
                                    if (const auto* Snap = VM->GetCurrentEntitySnapshot())
                                    {
                                        if (const auto* As = Snap->ActionSets.FindByPredicate(
                                            [&](const FCkGoapDebugger_ActionSetInfo& In) { return In.Handle == AsHandle; }))
                                        {
                                            return FSlateColor(ResolveActionSetBadge(*As).Color);
                                        }
                                    }
                                }
                                return FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted);
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    ]
            ];

        return SNew(FRowType, InOwnerTable)
            .Padding(FMargin(2.0f, 2.0f))
            .ShowSelection(true)
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        .Padding(2.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(InItem->ActionSetDebugName.IsEmpty()
                                    ? TEXT("(no name)") : InItem->ActionSetDebugName))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.0f, 0.0f)
                        [
                            BadgeBox
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(6.0f, 0.0f, 4.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text_Lambda([WeakVM, AsHandle]() -> FText
                                {
                                    if (const auto VM = WeakVM.Pin())
                                    {
                                        if (const auto* Snap = VM->GetCurrentEntitySnapshot())
                                        {
                                            if (const auto* As = Snap->ActionSets.FindByPredicate(
                                                [&](const FCkGoapDebugger_ActionSetInfo& In) { return In.Handle == AsHandle; }))
                                            {
                                                return FText::FromString(FString::Printf(TEXT("%d actions"), As->Catalog.Num()));
                                            }
                                        }
                                    }
                                    return FText::GetEmpty();
                                })
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                        ]
            ];
    }

    // Action row
    {
        const auto Indent = FMath::Max(0, InItem->ChainDepth) * 10.0f;
        const auto WeakItem = TWeakPtr<FNode>(InItem);
        const auto WeakVM   = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel);
        const auto ActionHandle = InItem->ActionHandle;

        return SNew(FRowType, InOwnerTable)
            .Padding(FMargin(2.0f, 1.0f))
            .ShowSelection(true)
            [
                SNew(SHorizontalBox)

                    // Indent
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SBox).WidthOverride(Indent)
                        ]

                    // Chain glyph
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(InItem->ChainDepth > 0
                                    ? FString(TEXT("└▸")) : FString(TEXT("▸"))))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                        ]

                    // Status dot — bound to live PlanStatus
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                        [
                            SNew(SBox)
                                .WidthOverride(8.0f)
                                .HeightOverride(8.0f)
                                [
                                    SNew(SBorder)
                                        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                        .BorderBackgroundColor_Lambda([WeakVM, ActionHandle]() -> FSlateColor
                                        {
                                            if (const auto VM = WeakVM.Pin())
                                            {
                                                if (const auto* Snap = VM->GetCurrentEntitySnapshot())
                                                {
                                                    for (const auto& As : Snap->ActionSets)
                                                    {
                                                        if (const auto* A = As.Catalog.FindByPredicate(
                                                            [&](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == ActionHandle; }))
                                                        {
                                                            return FSlateColor(ResolveActionStatusColor(*A));
                                                        }
                                                    }
                                                }
                                            }
                                            return FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted);
                                        })
                                        .Padding(FMargin(0.0f))
                                        [
                                            SNew(SSpacer)
                                        ]
                                ]
                        ]

                    // Class name
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        .Padding(2.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(InItem->ActionClassName))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                        ]

                    // Tag (monospace, muted)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(InItem->ActionTagText))
                                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
                        ]

                    // Role tag
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(6.0f, 0.0f, 4.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(RoleLabel(InItem->Role)))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                        ]
            ];
    }
}

auto
    SCkGoapDebugger_Sidebar::
    GetTreeChildren(
        TSharedPtr<FNode> InItem,
        TArray<TSharedPtr<FNode>>& OutChildren)
    -> void
{
    if (NOT InItem.IsValid()) { return; }
    OutChildren = InItem->Children;
}

auto
    SCkGoapDebugger_Sidebar::
    OnSelectionChanged(
        TSharedPtr<FNode> InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (_SuppressSelectionEcho) { return; }
    if (NOT _ViewModel.IsValid() || NOT InItem.IsValid()) { return; }

    if (InItem->Kind == ENodeKind::ActionSet)
    {
        _ViewModel->SetSelectedActionSet(InItem->ActionSetHandle);
    }
    else
    {
        // Selecting an Action also selects its owning ActionSet so panels
        // downstream don't lose context.
        // The Sidebar tree's nodes don't directly know their parent — but the
        // ViewModel's per-snapshot lookup is fine: walk the snapshot to find
        // which ActionSet contains this Action.
        if (const auto* Snap = _ViewModel->GetCurrentEntitySnapshot())
        {
            for (const auto& As : Snap->ActionSets)
            {
                const auto* Match = As.Catalog.FindByPredicate(
                    [&](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == InItem->ActionHandle; });
                if (Match != nullptr)
                {
                    _ViewModel->SetSelectedActionSet(As.Handle);
                    break;
                }
            }
        }

        _ViewModel->SetSelectedAction(InItem->ActionHandle);
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
        if (_HistoryItems.Num() == 0) { return; }
        _HistoryItems.Empty();
        if (_HistoryListView.IsValid()) { _HistoryListView->RequestListRefresh(); }
        return;
    }

    const auto Entity = _ViewModel->GetSelectedEntity();
    if (NOT ck::IsValid(Entity))
    {
        if (_HistoryItems.Num() == 0) { return; }
        _HistoryItems.Empty();
        if (_HistoryListView.IsValid()) { _HistoryListView->RequestListRefresh(); }
        return;
    }

    const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(Entity);

    // Hash on (count, last frame) — coarse but adequate for D2.
    auto NewHash = uint32{0};
    NewHash = HashCombine(NewHash, ::GetTypeHash(Hist.Num()));
    if (Hist.Num() > 0) { NewHash = HashCombine(NewHash, ::GetTypeHash(Hist.Last().FrameNumber)); }

    if (NewHash == _LastHistoryHash) { return; }
    _LastHistoryHash = NewHash;

    // Show most-recent first. Cap at a sensible number for the bottom panel.
    static constexpr auto MaxRowsToShow = 200;

    _HistoryItems.Empty();
    const auto Start = FMath::Max(0, Hist.Num() - MaxRowsToShow);
    for (auto Idx = Hist.Num() - 1; Idx >= Start; --Idx)
    { _HistoryItems.Add(MakeShared<FCkGoapDebugger_HistoryEvent>(Hist[Idx])); }

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

    const auto DotColor = HistoryEventColor(InItem->Kind);
    const auto TitleText = InItem->Title.IsEmpty()
        ? HistoryKindLabel(InItem->Kind)
        : InItem->Title;

    return SNew(FRowType, InOwnerTable)
        .Padding(FMargin(2.0f, 1.0f))
        .ShowSelection(false)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4.0f, 0.0f)
                    [
                        MakeStatusDot(DotColor, 8.0f)
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
                            .Text(FText::FromString(FormatTimestamp(InItem->WorldTimeSeconds)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
                    ]
        ];
}

// ====================================================================================================================
