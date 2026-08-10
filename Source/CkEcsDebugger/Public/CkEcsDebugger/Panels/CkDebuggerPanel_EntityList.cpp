#include "CkDebuggerPanel_EntityList.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SNullWidget.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_FeatureVisuals.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_EntityTree.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"

#include "Styling/StyleDefaults.h"

// ====================================================================================================================
// Named, not anonymous: this module builds with unity on, and a merged TU collides file-local
// helpers by name.
// ====================================================================================================================

namespace ck_debugger_panel_entity_list
{
    // The rail group heading is deliberately below every CkStyle role (the rail column is only as
    // wide as its widest chip), so the size stays literal — inside ScaledFont, so it still follows
    // TextScale.
    auto Get_RailGroupFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Bold", 6); }

    auto Get_QuickAccessRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(
            FMargin{FCkDebuggerStyle::Padding_Small, 1.0f, 0.0f, 1.0f});
    }

    // RowBanding, split the way the axis defines it: Zebra owns the row's fill, Hairline owns a
    // rule under the row. Asking for the band brush under Hairline would paint the separator
    // across the whole row instead of along its edge.
    auto Get_RowBandFill(int32 InRowIndex) -> const FSlateBrush*
    {
        return UCkDebuggerStyleSettings::Get_Selection().RowBanding == ECkDebugAxis_RowBanding::Zebra
            ? ck::debug_axes::Get_RowBandingBrush(InRowIndex)
            : FStyleDefaults::GetNoBrush();
    }

    auto Make_RowRule() -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .HeightOverride_Lambda([]() -> FOptionalSize
            {
                return FOptionalSize{ck::debug_axes::Get_RowBandingRuleThickness()};
            })
            .Visibility_Lambda([]()
            {
                return ck::debug_axes::Get_RowBandingRuleThickness() > 0.0f
                    ? EVisibility::Visible
                    : EVisibility::Collapsed;
            })
            [
                SNew(SBorder)
                .BorderImage(CkStyle::GetFilledBrush())
                .BorderBackgroundColor(FSlateColor{CkStyle::Border()})
            ];
    }
}

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

            // Pinned quick-access section (Phase 3) — hidden while empty.
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SAssignNew(PinnedSectionBox, SVerticalBox)
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(SHorizontalBox)

                // Feature rails (Phase 3): one chip per flagged feature; click narrows
                // the tree to entities carrying it (own or rolled up). Split across
                // both flanks of the tree so the full set fits without scrolling.
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    Build_FeatureRail(/*InRightFlank=*/false)
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

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
                [
                    Build_FeatureRail(/*InRightFlank=*/true)
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

    // Quick-access wiring: pins change → rebuild the section. The pins handle is
    // removed in ~.
    PinsChangedHandle = EntityTree->OnPinsChanged.AddLambda(
        [WeakSelf = TWeakPtr<SCkDebuggerPanel_EntityList>(SharedThis(this))]()
        {
            if (const auto Pinned = WeakSelf.Pin())
            { Pinned->RefreshQuickAccessSections(); }
        });
}

auto SCkDebuggerPanel_EntityList::Set_FilterText(const FString& InText) -> void
{
    if (SearchBar.IsValid())
    {
        // Routes through the bar's OnFilterTextChanged → the tree's filter pipeline,
        // exactly as if typed.
        SearchBar->Set_FilterText(InText);
    }
}

auto SCkDebuggerPanel_EntityList::Get_FilterText() const -> FString
{
    return SearchBar.IsValid() ? SearchBar->Get_FilterText() : FString{};
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
        TEXT("  arch:<substr>   archetype name contains; repeat to OR several\n")
        TEXT("  <text>          fuzzy name match\n")
        TEXT("\nQuote multi-word values: arch:\"UnrealComponent: BackWall\".\n")
        TEXT("Filter hides non-matches; Highlight dims them.");

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

auto SCkDebuggerPanel_EntityList::Build_FeatureRail(bool InRightFlank) -> TSharedRef<SWidget>
{
    namespace visuals = ck::ecs_debugger_feature_visuals;

    // Chips clustered by feature group (Core/Attributes/AI/...), each cluster under a
    // tiny label. The flag set outgrew one column: groups are split across the tree's
    // two flanks — whole groups, greedily assigned to the lighter flank in display
    // order (weight = chips + header), so both columns come out near-equal and the
    // split is deterministic. Each flank scrolls independently when the window is short.
    auto GroupedFeatures = TMap<FName, TArray<FName>>{};
    for (const auto& [FeatureId, Bit] : visuals::Get_BadgeFeatures())
    { GroupedFeatures.FindOrAdd(visuals::Get_FeatureGroup(FeatureId)).Add(FeatureId); }

    auto FlankGroups = TArray<FName>{};
    {
        auto LeftRows  = 0;
        auto RightRows = 0;
        for (const auto& Group : visuals::Get_FeatureGroupOrder())
        {
            const auto* Features = GroupedFeatures.Find(Group);
            if (Features == nullptr || Features->IsEmpty())
            { continue; }

            const auto Weight       = Features->Num() + 1;
            const auto AssignRight  = LeftRows > RightRows;
            (AssignRight ? RightRows : LeftRows) += Weight;
            if (AssignRight == InRightFlank)
            { FlankGroups.Add(Group); }
        }
    }

    auto Rail = SNew(SVerticalBox);

    for (const auto& Group : FlankGroups)
    {
        const auto* Features = GroupedFeatures.Find(Group);
        if (Features == nullptr || Features->IsEmpty())
        { continue; }

        Rail->AddSlot()
        .AutoHeight()
        .Padding(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 2.0f)
        [
            SNew(STextBlock)
            .Font_Static(&ck_debugger_panel_entity_list::Get_RailGroupFont)
            .Text(FText::FromString(Group.ToString().ToUpper()))
            .ColorAndOpacity(CkStyle::TextMute())
        ];

        for (const auto& FeatureId : *Features)
        {
            const auto* Visual = visuals::Get_FeatureVisuals().Find(FeatureId);
            const auto* Brush = Visual != nullptr ? FCkDebuggerStyle::Get_IconBrush(Visual->IconName) : nullptr;
            if (Brush == nullptr)
            { continue; }

            const auto Color = Visual->Color;
            const auto CapturedId = FeatureId;

            // HAlign_Left: the rail column is AutoWidth (sized by the widest group label),
            // and SVerticalBox slots default to HAlign_Fill — without this the chip (and the
            // SVG inside) stretches to the label's width.
            Rail->AddSlot()
            .AutoHeight()
            .HAlign(HAlign_Left)
            .Padding(0.0f, 0.0f, 0.0f, 2.0f)
            [
                SNew(SCkDebug_ToggleSurface)
                .ToolTipText(FText::FromString(FString::Printf(TEXT("Show only entities with %s (own or rolled up). Click again to release."), *FeatureId.ToString())))
                .AccessibleText(FText::FromName(FeatureId))
                .IsOn_Lambda([this, CapturedId]() -> bool
                {
                    return EntityTree.IsValid() && EntityTree->Get_RailIncluded().Contains(CapturedId);
                })
                .OnStateChanged_Lambda([this, CapturedId](bool)
                {
                    if (EntityTree.IsValid())
                    { EntityTree->Toggle_RailFeature(CapturedId); }
                })
                [
                    SNew(SImage)
                    .Image(Brush)
                    .ColorAndOpacity(Color)
                    .DesiredSizeOverride_Lambda([]() -> TOptional<FVector2D>
                    {
                        const auto Size = ck::debug_axes::Apply_IconSize(14.0f);
                        return FVector2D{Size, Size};
                    })
                ]
            ];
        }
    }

    return SNew(SScrollBox)
        .ScrollBarVisibility(EVisibility::Collapsed)

        + SScrollBox::Slot()
        [
            Rail
        ];
}

auto SCkDebuggerPanel_EntityList::RefreshQuickAccessSections() -> void
{
    if (NOT PinnedSectionBox.IsValid())
    { return; }

    PinnedSectionBox->ClearChildren();

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

        // Separator-style header: label + rule line, with air above — the sections read
        // as distinct bands instead of two more rows of text.
        InContainer->AddSlot()
        .AutoHeight()
        .Padding(0.0f, FCkDebuggerStyle::Padding_Medium, 0.0f, 2.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                ck::debug_axes::Make_SectionHeader(
                    UCkDebuggerStyleSettings::Get_Selection(),
                    FText::FromString(ck::Format_UE(TEXT("{} ({})"), FString{InTitle}, ValidEntities.Num())),
                    ECk_Tone::Neutral)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .HeightOverride_Lambda([]() -> FOptionalSize
                {
                    return FOptionalSize{ck::debug_axes::Get_SeparatorThickness(
                        UCkDebuggerStyleSettings::Get_Selection())};
                })
                .Visibility_Lambda([]()
                {
                    return ck::debug_axes::Get_SeparatorThickness(
                        UCkDebuggerStyleSettings::Get_Selection()) > 0.0f
                        ? EVisibility::Visible
                        : EVisibility::Collapsed;
                })
                [
                    SNew(SImage)
                    .Image(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Separator"))
                    .ColorAndOpacity(CkStyle::Border())
                ]
            ]
        ];

        DoBuildQuickAccessRows(ValidEntities, InContainer);
    };

    BuildSection(TEXT("Pinned"), EntityTree.IsValid() ? EntityTree->Get_PinnedEntities() : TArray<FCk_Handle>{}, PinnedSectionBox);
}

auto SCkDebuggerPanel_EntityList::DoBuildQuickAccessRows(
    const TArray<FCk_Handle>& InEntities,
    const TSharedPtr<SVerticalBox>& InContainer) -> void
{
    auto RowIndex = 0;

    for (const auto& Entity : InEntities)
    {
        const auto CleanName = ck::DebugNameClean::Get_CleanName(
            UCk_Utils_Handle_UE::Get_DebugName(Entity).ToString());

        const auto Index = RowIndex++;

        InContainer->AddSlot()
        .AutoHeight()
        .Padding(TAttribute<FMargin>::CreateStatic(&ck_debugger_panel_entity_list::Get_QuickAccessRowPadding))
        [
            SNew(SVerticalBox)

            // RowBanding: Zebra paints an alternating full-bleed fill, Off paints nothing (a
            // no-brush border with zero padding contributes neither paint nor geometry, which is
            // what keeps Classic identical).
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage_Lambda([Index]() -> const FSlateBrush*
                {
                    return ck_debugger_panel_entity_list::Get_RowBandFill(Index);
                })
                .Padding(0.0f)
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
                ]
            ]

            // Hairline banding draws the rule along the row's bottom edge instead of filling it.
            // Zero thickness (Off / Zebra, or SeparatorWeight None) collapses the box, taking its
            // slot with it.
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_debugger_panel_entity_list::Make_RowRule()
            ]
        ];
    }
}

auto SCkDebuggerPanel_EntityList::Notify_StyleRevisionChanged() -> void
{
    RefreshQuickAccessSections();
}

auto SCkDebuggerPanel_EntityList::Reset_ForWorldChange() -> void
{
    if (EntityTree.IsValid())
    { EntityTree->Reset_ForWorldChange(); }

    RefreshQuickAccessSections();
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
                        .DesiredSizeOverride_Lambda([]() -> TOptional<FVector2D>
                        {
                            const auto Size = ck::debug_axes::Apply_IconSize(16.0f);
                            return FVector2D{Size, Size};
                        })
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
                        .DesiredSizeOverride_Lambda([]() -> TOptional<FVector2D>
                        {
                            const auto Size = ck::debug_axes::Apply_IconSize(16.0f);
                            return FVector2D{Size, Size};
                        })
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
                        .DesiredSizeOverride_Lambda([]() -> TOptional<FVector2D>
                        {
                            const auto Size = ck::debug_axes::Apply_IconSize(16.0f);
                            return FVector2D{Size, Size};
                        })
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
                    SNew(SCkDebug_IconToggle)
                    .IconId(TEXT("Package"))
                    .Label(FText::FromString(TEXT("Fold Internals")))
                    .ToolTip(FText::FromString(TEXT("Fold internal entities (timers, scene nodes, attributes ...) under their owner with a +N chip")))
                    .IsOn_Lambda([this]() -> bool
                    {
                        return EntityTree.IsValid() && EntityTree->Get_FoldInternals();
                    })
                    .OnStateChanged_Lambda([this](bool InIsOn)
                    {
                        if (EntityTree.IsValid())
                        { EntityTree->Set_FoldInternals(InIsOn); }
                    })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SCkDebug_IconToggle)
                    .IconId(TEXT("EntityCollection"))
                    .Label(FText::FromString(TEXT("Group Siblings")))
                    .ToolTip(FText::FromString(TEXT("Coalesce runs of same-archetype siblings into one \"Name xN\" row")))
                    .IsOn_Lambda([this]() -> bool
                    {
                        return EntityTree.IsValid() && EntityTree->Get_GroupSiblings();
                    })
                    .OnStateChanged_Lambda([this](bool InIsOn)
                    {
                        if (EntityTree.IsValid())
                        { EntityTree->Set_GroupSiblings(InIsOn); }
                    })
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

