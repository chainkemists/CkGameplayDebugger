#include "CkDebuggerPanel_Inspector.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_SearchBar.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityDebuggerLinks.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
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

    // Diff mode replays every inspector's Build path once PER ENTITY. Past a handful of entities that
    // cost stops paying for itself and the tint stops being readable, so the comparison is capped -
    // and the remainder is REPORTED next to the toggle, never silently dropped.
    constexpr auto MaxDiffEntities = 8;

    // Feature glyph + accent for an inspector's section header (the debugger-wide
    // icon language). Nullptr brush = inspector declared no glyph; header unchanged.
    static auto Get_InspectorIconBrush(const TSharedPtr<ICkDebuggerComponentInspector_Base>& InInspector) -> const FSlateBrush*
    {
        return FCkIconStyle::Get_Brush(InInspector->Get_Icon(), ECk_Icon_BrushSize::Size_16x16);
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

auto ck_debugger_panel_inspector::Matches_SectionQuery(
    const FString& InQuery, const FString& InInspectorName, const FString& InSectionName) -> bool
{
    // Display labels split identifiers such as WorldItem into World Item.
    const auto Normalize = [](const FString& InText)
    {
        return InText.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
    };
    const auto Query = Normalize(InQuery);
    return Query.IsEmpty() || Normalize(InInspectorName).Contains(Query, ESearchCase::IgnoreCase) ||
        Normalize(InSectionName).Contains(Query, ESearchCase::IgnoreCase);
}

auto SCkDebuggerPanel_Inspector::Matches_PanelFilter(
    const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector, const FText& InSectionName) const -> bool
{
    if (_PanelFilterString.IsEmpty())
    { return true; }

    if (NOT Inspector.IsValid())
    { return false; }

    return ck_debugger_panel_inspector::Matches_SectionQuery(
        _PanelFilterString, Inspector->Get_ComponentName().ToString(), InSectionName.ToString());
}

// Highlight pass - dim rather than hide. Returns the section's RenderOpacity.
//
// NOTE: RenderOpacity is a plain float SLATE argument in this engine
// (SlateCore SWidget.h:1876 / DeclarativeSyntaxSupport.h:671), NOT a TAttribute,
// so it cannot be lambda-bound. That is fine here: every change to the highlight
// query runs RebuildInspectors, so a value computed at build time is always current.
auto SCkDebuggerPanel_Inspector::Get_PanelHighlightOpacity(
    const TSharedPtr<ICkDebuggerComponentInspector_Base>& Inspector, const FText& InSectionName) const -> float
{
    constexpr auto FullOpacity = 1.0f;
    constexpr auto DimOpacity  = 0.35f;

    if (_PanelHighlightString.IsEmpty())
    { return FullOpacity; }

    if (NOT Inspector.IsValid())
    { return DimOpacity; }

    return ck_debugger_panel_inspector::Matches_SectionQuery(
        _PanelHighlightString, Inspector->Get_ComponentName().ToString(), InSectionName.ToString())
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

    // POLICY - NEVER rebuild widget structure from Tick.
    //
    // Widget trees are built exactly once per (entity selection × inspector)
    // pair. Dynamic values flow via TAttribute<FText> bindings on the rows
    // produced by FCkInspectorWidgetBuilder; those bindings re-evaluate every
    // paint and pick up live data with zero structural churn. Inspectors that
    // need per-tick updates MUST mutate in-place (e.g. FCkInspector_InteractTarget
    // updates its existing badge boxes via PopulateBadgeBox - no SetContent).
    //
    // Inspectors retain RequestRebuild() as a marker for genuine structural
    // changes (new inventory item, new interaction target). We intentionally
    // DO NOT rebuild the panel in response - any structural mutation of a
    // Slate widget tree during the Tick phase causes a one-frame "scrunch"
    // where the new sub-tree hasn't been prepassed yet and parent AutoHeight
    // slots collapse to zero. See the Gallery's "Rebuild Storm" section for
    // a live A/B repro - only the Data-only strategy (TAttribute bindings,
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

    // Recomputed below only for a multi-entity selection; clearing here is what keeps a stale verdict
    // from surviving into a single-entity or empty selection.
    _DiffLabelsByInspector.Empty();
    _DiffSkippedCount = 0;

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

        // Ahead of BOTH the toggle row (which reports the cap) and the content build (which consumes
        // the verdict) - the compare pass is what makes those two agree.
        Rebuild_DiffLabels(SelectedEntities);

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

    // Top-level entity - an ancestry strip would be noise.
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

            // Diff mode rides the mode row because that row already exists exactly when more than one
            // entity is selected - the only selection shape a value comparison means anything for.
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                Build_DiffModeControls()
            ]
        ];
}

// ============================================================================
// Build_DiffModeControls
// ============================================================================

auto SCkDebuggerPanel_Inspector::Build_DiffModeControls() -> TSharedRef<SWidget>
{
    const auto WeakPanel = TWeakPtr<SCkDebuggerPanel_Inspector>{SharedThis(this)};

    auto Row = SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SCkDebug_IconToggle)
            .IconId(ECk_Icon::Size)
            .Label(FText::FromString(TEXT("Diff Values")))
            .ToolTip(FText::FromString(ck::Format_UE(
                TEXT("Tint every row whose value DIFFERS across the selected entities. Compares up to {} of them; a label missing from an entity counts as a difference."),
                ck_debugger_panel_inspector::MaxDiffEntities)))
            .IsOn_Lambda([WeakPanel]()
            {
                const auto Panel = WeakPanel.Pin();
                return Panel.IsValid() && Panel->_DiffMode;
            })
            .OnStateChanged_Lambda([WeakPanel](bool InIsOn)
            {
                const auto Panel = WeakPanel.Pin();

                if (NOT Panel.IsValid() || Panel->_DiffMode == InIsOn)
                { return; }

                Panel->_DiffMode = InIsOn;
                Panel->Request_RebuildInspectors();
            })
        ];

    // The cap is stated, never silent: the user must be able to tell "these rows all match" from
    // "these rows were never compared".
    if (_DiffMode && _DiffSkippedCount > 0)
    {
        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Text(FText::FromString(ck::Format_UE(TEXT("+{} not compared"), _DiffSkippedCount)))
                .ToolTipText(FText::FromString(ck::Format_UE(
                    TEXT("Only the first {} selected entities take part in the comparison."),
                    ck_debugger_panel_inspector::MaxDiffEntities)))
                .ColorAndOpacity(FSlateColor{CkStyle::Warn()})
            ];
    }

    return Row;
}

// ============================================================================
// Rebuild_DiffLabels
// ============================================================================

auto SCkDebuggerPanel_Inspector::Rebuild_DiffLabels(const TArray<FCk_Handle>& InEntities) -> void
{
    _DiffLabelsByInspector.Empty();
    _DiffSkippedCount = 0;

    if (NOT _DiffMode)
    { return; }

    auto Comparable = TArray<FCk_Handle>{};
    Comparable.Reserve(InEntities.Num());

    for (const auto& Entity : InEntities)
    {
        if (ck::IsValid(Entity))
        { Comparable.Add(Entity); }
    }

    _DiffSkippedCount = FMath::Max(0, Comparable.Num() - ck_debugger_panel_inspector::MaxDiffEntities);

    if (_DiffSkippedCount > 0)
    { Comparable.SetNum(ck_debugger_panel_inspector::MaxDiffEntities); }

    if (Comparable.Num() < 2)
    { return; }

    for (auto Index = 0; Index < Inspectors.Num(); ++Index)
    {
        const auto& Inspector = Inspectors[Index];

        if (NOT Inspector.IsValid())
        { continue; }

        const auto Filter = InspectorFilters.FindRef(Index);

        auto PerEntityRows = TArray<TMap<FString, FString>>{};
        PerEntityRows.Reserve(Comparable.Num());

        for (const auto& Entity : Comparable)
        {
            // An inspector that cannot inspect this entity contributes an EMPTY map, which makes every
            // label the others carry read as a difference - the honest answer for "this one has no
            // Transform at all".
            if (NOT Inspector->CanInspect(Entity))
            {
                PerEntityRows.Emplace();
                continue;
            }

            // The scope makes the replay compose nothing; the returned widget is deliberately dropped.
            const auto Capture = FCkInspector_RowCaptureScope{};

            if (Inspector->IsMultiSection())
            { Inspector->Get_InspectorSections(Entity); }
            else if (Inspector->IsFilterable())
            { Inspector->Build_Inspector(Entity, Filter); }
            else
            { Inspector->Build_Inspector(Entity); }

            PerEntityRows.Add(Capture.Get_Rows());
        }

        auto Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels(PerEntityRows);

        if (Differing.Num() == 0)
        { continue; }

        _DiffLabelsByInspector.Add(Index, MoveTemp(Differing));
    }
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

    // Composed through the EntityIdStyle axis instead of a private "{Name} [{Id}]" format, so this
    // header reads exactly like every SCkDebug_EntityRef pill in the suite and follows the same
    // NameAndId / CompactId / NameOnly setting. The panel re-composes on the window's style
    // revision, so a Style Lab flip lands without a re-selection.
    const auto CleanName = ck::DebugNameClean::Get_CleanName(
        UCk_Utils_Handle_UE::Get_DebugName(Entity).ToString());

    return ck::debug_axes::Make_EntityIdText(
        UCkDebuggerStyleSettings::Get_Selection(),
        CleanName,
        ck::Format_UE(TEXT("{}"), Entity.Get_Entity()));
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

        if (NOT Inspector->IsMultiSection() && NOT Matches_PanelFilter(Inspector))
        { continue; }

        const auto SectionOpacity = Get_PanelHighlightOpacity(Inspector);

        if (Inspector->IsMultiSection())
        {
            for (const auto& Section : Inspector->Get_InspectorSections(Entity))
            {
                if (NOT Matches_PanelFilter(Inspector, Section.Name))
                { continue; }
                AddSeparator();
                AddSection(Section.Name, Section.Widget,
                    ck_debugger_panel_inspector::Get_InspectorIconBrush(Inspector),
                    ck_debugger_panel_inspector::Get_InspectorIconColor(Inspector),
                    Get_PanelHighlightOpacity(Inspector, Section.Name));
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

        if (NOT Inspector->IsMultiSection() && NOT Matches_PanelFilter(Inspector)) { continue; }

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

        auto VisibleEntityCount = 0;
        for (int32 EntityIdx = 0; EntityIdx < ApplicableEntities.Num(); ++EntityIdx)
        {
            const auto& Entity = ApplicableEntities[EntityIdx];
            const auto SubSection = Build_EntitySubSection(Entity, Inspector, InspectorIdx, EntityIdx);
            if (NOT SubSection.IsValid())
            { continue; }
            ++VisibleEntityCount;
            InnerBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SubSection.ToSharedRef()
                ];
        }

        if (VisibleEntityCount == 0)
        { continue; }
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

        // Top-level expandable for the inspector
        VerticalBox->AddSlot()
            .AutoHeight()
            .Padding(FCkDebuggerStyle::Padding_Small)
            [
                SNew(SCkDebug_InspectorPanel)
                .RenderOpacity(Inspector->IsMultiSection() ? 1.0f : Get_PanelHighlightOpacity(Inspector))
                .Title(Inspector->Get_ComponentName())
                .IconBrush(ck_debugger_panel_inspector::Get_InspectorIconBrush(Inspector))
                .IconColor(ck_debugger_panel_inspector::Get_InspectorIconColor(Inspector))
                .CountText(FText::FromString(ck::Format_UE(TEXT("{}"), VisibleEntityCount)))
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

            if (NOT Inspector->IsMultiSection() && NOT Matches_PanelFilter(Inspector)) { continue; }

            const auto SubSection = Build_EntitySubSection(Entity, Inspector, EntityIdx, InspectorIdx);
            if (NOT SubSection.IsValid())
            { continue; }

            HasAnyInspector = true;

            InnerBox->AddSlot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    SNew(SBox)
                    .RenderOpacity(Inspector->IsMultiSection() ? 1.0f : Get_PanelHighlightOpacity(Inspector))
                    [
                        SubSection.ToSharedRef()
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
    int32 InnerIndex) -> TSharedPtr<SWidget>
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

    // Diff verdict for THIS inspector, computed once in Rebuild_DiffLabels. A null set is an inactive
    // scope, so a non-diff rebuild composes exactly the tree it always did. The scope must outlive
    // every Build_Inspector call below, which is why it is declared here rather than inline.
    const auto DiffScope = FCkInspector_DiffMarkScope{_DiffLabelsByInspector.Find(InspectorIndex)};

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
        auto HasVisibleSection = false;
        for (const auto& Section : Inspector->Get_InspectorSections(Entity))
        {
            if (NOT Matches_PanelFilter(Inspector, Section.Name))
            { continue; }
            HasVisibleSection = true;
            BodyContent->AddSlot()
                .AutoHeight()
                .Padding(FMargin(FCkDebuggerStyle::Padding_Medium))
                [
                    SNew(SVerticalBox)
                    .RenderOpacity(Get_PanelHighlightOpacity(Inspector, Section.Name))

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
        if (NOT HasVisibleSection)
        { return {}; }
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

    // Icon only when the header names the inspector (GroupByEntity mode) - in
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

    // The granular path below is still a structural swap of the section's content - it would destroy
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
