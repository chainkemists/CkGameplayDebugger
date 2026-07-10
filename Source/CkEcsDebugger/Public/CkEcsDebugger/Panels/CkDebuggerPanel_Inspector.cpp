#include "CkDebuggerPanel_Inspector.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_SearchBar.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SNullWidget.h"

#include "CkEditorTools/Style/CkStyle.h"
SCkDebuggerPanel_Inspector::~SCkDebuggerPanel_Inspector()
{
    DeactivateAllInspectors();
}

auto SCkDebuggerPanel_Inspector::Get_CurrentInspectedEntity() const -> FCk_Handle
{
    if (SelectionModel.IsValid())
    {
        return SelectionModel->Get_PrimarySelection();
    }
    return FCk_Handle{};
}

auto SCkDebuggerPanel_Inspector::DeactivateAllInspectors() -> void
{
    for (const auto& Inspector : Inspectors)
    {
        if (Inspector.IsValid())
        {
            Inspector->OnDeactivated();
        }
    }
}

// ============================================================================
// Construct
// ============================================================================

auto SCkDebuggerPanel_Inspector::Construct(const FArguments& InArgs, TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel) -> void
{
    SelectionModel = InSelectionModel;

    RegisterDefaultInspectors();

    if (SelectionModel.IsValid())
    {
        SelectionModel->OnSelectionChanged.AddSP(this, &SCkDebuggerPanel_Inspector::OnSelectionChanged);
    }

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
        .Padding(0.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(_ModeToggleContainer, SBox)
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(ScrollBox, SScrollBox)
                .Orientation(Orient_Vertical)
                .ScrollBarAlwaysVisible(true)
                .ScrollBarVisibility(EVisibility::Visible)
            ]
        ]
    ];

    RebuildInspectors();
}

// ============================================================================
// Tick
// ============================================================================

auto SCkDebuggerPanel_Inspector::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    // Honour the user's refresh-mode + rate-cap settings for the ECS debugger.
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(SCkDebuggerWindow_Main::WindowId))
    { return; }

    // POLICY — NEVER rebuild widget structure from Tick.
    //
    // Widget trees are built exactly once per (entity selection × inspector)
    // pair. Dynamic values flow via TAttribute<FText> bindings on the rows
    // produced by FCkInspectorWidgetBuilder; those bindings re-evaluate every
    // paint and pick up live data with zero structural churn. Inspectors that
    // need per-tick updates MUST mutate in-place (e.g. FCkInspector_InteractTarget
    // updates its existing badge boxes via PopulateBadgeBox — no SetContent).
    //
    // Inspectors retain RequestRebuild() as a marker for genuine structural
    // changes (new inventory item, new interaction target). We intentionally
    // DO NOT rebuild the panel in response — any structural mutation of a
    // Slate widget tree during the Tick phase causes a one-frame "scrunch"
    // where the new sub-tree hasn't been prepassed yet and parent AutoHeight
    // slots collapse to zero. See the Gallery's "Rebuild Storm" section for
    // a live A/B repro — only the Data-only strategy (TAttribute bindings,
    // no structural rebuild) is scrunch-free.
    //
    // Structural refresh paths that ARE permitted:
    //   - Entity re-selection (OnSelectionChanged → RebuildInspectors)
    //   - Explicit user "Refresh" action (TODO: add toolbar button)
    //   - Per-inspector in-place mutation of its own stable sub-containers

    for (const auto& Entity : _CurrentInspectedEntities)
    {
        if (ck::Is_NOT_Valid(Entity)) { continue; }

        for (const auto& Inspector : Inspectors)
        {
            if (NOT Inspector.IsValid()) { continue; }
            if (NOT Inspector->CanInspect(Entity)) { continue; }

            Inspector->Tick(Entity, InDeltaTime);

            // Clear the flag so stale rebuild requests don't accumulate.
            // This is a no-op data-wise; the panel simply does not rebuild.
            if (Inspector->NeedsRebuild())
            { Inspector->ClearRebuildFlag(); }
        }
    }
}

// ============================================================================
// RebuildInspectors
// ============================================================================

auto SCkDebuggerPanel_Inspector::RebuildInspectors() -> void
{
    if (NOT ScrollBox.IsValid())
    { return; }

    DeactivateAllInspectors();

    ScrollBox->ClearChildren();
    _InspectorContentContainers.Empty();

    // Hide mode toggle by default
    if (_ModeToggleContainer.IsValid())
    {
        _ModeToggleContainer->SetContent(SNullWidget::NullWidget);
    }

    if (NOT SelectionModel.IsValid())
    {
        _CurrentInspectedEntities.Empty();
        ScrollBox->AddSlot()
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                Build_NoSelectionWidget()
            ];
        return;
    }

    const auto& SelectedEntities = SelectionModel->Get_SelectedEntities();

    if (SelectedEntities.Num() == 0)
    {
        _CurrentInspectedEntities.Empty();
        ScrollBox->AddSlot()
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                Build_NoSelectionWidget()
            ];
        return;
    }

    if (SelectedEntities.Num() > 1)
    {
        _CurrentInspectedEntities = SelectedEntities;

        if (_ModeToggleContainer.IsValid())
        {
            _ModeToggleContainer->SetContent(Build_ModeToggle());
        }

        auto ContentWidget = (_DisplayMode == ECkInspectorDisplayMode::GroupByInspector)
            ? Build_MultiEntityInspector_GroupByInspector(SelectedEntities)
            : Build_MultiEntityInspector_GroupByEntity(SelectedEntities);

        ScrollBox->AddSlot()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                ContentWidget
            ];
        return;
    }

    const auto& Entity = SelectedEntities[0];
    _CurrentInspectedEntities = { Entity };

    if (ck::Is_NOT_Valid(Entity))
    {
        ScrollBox->AddSlot()
            .HAlign(HAlign_Left)
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                SNew(SCkDebug_StatusPill)
                .Text(FText::FromString(TEXT("Invalid Entity")))
                .Tone(ECk_Tone::Err)
            ];
        return;
    }

    ScrollBox->AddSlot()
        .Padding(FCkDebuggerStyle::Padding_Small)
        [
            Build_SingleEntityInspector(Entity)
        ];
}

// ============================================================================
// Build_NoSelectionWidget
// ============================================================================

auto SCkDebuggerPanel_Inspector::Build_NoSelectionWidget() -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(FCkDebuggerStyle::Padding_Large)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.LargeHeader"))
                .Text(FText::FromString(TEXT("No Entity Selected")))
                .ColorAndOpacity(CkStyle::TextMute())
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Text(FText::FromString(TEXT("Select an entity from the list to inspect")))
                .ColorAndOpacity(CkStyle::TextDim())
            ]
        ];
}

// ============================================================================
// Build_ModeToggle
// ============================================================================

auto SCkDebuggerPanel_Inspector::Build_ModeToggle() -> TSharedRef<SWidget>
{
    const auto bInspectorActive = (_DisplayMode == ECkInspectorDisplayMode::GroupByInspector);
    const auto bEntityActive = (_DisplayMode == ECkInspectorDisplayMode::GroupByEntity);

    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Medium"))
        .Padding(FMargin(FCkDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SBorder)
                .BorderImage(bInspectorActive
                    ? FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border")
                    : FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
                .Padding(FMargin(2.0f))
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "Button")
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        OnDisplayModeChanged(ECkInspectorDisplayMode::GroupByInspector);
                        return FReply::Handled();
                    })
                    .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small))
                    .HAlign(HAlign_Center)
                    [
                        SNew(STextBlock)
                        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                        .Text(FText::FromString(TEXT("By Inspector")))
                        .ColorAndOpacity(bInspectorActive
                            ? CkStyle::Selection()
                            : CkStyle::Text())
                        .Justification(ETextJustify::Center)
                    ]
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                SNullWidget::NullWidget
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SBorder)
                .BorderImage(bEntityActive
                    ? FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border")
                    : FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
                .Padding(FMargin(2.0f))
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "Button")
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        OnDisplayModeChanged(ECkInspectorDisplayMode::GroupByEntity);
                        return FReply::Handled();
                    })
                    .ContentPadding(FMargin(FCkDebuggerStyle::Padding_Small))
                    .HAlign(HAlign_Center)
                    [
                        SNew(STextBlock)
                        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                        .Text(FText::FromString(TEXT("By Entity")))
                        .ColorAndOpacity(bEntityActive
                            ? CkStyle::Selection()
                            : CkStyle::Text())
                        .Justification(ETextJustify::Center)
                    ]
                ]
            ]
        ];
}

// ============================================================================
// OnDisplayModeChanged
// ============================================================================

auto SCkDebuggerPanel_Inspector::OnDisplayModeChanged(ECkInspectorDisplayMode NewMode) -> void
{
    if (_DisplayMode == NewMode) { return; }
    _DisplayMode = NewMode;
    RebuildInspectors();
}

// ============================================================================
// Format_EntityDisplayName
// ============================================================================

auto SCkDebuggerPanel_Inspector::Format_EntityDisplayName(const FCk_Handle& Entity) const -> FText
{
    if (ck::Is_NOT_Valid(Entity))
    { return FText::FromString(TEXT("Invalid Entity")); }

    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
    const auto EntityId = Entity.Get_Entity().Get_ID();
    return FText::FromString(ck::Format_UE(TEXT("{} [{}]"), DebugName, EntityId));
}

// ============================================================================
// Build_SingleEntityInspector (unchanged logic)
// ============================================================================

auto SCkDebuggerPanel_Inspector::Build_SingleEntityInspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto VerticalBox = SNew(SVerticalBox);

    auto FirstSection = true;

    auto AddSeparator = [&]()
    {
        if (NOT FirstSection)
        {
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SSeparator)
                    .Orientation(Orient_Horizontal)
                    .SeparatorImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Separator"))
                    .Thickness(1.0f)
                ];
        }
        FirstSection = false;
    };

    auto AddSection = [&](const FText& InName, const TSharedRef<SWidget>& InContent)
    {
        VerticalBox->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebug_InspectorPanel)
                .Title(InName)
                .Body()
                [
                    SNew(SBox)
                    .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
                    [
                        InContent
                    ]
                ]
            ];
    };

    for (int32 Index = 0; Index < Inspectors.Num(); ++Index)
    {
        const auto& Inspector = Inspectors[Index];

        if (NOT Inspector.IsValid())
        { continue; }

        if (NOT Inspector->CanInspect(Entity))
        { continue; }

        if (Inspector->IsMultiSection())
        {
            for (const auto& Section : Inspector->Get_InspectorSections(Entity))
            {
                AddSeparator();
                AddSection(Section.Name, Section.Widget);
            }
        }
        else
        {
            AddSeparator();
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    Build_InspectorSection(Entity, Inspector, Index)
                ];
        }
    }

    return VerticalBox;
}

// ============================================================================
// Build_InspectorSection (single-entity, unchanged logic)
// ============================================================================

auto SCkDebuggerPanel_Inspector::Build_InspectorSection(
    const FCk_Handle& Entity,
    const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector,
    int32 InspectorIndex) -> TSharedRef<SWidget>
{
    const auto Filter = InspectorFilters.FindRef(InspectorIndex);

    auto BodyContent = SNew(SVerticalBox);

    if (Inspector->IsFilterable())
    {
        BodyContent->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebuggerWidget_SearchBar)
                .OnSearchTextChanged_Lambda([this, InspectorIndex](const FString& InText)
                {
                    OnInspectorFilterChanged(InspectorIndex, InText);
                })
            ];
    }

    TSharedPtr<SBox> ContentContainer;

    BodyContent->AddSlot()
        .AutoHeight()
        [
            SAssignNew(ContentContainer, SBox)
            .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
            [
                Inspector->IsFilterable()
                    ? Inspector->Build_Inspector(Entity, Filter)
                    : Inspector->Build_Inspector(Entity)
            ]
        ];

    _InspectorContentContainers.Add(TPair<int32, int32>(InspectorIndex, 0), ContentContainer);

    return SNew(SCkDebug_InspectorPanel)
        .Title(Inspector->Get_ComponentName())
        .Body()
        [
            BodyContent
        ];
}

// ============================================================================
// Build_MultiEntityInspector_GroupByInspector
// ============================================================================

auto SCkDebuggerPanel_Inspector::Build_MultiEntityInspector_GroupByInspector(
    const TArray<FCk_Handle>& Entities) -> TSharedRef<SWidget>
{
    auto VerticalBox = SNew(SVerticalBox);
    auto FirstSection = true;

    for (int32 InspectorIdx = 0; InspectorIdx < Inspectors.Num(); ++InspectorIdx)
    {
        const auto& Inspector = Inspectors[InspectorIdx];
        if (NOT Inspector.IsValid()) { continue; }

        // Collect entities this inspector can handle
        TArray<FCk_Handle> ApplicableEntities;
        for (const auto& Entity : Entities)
        {
            if (ck::IsValid(Entity) && Inspector->CanInspect(Entity))
            {
                ApplicableEntities.Add(Entity);
            }
        }

        if (ApplicableEntities.Num() == 0) { continue; }

        // Separator between inspector groups
        if (NOT FirstSection)
        {
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SSeparator)
                    .Orientation(Orient_Horizontal)
                    .SeparatorImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Separator"))
                    .Thickness(1.0f)
                ];
        }
        FirstSection = false;

        // Build inner content: optional shared search bar + entity sub-sections
        auto InnerBox = SNew(SVerticalBox);

        // For filterable inspectors, place ONE search bar at the outer level
        if (Inspector->IsFilterable())
        {
            InnerBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SCkDebuggerWidget_SearchBar)
                    .OnSearchTextChanged_Lambda([this, InspectorIdx](const FString& InText)
                    {
                        OnInspectorFilterChanged(InspectorIdx, InText);
                    })
                ];
        }

        for (int32 EntityIdx = 0; EntityIdx < ApplicableEntities.Num(); ++EntityIdx)
        {
            const auto& Entity = ApplicableEntities[EntityIdx];
            InnerBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    Build_EntitySubSection(Entity, Inspector, InspectorIdx, EntityIdx)
                ];
        }

        // Top-level expandable for the inspector
        VerticalBox->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebug_InspectorPanel)
                .Title(Inspector->Get_ComponentName())
                .CountText(FText::FromString(ck::Format_UE(TEXT("{}"), ApplicableEntities.Num())))
                .Body()
                [
                    InnerBox
                ]
            ];
    }

    return VerticalBox;
}

// ============================================================================
// Build_MultiEntityInspector_GroupByEntity
// ============================================================================

auto SCkDebuggerPanel_Inspector::Build_MultiEntityInspector_GroupByEntity(
    const TArray<FCk_Handle>& Entities) -> TSharedRef<SWidget>
{
    auto VerticalBox = SNew(SVerticalBox);
    auto FirstSection = true;

    for (int32 EntityIdx = 0; EntityIdx < Entities.Num(); ++EntityIdx)
    {
        const auto& Entity = Entities[EntityIdx];
        if (ck::Is_NOT_Valid(Entity)) { continue; }

        auto InnerBox = SNew(SVerticalBox);
        auto HasAnyInspector = false;

        for (int32 InspectorIdx = 0; InspectorIdx < Inspectors.Num(); ++InspectorIdx)
        {
            const auto& Inspector = Inspectors[InspectorIdx];
            if (NOT Inspector.IsValid()) { continue; }
            if (NOT Inspector->CanInspect(Entity)) { continue; }

            HasAnyInspector = true;

            InnerBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    Build_EntitySubSection(Entity, Inspector, EntityIdx, InspectorIdx)
                ];
        }

        if (NOT HasAnyInspector) { continue; }

        if (NOT FirstSection)
        {
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SSeparator)
                    .Orientation(Orient_Horizontal)
                    .SeparatorImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Separator"))
                    .Thickness(1.0f)
                ];
        }
        FirstSection = false;

        VerticalBox->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebug_InspectorPanel)
                .Title(Format_EntityDisplayName(Entity))
                .Body()
                [
                    InnerBox
                ]
            ];
    }

    return VerticalBox;
}

// ============================================================================
// Build_EntitySubSection
// ============================================================================

auto SCkDebuggerPanel_Inspector::Build_EntitySubSection(
    const FCk_Handle& Entity,
    const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector,
    int32 OuterIndex,
    int32 InnerIndex) -> TSharedRef<SWidget>
{
    // Header text depends on mode:
    // GroupByInspector: outer is inspector, inner is entity → show entity name
    // GroupByEntity: outer is entity, inner is inspector → show inspector name
    const auto HeaderText = (_DisplayMode == ECkInspectorDisplayMode::GroupByInspector)
        ? Format_EntityDisplayName(Entity)
        : Inspector->Get_ComponentName();

    const auto InspectorIndex = (_DisplayMode == ECkInspectorDisplayMode::GroupByInspector)
        ? OuterIndex : InnerIndex;

    const auto Filter = InspectorFilters.FindRef(InspectorIndex);

    auto BodyContent = SNew(SVerticalBox);

    // In GroupByEntity mode, filterable inspectors get their own search bar
    if (Inspector->IsFilterable() && _DisplayMode == ECkInspectorDisplayMode::GroupByEntity)
    {
        BodyContent->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebuggerWidget_SearchBar)
                .OnSearchTextChanged_Lambda([this, InspectorIndex](const FString& InText)
                {
                    OnInspectorFilterChanged(InspectorIndex, InText);
                })
            ];
    }

    // Handle multi-section inspectors
    if (Inspector->IsMultiSection())
    {
        for (const auto& Section : Inspector->Get_InspectorSections(Entity))
        {
            BodyContent->AddSlot()
                .AutoHeight()
                .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small)
                    [
                        SNew(SCkDebug_SectionHeader)
                        .Label(Section.Name)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        Section.Widget
                    ]
                ];
        }
    }
    else
    {
        TSharedPtr<SBox> ContentContainer;

        BodyContent->AddSlot()
            .AutoHeight()
            [
                SAssignNew(ContentContainer, SBox)
                .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
                [
                    Inspector->IsFilterable()
                        ? Inspector->Build_Inspector(Entity, Filter)
                        : Inspector->Build_Inspector(Entity)
                ]
            ];

        _InspectorContentContainers.Add(TPair<int32, int32>(OuterIndex, InnerIndex), ContentContainer);
    }

    return SNew(SCkDebug_InspectorPanel)
        .Title(HeaderText)
        .Body()
        [
            BodyContent
        ];
}

// ============================================================================
// RegisterDefaultInspectors
// ============================================================================

auto SCkDebuggerPanel_Inspector::RegisterDefaultInspectors() -> void
{
    Inspectors = FCkDebuggerInspectorRegistry::Get().CreateAll();

    for (const auto& Inspector : Inspectors)
    {
        if (Inspector.IsValid())
        {
            Inspector->Set_SelectionModel(SelectionModel);
        }
    }
}

// ============================================================================
// OnSelectionChanged
// ============================================================================

auto SCkDebuggerPanel_Inspector::OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void
{
    RebuildInspectors();
}

// ============================================================================
// OnInspectorFilterChanged
// ============================================================================

auto SCkDebuggerPanel_Inspector::OnInspectorFilterChanged(int32 InspectorIndex, const FString& InFilterText) -> void
{
    InspectorFilters.Add(InspectorIndex, InFilterText);

    // Multi-entity: full rebuild to apply filter across all sub-sections
    if (_CurrentInspectedEntities.Num() > 1)
    {
        RebuildInspectors();
        return;
    }

    // Single entity: granular update
    if (_CurrentInspectedEntities.Num() == 0)
    { return; }

    const auto& Entity = _CurrentInspectedEntities[0];
    if (ck::Is_NOT_Valid(Entity))
    { return; }

    if (NOT Inspectors.IsValidIndex(InspectorIndex))
    { return; }

    const auto& Inspector = Inspectors[InspectorIndex];
    if (NOT Inspector.IsValid() || NOT Inspector->IsFilterable())
    { return; }

    const auto ContainerKey = TPair<int32, int32>(InspectorIndex, 0);
    if (const auto Container = _InspectorContentContainers.Find(ContainerKey))
    {
        if (Container->IsValid())
        {
            (*Container)->SetContent(
                SNew(SBox)
                .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
                [
                    Inspector->Build_Inspector(Entity, InFilterText)
                ]
            );
        }
    }
}
