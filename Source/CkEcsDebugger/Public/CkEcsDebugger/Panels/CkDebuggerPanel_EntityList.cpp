#include "CkDebuggerPanel_EntityList.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_FeatureVisuals.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_EntityTree.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"

SCkDebuggerPanel_EntityList::~SCkDebuggerPanel_EntityList()
{
    if (PinsChangedHandle.IsValid() && EntityTree.IsValid())
    {
        EntityTree->OnPinsChanged.Remove(PinsChangedHandle);
        PinsChangedHandle.Reset();
    }
}

auto SCkDebuggerPanel_EntityList::Construct(
    const FArguments& InArgs,
    TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
    TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel,
    TSharedPtr<FCkDebuggerModel_InspectorFilter> InFilterModel) -> void
{
    SelectionModel = InSelectionModel;
    WorldModel = InWorldModel;

    SetClipping(EWidgetClipping::ClipToBounds);

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
        .Padding(0.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebug_WorldSelector, WorldModel->Get_SelectorModel())
                .ShowHeaderLabel(true)
                .OnWorldChanged(this, &SCkDebuggerPanel_EntityList::OnWorldSelectionChanged)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                Build_Toolbar()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(SearchBar, SCkDebug_DualSearchBar)
                    .OnFilterTextChanged(this, &SCkDebuggerPanel_EntityList::OnFilterTextChanged)
                    .OnHighlightTextChanged(this, &SCkDebuggerPanel_EntityList::OnHighlightTextChanged)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
                [
                    Build_QueryHelpButton()
                ]
            ]

            // Pinned + Recent quick-access sections (Phase 3) — hidden while empty.
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SAssignNew(PinnedSectionBox, SVerticalBox)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SAssignNew(RecentSectionBox, SVerticalBox)
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(SHorizontalBox)

                // Feature rail (Phase 3): one chip per flagged feature; click narrows
                // the tree to entities carrying it (own or rolled up).
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    Build_FeatureRail()
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border"))
                    .Padding(0.0f)
                    [
                        SAssignNew(EntityTree, SCkDebuggerWidget_EntityTree, SelectionModel, WorldModel, InFilterModel)
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                Build_StatusBar()
            ]
        ]
    ];

    // Quick-access wiring: pins change → rebuild sections; selection change → track
    // recents. AddSP self-cleans with this panel; the pins handle is removed in ~.
    PinsChangedHandle = EntityTree->OnPinsChanged.AddLambda(
        [WeakSelf = TWeakPtr<SCkDebuggerPanel_EntityList>(SharedThis(this))]()
        {
            if (const auto Pinned = WeakSelf.Pin())
            { Pinned->RefreshQuickAccessSections(); }
        });

    if (SelectionModel.IsValid())
    {
        SelectionModel->OnSelectionChanged.AddSP(this, &SCkDebuggerPanel_EntityList::HandleSelectionChanged);
    }
}

auto SCkDebuggerPanel_EntityList::HandleSelectionChanged(const TArray<FCk_Handle>& InSelection) -> void
{
    if (InSelection.IsEmpty() || ck::Is_NOT_Valid(InSelection[0]))
    { return; }

    constexpr auto MaxRecent = 5;

    RecentEntities.Remove(InSelection[0]);
    RecentEntities.Insert(InSelection[0], 0);
    if (RecentEntities.Num() > MaxRecent)
    { RecentEntities.SetNum(MaxRecent); }

    RefreshQuickAccessSections();
}

auto SCkDebuggerPanel_EntityList::Build_QueryHelpButton() -> TSharedRef<SWidget>
{
    const auto HelpText =
        TEXT("Query grammar (terms AND-compose):\n")
        TEXT("  has:<feature>   entity or its internals carry the feature (e.g. has:timer)\n")
        TEXT("  is:<feature>    entity itself carries it (e.g. is:probe)\n")
        TEXT("  is:primary      only primary entities\n")
        TEXT("  is:aux          only internal (folded) entities\n")
        TEXT("  net:<auth|proxy|none>\n")
        TEXT("  id:<n>          exact entity id\n")
        TEXT("  arch:<substr>   archetype name contains\n")
        TEXT("  <text>          fuzzy name match\n")
        TEXT("\nFilter hides non-matches; Highlight dims them.");

    return SNew(SButton)
        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
        .ToolTipText(FText::FromString(HelpText))
        .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small))
        [
            SNew(STextBlock)
            .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
            .Text(FText::FromString(TEXT("?")))
            .ColorAndOpacity(CkStyle::TextDim())
        ];
}

auto SCkDebuggerPanel_EntityList::Build_FeatureRail() -> TSharedRef<SWidget>
{
    auto Rail = SNew(SVerticalBox);

    for (const auto& [FeatureId, Bit] : ck::ecs_debugger_feature_visuals::Get_BadgeFeatures())
    {
        const auto* Visual = ck::ecs_debugger_feature_visuals::Get_FeatureVisuals().Find(FeatureId);
        const auto* Brush = Visual != nullptr ? FCkDebuggerStyle::Get_IconBrush(Visual->IconName) : nullptr;
        if (Brush == nullptr)
        { continue; }

        const auto Color = Visual->Color;
        const auto CapturedId = FeatureId;

        Rail->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 2.0f)
        [
            SNew(SCheckBox)
            .Style(FAppStyle::Get(), "ToggleButton")
            .ToolTipText(FText::FromString(FString::Printf(TEXT("Show only entities with %s (own or rolled up). Click again to release."), *FeatureId.ToString())))
            .IsChecked_Lambda([this, CapturedId]()
            {
                return EntityTree.IsValid() && EntityTree->Get_RailIncluded().Contains(CapturedId)
                    ? ECheckBoxState::Checked
                    : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([this, CapturedId](ECheckBoxState)
            {
                if (EntityTree.IsValid())
                { EntityTree->Toggle_RailFeature(CapturedId); }
            })
            [
                SNew(SImage)
                .Image(Brush)
                .ColorAndOpacity(Color)
                .DesiredSizeOverride(FVector2D(14.0f, 14.0f))
            ]
        ];
    }

    return Rail;
}

auto SCkDebuggerPanel_EntityList::RefreshQuickAccessSections() -> void
{
    if (NOT PinnedSectionBox.IsValid() || NOT RecentSectionBox.IsValid())
    { return; }

    PinnedSectionBox->ClearChildren();
    RecentSectionBox->ClearChildren();

    // Prune dead handles (PIE end) as we render.
    RecentEntities.RemoveAll([](const FCk_Handle& InEntity) { return ck::Is_NOT_Valid(InEntity); });

    const auto BuildSection = [this](
        const TCHAR* InTitle,
        const TArray<FCk_Handle>& InEntities,
        const TSharedPtr<SVerticalBox>& InContainer) -> void
    {
        auto ValidEntities = TArray<FCk_Handle>{};
        for (const auto& Entity : InEntities)
        {
            if (ck::IsValid(Entity))
            { ValidEntities.Add(Entity); }
        }

        if (ValidEntities.IsEmpty())
        { return; }

        InContainer->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
            .Text(FText::FromString(FString::Printf(TEXT("%s (%d)"), InTitle, ValidEntities.Num())))
            .ColorAndOpacity(CkStyle::TextDim())
        ];

        DoBuildQuickAccessRows(ValidEntities, InContainer);
    };

    BuildSection(TEXT("Pinned"), EntityTree.IsValid() ? EntityTree->Get_PinnedEntities() : TArray<FCk_Handle>{}, PinnedSectionBox);
    BuildSection(TEXT("Recent"), RecentEntities, RecentSectionBox);
}

auto SCkDebuggerPanel_EntityList::DoBuildQuickAccessRows(
    const TArray<FCk_Handle>& InEntities,
    const TSharedPtr<SVerticalBox>& InContainer) -> void
{
    for (const auto& Entity : InEntities)
    {
        const auto CleanName = ck::DebugNameClean::Get_CleanName(
            UCk_Utils_Handle_UE::Get_DebugName(Entity).ToString());

        InContainer->AddSlot()
        .AutoHeight()
        .Padding(FCkDebuggerStyle::Padding_Small, 1.0f, 0.0f, 1.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ToolTipText(FText::FromString(TEXT("Select in the tree")))
                .ContentPadding(FMargin(2.0f, 0.0f))
                .OnClicked_Lambda([this, Entity]() -> FReply
                {
                    if (SelectionModel.IsValid() && ck::IsValid(Entity))
                    { SelectionModel->Set_SelectedEntities({ Entity }); }
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                    .Text(FText::FromString(CleanName))
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                SNew(SCkDebug_EntityRef)
                .Entity_Lambda([Entity]() { return Entity; })
            ]
        ];
    }
}

auto SCkDebuggerPanel_EntityList::OnWorldSelectionChanged() -> void
{
    // World switched (or auto-selected) — the previously-selected entities belong
    // to the old world and are stale, so clear them and refresh the tree.
    if (SelectionModel.IsValid())
    {
        SelectionModel->Clear_Selection();
    }

    if (EntityTree.IsValid())
    {
        EntityTree->RefreshTree();
    }
}

auto SCkDebuggerPanel_EntityList::Build_Toolbar() -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .Padding(FMargin(0.0f))
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
            .Padding(FMargin(FCkDebuggerStyle::Padding_Small))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .OnClicked(this, &SCkDebuggerPanel_EntityList::OnRefreshClicked)
                    .ToolTipText(FText::FromString(TEXT("Refresh entity list")))
                    .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small))
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.Refresh"))
                        .ColorAndOpacity(CkStyle::TextDim())
                        .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .OnClicked(this, &SCkDebuggerPanel_EntityList::OnExpandAllClicked)
                    .ToolTipText(FText::FromString(TEXT("Expand all nodes")))
                    .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small))
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.ChevronDown"))
                        .ColorAndOpacity(CkStyle::TextDim())
                        .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .OnClicked(this, &SCkDebuggerPanel_EntityList::OnCollapseAllClicked)
                    .ToolTipText(FText::FromString(TEXT("Collapse all nodes")))
                    .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small))
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.ChevronUp"))
                        .ColorAndOpacity(CkStyle::TextDim())
                        .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                    ]
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SBox)
                ]

                // Presentation toggles (Phase 2): fold internals + group siblings.
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(SCheckBox)
                    .Style(FAppStyle::Get(), "ToggleButton")
                    .ToolTipText(FText::FromString(TEXT("Fold internal entities (timers, scene nodes, attributes ...) under their owner with a +N chip")))
                    .IsChecked_Lambda([this]() { return EntityTree.IsValid() && EntityTree->Get_FoldInternals() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                    {
                        if (EntityTree.IsValid())
                        { EntityTree->Set_FoldInternals(InState == ECheckBoxState::Checked); }
                    })
                    [
                        SNew(STextBlock)
                        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                        .Text(FText::FromString(TEXT("Fold")))
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SCheckBox)
                    .Style(FAppStyle::Get(), "ToggleButton")
                    .ToolTipText(FText::FromString(TEXT("Coalesce runs of same-archetype siblings into one \"Name xN\" row")))
                    .IsChecked_Lambda([this]() { return EntityTree.IsValid() && EntityTree->Get_GroupSiblings() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState InState)
                    {
                        if (EntityTree.IsValid())
                        { EntityTree->Set_GroupSiblings(InState == ECheckBoxState::Checked); }
                    })
                    [
                        SNew(STextBlock)
                        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                        .Text(FText::FromString(TEXT("Group")))
                    ]
                ]
            ]
        ];
}

auto SCkDebuggerPanel_EntityList::Build_StatusBar() -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .Padding(FMargin(0.0f))
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
            .Padding(FMargin(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                    .Text(this, &SCkDebuggerPanel_EntityList::Get_EntityCountText)
                    .ColorAndOpacity(CkStyle::TextDim())
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FCkDebuggerStyle::Padding_Large, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                    .Text(this, &SCkDebuggerPanel_EntityList::Get_SelectionCountText)
                    .ColorAndOpacity(CkStyle::Selection())
                ]
            ]
        ];
}

auto SCkDebuggerPanel_EntityList::OnFilterTextChanged(const FString& InText) -> void
{
    if (EntityTree.IsValid())
    {
        EntityTree->ApplyFilter(InText);
    }
}

auto SCkDebuggerPanel_EntityList::OnHighlightTextChanged(const FString& InText) -> void
{
    if (EntityTree.IsValid())
    {
        EntityTree->ApplyHighlight(InText);
    }
}

auto SCkDebuggerPanel_EntityList::OnRefreshClicked() -> FReply
{
    // Manual refresh is the escape hatch: full rebuild, re-deriving cached names —
    // the incremental path (live churn) reuses nodes and never re-reads names.
    if (EntityTree.IsValid())
    {
        EntityTree->ForceFullRefresh();
    }

    return FReply::Handled();
}

auto SCkDebuggerPanel_EntityList::OnExpandAllClicked() -> FReply
{
    if (EntityTree.IsValid())
    {
        EntityTree->ExpandAll();
    }

    return FReply::Handled();
}

auto SCkDebuggerPanel_EntityList::OnCollapseAllClicked() -> FReply
{
    if (EntityTree.IsValid())
    {
        EntityTree->CollapseAll();
    }

    return FReply::Handled();
}

auto SCkDebuggerPanel_EntityList::Get_EntityCountText() const -> FText
{
    if (NOT EntityTree.IsValid())
    { return FText::FromString(TEXT("Entities: 0")); }

    // Classification-aware counts (Phase 2): total plus the primary/internal split the
    // fold + rollup presentation is built on.
    const auto Counts = EntityTree->Get_Counts();
    return FText::FromString(ck::Format_UE(TEXT("Entities: {} ({} primary · {} internal)"),
        Counts.Total, Counts.Primaries, Counts.Internals));
}

auto SCkDebuggerPanel_EntityList::Get_SelectionCountText() const -> FText
{
    if (NOT SelectionModel.IsValid())
    { return FText::FromString(TEXT("Selected: 0")); }

    const auto SelectionCount = SelectionModel->Get_SelectionCount();
    return FText::FromString(ck::Format_UE(TEXT("Selected: {}"), SelectionCount));
}

