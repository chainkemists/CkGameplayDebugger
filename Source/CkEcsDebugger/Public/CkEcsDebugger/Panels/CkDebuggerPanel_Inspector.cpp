#include "CkDebuggerPanel_Inspector.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_SearchBar.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityDebuggerLinks.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"

#include "Algo/Reverse.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SNullWidget.h"

#include "CkEditorTools/Style/CkStyle.h"

namespace ck_debugger_panel_inspector
{
    auto Should_TickInspector(bool InCanInspect, bool InWantsTickWhenNotInspectable) -> bool
    {
        return InCanInspect || InWantsTickWhenNotInspectable;
    }

    // Feature glyph + accent for an inspector's section header (the debugger-wide
    // icon language). Nullptr brush = inspector declared no glyph; header unchanged.
    static auto Get_InspectorIconBrush(const TSharedPtr<ICkDebuggerComponentInspector_Base>& InInspector) -> const FSlateBrush*
    {
        return FCkDebuggerStyle::Get_IconBrush(InInspector->Get_IconName());
    }

    static auto Get_InspectorIconColor(const TSharedPtr<ICkDebuggerComponentInspector_Base>& InInspector) -> FLinearColor
    {
        return InInspector->Get_FeatureColor().Get(CkStyle::TextDim());
    }
}

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

// Panel-level section filter. Matches on the inspector's component name — the same
// text that titles its section header — so typing "timer" leaves only the Timer
// section. Empty query matches everything.
auto SCkDebuggerPanel_Inspector::Matches_PanelFilter(
    const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector) const -> bool
{
    if (_PanelFilterString.IsEmpty())
    { return true; }

    if (NOT Inspector.IsValid())
    { return false; }

    return Inspector->Get_ComponentName().ToString().Contains(_PanelFilterString, ESearchCase::IgnoreCase);
}

// Highlight pass — dim rather than hide. Returns the section's RenderOpacity.
//
// NOTE: RenderOpacity is a plain float SLATE argument in this engine
// (SlateCore SWidget.h:1876 / DeclarativeSyntaxSupport.h:671), NOT a TAttribute,
// so it cannot be lambda-bound. That is fine here: every change to the highlight
// query runs RebuildInspectors, so a value computed at build time is always current.
auto SCkDebuggerPanel_Inspector::Get_PanelHighlightOpacity(
    const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector) const -> float
{
    constexpr auto FullOpacity = 1.0f;
    constexpr auto DimOpacity  = 0.35f;

    if (_PanelHighlightString.IsEmpty())
    { return FullOpacity; }

    if (NOT Inspector.IsValid())
    { return DimOpacity; }

    return Inspector->Get_ComponentName().ToString().Contains(_PanelHighlightString, ESearchCase::IgnoreCase)
        ? FullOpacity
        : DimOpacity;
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

    _EditGuard = MakeShared<FCkInspectorEditGuard>();

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
                SAssignNew(_BreadcrumbContainer, SBox)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(_ModeToggleContainer, SBox)
            ]

            // Panel-level section search. Sits ABOVE the sections scroll box and is
            // independent of the per-inspector search bars built inside each section.
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SAssignNew(_PanelSearchBar, SCkDebug_DualSearchBar)
                .FilterHintText(FText::FromString(TEXT("Filter sections\x2026")))
                .HighlightHintText(FText::FromString(TEXT("Highlight\x2026")))
                .OnFilterTextChanged_Lambda([this](const FString& InText)
                {
                    if (_PanelFilterString == InText) { return; }
                    _PanelFilterString = InText;
                    Request_RebuildInspectors();
                })
                .OnHighlightTextChanged_Lambda([this](const FString& InText)
                {
                    if (_PanelHighlightString == InText) { return; }
                    _PanelHighlightString = InText;
                    Request_RebuildInspectors();
                })
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

    // A rebuild parked while the user was typing fires here, on the first tick after the edit ends.
    // Deliberately ahead of the refresh gate: this is a one-shot edge, not per-frame work, and the
    // panel is showing stale structure until it runs.
    if (_EditGuard.IsValid() && _EditGuard->Consume_PendingRebuild())
    {
        RebuildInspectors();
        return;
    }

    // Honour the user's refresh-mode + rate-cap settings for the ECS debugger.
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(SCkDebuggerWindow_Main::WindowId))
    { return; }

    const auto HasActiveEdit = _EditGuard.IsValid() && _EditGuard->Get_HasActiveEdit();

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
            if (NOT ck_debugger_panel_inspector::Should_TickInspector(
                Inspector->CanInspect(Entity),
                Inspector->Wants_TickWhenNotInspectable(Entity)))
            { continue; }

            Inspector->Tick(Entity, InDeltaTime);

            if (NOT Inspector->NeedsRebuild())
            { continue; }

            // DEFER, don't drop: while a row is mid-edit the inspector keeps its dirty flag, so the
            // request survives to the tick after the edit ends instead of being swallowed here.
            if (HasActiveEdit)
            { continue; }

            // Clear the flag so stale rebuild requests don't accumulate.
            // This is a no-op data-wise; the panel simply does not rebuild (see the POLICY note).
            Inspector->ClearRebuildFlag();
        }
    }
}

// ============================================================================
// Request_RebuildInspectors
// ============================================================================

auto SCkDebuggerPanel_Inspector::Request_RebuildInspectors() -> void
{
    if (_EditGuard.IsValid() && _EditGuard->Get_HasActiveEdit())
    {
        _EditGuard->Request_Rebuild();
        return;
    }

    RebuildInspectors();
}

// ============================================================================
// RebuildInspectors
// ============================================================================

auto SCkDebuggerPanel_Inspector::RebuildInspectors() -> void
{
    if (NOT ScrollBox.IsValid())
    { return; }

    DeactivateAllInspectors();

    // Every interactive row is about to be destroyed, taking its edit scope with it. Clearing here
    // makes that explicit rather than relying on destruction order.
    if (_EditGuard.IsValid())
    { _EditGuard->Clear_AllEdits(); }

    ScrollBox->ClearChildren();
    _InspectorContentContainers.Empty();

    // Hide mode toggle by default
    if (_ModeToggleContainer.IsValid())
    {
        _ModeToggleContainer->SetContent(SNullWidget::NullWidget);
    }

    RebuildBreadcrumb();

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
// RebuildBreadcrumb
// ============================================================================

auto SCkDebuggerPanel_Inspector::RebuildBreadcrumb() -> void
{
    if (NOT _BreadcrumbContainer.IsValid())
    { return; }

    _BreadcrumbContainer->SetContent(SNullWidget::NullWidget);

    if (NOT SelectionModel.IsValid())
    { return; }

    const auto Primary = SelectionModel->Get_PrimarySelection();
    if (ck::Is_NOT_Valid(Primary) || UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(Primary))
    { return; }

    // Owner chain, root-first, transient root omitted. Depth-capped like the
    // selection-sync lineage walk.
    constexpr auto MaxDepth = 8;
    auto Chain = TArray<FCk_Handle>{};
    auto Cursor = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Primary);
    for (auto Depth = 0; Depth < MaxDepth && ck::IsValid(Cursor); ++Depth)
    {
        if (UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(Cursor))
        { break; }

        Chain.Add(Cursor);
        Cursor = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Cursor);
    }

    // Top-level entity — an ancestry strip would be noise.
    if (Chain.IsEmpty())
    { return; }

    Algo::Reverse(Chain);

    const auto Crumbs = SNew(SWrapBox).UseAllottedSize(true);
    for (const auto& Ancestor : Chain)
    {
        Crumbs->AddSlot()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 1.0f))
            [
                SNew(SCkDebug_EntityRef)
                .ShowName(true)
                .Entity(Ancestor)
            ];

        Crumbs->AddSlot()
            .VAlign(VAlign_Center)
            .Padding(FMargin(4.0f, 1.0f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("›")))
                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
            ];
    }

    // Tail: the selected entity itself, muted (it's already the inspector's subject).
    Crumbs->AddSlot()
        .VAlign(VAlign_Center)
        .Padding(FMargin(0.0f, 1.0f))
        [
            SNew(STextBlock)
            .Text(Format_EntityDisplayName(Primary))
            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
        ];

    _BreadcrumbContainer->SetContent(
        SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
        .Padding(FMargin(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small))
        [
            Crumbs
        ]);
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
    Request_RebuildInspectors();
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

    VerticalBox->AddSlot()
        .AutoHeight()
        .Padding(FCkDebuggerStyle::Padding_Small)
        [
            SNew(SCkDebug_EntityDebuggerLinks)
            .Entity(Entity)
            .ExcludeTabId(TEXT("CkEcsDebugger"))
        ];

    auto FirstSection = true;

    auto AddSeparator = [&]()
    {
        if (NOT FirstSection)
        {
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small)
                [
                    ck::debug_axes::Make_AxisSeparator()
                ];
        }
        FirstSection = false;
    };

    auto AddSection = [&](const FText& InName, const TSharedRef<SWidget>& InContent,
        const FSlateBrush* InIconBrush, const FLinearColor& InIconColor, float InRenderOpacity)
    {
        VerticalBox->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebug_InspectorPanel)
                .RenderOpacity(InRenderOpacity)
                .Title(InName)
                .IconBrush(InIconBrush)
                .IconColor(InIconColor)
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

        // Panel-level section search — hide sections whose inspector name doesn't match.
        if (NOT Matches_PanelFilter(Inspector))
        { continue; }

        const auto SectionOpacity = Get_PanelHighlightOpacity(Inspector);

        if (Inspector->IsMultiSection())
        {
            for (const auto& Section : Inspector->Get_InspectorSections(Entity))
            {
                AddSeparator();
                AddSection(Section.Name, Section.Widget,
                    ck_debugger_panel_inspector::Get_InspectorIconBrush(Inspector),
                    ck_debugger_panel_inspector::Get_InspectorIconColor(Inspector),
                    SectionOpacity);
            }
        }
        else
        {
            AddSeparator();
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SBox)
                    .RenderOpacity(SectionOpacity)
                    [
                        Build_InspectorSection(Entity, Inspector, Index)
                    ]
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
        .IconBrush(ck_debugger_panel_inspector::Get_InspectorIconBrush(Inspector))
        .IconColor(ck_debugger_panel_inspector::Get_InspectorIconColor(Inspector))
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

        // Panel-level section search — same name test as single-entity mode, applied
        // here to the OUTER inspector group.
        if (NOT Matches_PanelFilter(Inspector)) { continue; }

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
                    ck::debug_axes::Make_AxisSeparator()
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
                .RenderOpacity(Get_PanelHighlightOpacity(Inspector))
                .Title(Inspector->Get_ComponentName())
                .IconBrush(ck_debugger_panel_inspector::Get_InspectorIconBrush(Inspector))
                .IconColor(ck_debugger_panel_inspector::Get_InspectorIconColor(Inspector))
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

            // Panel-level section search — in entity-grouped mode the inspector
            // sub-sections are what carry the component name, so filter here.
            if (NOT Matches_PanelFilter(Inspector)) { continue; }

            HasAnyInspector = true;

            InnerBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SBox)
                    .RenderOpacity(Get_PanelHighlightOpacity(Inspector))
                    [
                        Build_EntitySubSection(Entity, Inspector, EntityIdx, InspectorIdx)
                    ]
                ];
        }

        if (NOT HasAnyInspector) { continue; }

        if (NOT FirstSection)
        {
            VerticalBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Small)
                [
                    ck::debug_axes::Make_AxisSeparator()
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

    // Icon only when the header names the inspector (GroupByEntity mode) — in
    // GroupByInspector mode the sub-section header is the ENTITY's name, and the
    // outer panel already carries the inspector's glyph.
    const auto ShowInspectorIcon = _DisplayMode == ECkInspectorDisplayMode::GroupByEntity;

    return SNew(SCkDebug_InspectorPanel)
        .Title(HeaderText)
        .IconBrush(ShowInspectorIcon ? ck_debugger_panel_inspector::Get_InspectorIconBrush(Inspector) : nullptr)
        .IconColor(ck_debugger_panel_inspector::Get_InspectorIconColor(Inspector))
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
            Inspector->Set_EditGuard(_EditGuard);
        }
    }
}

// ============================================================================
// OnSelectionChanged
// ============================================================================

auto SCkDebuggerPanel_Inspector::OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void
{
    Request_RebuildInspectors();
}

// ============================================================================
// OnInspectorFilterChanged
// ============================================================================

auto SCkDebuggerPanel_Inspector::OnInspectorFilterChanged(int32 InspectorIndex, const FString& InFilterText) -> void
{
    InspectorFilters.Add(InspectorIndex, InFilterText);

    // The granular path below is still a structural swap of the section's content — it would destroy
    // an interactive row mid-edit exactly like a full rebuild. The filter text is already stored, so
    // parking the rebuild loses nothing: it re-runs with this filter once the edit ends.
    if (_EditGuard.IsValid() && _EditGuard->Get_HasActiveEdit())
    {
        _EditGuard->Request_Rebuild();
        return;
    }

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
