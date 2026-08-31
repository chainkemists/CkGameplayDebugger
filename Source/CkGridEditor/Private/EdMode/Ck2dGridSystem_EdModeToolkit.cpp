#include "CkGridEditor/EdMode/Ck2dGridSystem_EdModeToolkit.h"

#include "CkGridEditor/EdMode/Ck2dGridSystem_EdMode.h"
#include "CkGridEditor/Draw/Ck2dGridSystem_AuthoredOverlay.h"

#include "CkGrid/2dGridSystem/Authoring/Ck2dGridSystem_Spec.h"

#include "CkEntitySpawner/CkEntitySpawner_Actor.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CategoryDot.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconButton.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_LabeledGroup.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "SGameplayTagCombo.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Styling/AppStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "Ck_2dGridSystem_EdModeToolkit"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_grid_toolkit
{
    auto
    Make_Group(
        const FText&        InLabel,
        TSharedRef<SWidget> InBody) -> TSharedRef<SWidget>
    {
        auto Group = SNew(SCkDebug_LabeledGroup).Label(InLabel);
        Group->AddChild(InBody);
        return Group;
    }

    auto
    Make_BodyText(
        const FText&          InText,
        const FLinearColor&   InColor) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Text(InText)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
            .ColorAndOpacity(FSlateColor(InColor))
            .AutoWrapText(true);
    }

    // A rounded chip carrying one label in ONE color — hairline ring, faint fill, and text, all three
    // tinted from the caller's color so the chip reads as that thing's own swatch. Two nested SBorders
    // and an STextBlock only, so a chip stays click-passive inside an STableRow.
    auto
    Make_Chip(
        const FText&        InText,
        const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush_Pill())
            .BorderBackgroundColor(FSlateColor(CkStyle::OverlayOf(InColor, CkStyle::AlphaSoft())))
            .Padding(FMargin(1.0f))
            [
                SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush_Pill())
                .BorderBackgroundColor(FSlateColor(CkStyle::OverlayOf(InColor, CkStyle::AlphaFaint())))
                .Padding(FMargin(CkStyle::SpaceM, 2.0f))
                [
                    SNew(STextBlock)
                    .Text(InText)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor(InColor))
                ]
            ];
    }

    // One shape for every tag row in the toolkit — legend, Select-tool list, per-cell rows, multi-cell
    // union — so a row gains an action or a count without the four drifting apart. Composed only of
    // click-passive widgets, which is what lets the Select-tool list host it inside an STableRow.
    //
    // Dot, chip and count badge are all tinted from ck::grid_editor::Resolve_TagColor — the SAME
    // function the authored overlay paints the world with, so a tag's color in this panel and its
    // color on the grid are one value, not two that can drift.
    auto
    Make_TagRow(
        const FGameplayTag& InTag,
        TOptional<int32>    InCount,
        TSharedPtr<SWidget> InTrailing) -> TSharedRef<SWidget>
    {
        const auto TagColor = ck::grid_editor::Resolve_TagColor(InTag);

        auto Row = SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(SCkDebug_CategoryDot)
                .Color(TagColor)
                .Diameter(10.0f)
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Left)
            [
                Make_Chip(FText::FromName(InTag.GetTagName()), TagColor)
            ];

        if (InCount.IsSet())
        {
            Row->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_CountBadge)
                    .ValueText(FText::AsNumber(InCount.GetValue()))
                    .ValueColor(TagColor)
                    .BackgroundColor(CkStyle::OverlayOf(TagColor, CkStyle::AlphaFaint()))
                    .BorderColor(CkStyle::OverlayOf(TagColor, CkStyle::AlphaDim()))
                ];
        }

        if (InTrailing.IsValid())
        {
            Row->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    InTrailing.ToSharedRef()
                ];
        }

        return Row;
    }

    // One gesture hint: the pill carries the mouse/key combo, the dim text what it does. The pill sits in a
    // fixed-width, left-aligned box so a block of hints reads as two columns instead of a ragged edge.
    auto
    Make_HintRow(
        const FText& InGesture,
        const FText& InEffect) -> TSharedRef<SWidget>
    {
        constexpr auto GestureColumnWidth = 96.0f;

        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(SBox)
                .MinDesiredWidth(GestureColumnWidth)
                .HAlign(HAlign_Left)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text(InGesture)
                    .Tone(ECk_Tone::Neutral)
                    .ShowDot(false)
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(InEffect)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                .AutoWrapText(true)
            ];
    }
}

// --------------------------------------------------------------------------------------------------------------------

FCk_2dGridSystem_EdModeToolkit::FCk_2dGridSystem_EdModeToolkit()
{
}

void
    FCk_2dGridSystem_EdModeToolkit::
    Init(
        const TSharedPtr<IToolkitHost>& InToolkitHost,
        TWeakObjectPtr<UEdMode>         InOwningMode)
{
    OwningMode = InOwningMode;

    using FToolControl = SSegmentedControl<ECk_GridPaint_Tool>;

    const auto ToolSelector = SNew(FToolControl)
        .Value(this, &FCk_2dGridSystem_EdModeToolkit::Get_ActiveTool)
        .OnValueChanged(this, &FCk_2dGridSystem_EdModeToolkit::Set_ActiveTool)
        + FToolControl::Slot(ECk_GridPaint_Tool::Shape)
            .Text(LOCTEXT("ToolShape", "Shape"))
            .ToolTip(LOCTEXT("ToolShapeTip", "Toggle a cell's membership in the grid's disabled set — paints the grid footprint."))
        + FToolControl::Slot(ECk_GridPaint_Tool::Tags)
            .Text(LOCTEXT("ToolTags", "Tags"))
            .ToolTip(LOCTEXT("ToolTagsTip", "Paint per-cell gameplay tags, or edit the grid-wide default cell tags."))
        + FToolControl::Slot(ECk_GridPaint_Tool::Blocker)
            .Text(LOCTEXT("ToolBlocker", "Blocker"))
            .ToolTip(LOCTEXT("ToolBlockerTip", "Drag a rect to place a blocker footprint; click one to select it."))
        + FToolControl::Slot(ECk_GridPaint_Tool::Select)
            .Text(LOCTEXT("ToolSelect", "Select"))
            .ToolTip(LOCTEXT("ToolSelectTip", "Click a cell or blocker, or drag a marquee, then edit it in Cell Details."));

    SAssignNew(InlineContent, SBorder)
    .BorderImage(FCkDebuggerStyle::Get_SquareBrush())
    .BorderBackgroundColor(FSlateColor(CkStyle::BgRoot()))
    .Padding(FMargin(CkStyle::SpaceM))
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
        [
            Build_GridsInLevelSection()
        ]
        + SScrollBox::Slot()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
        [
            Build_GridSection()
        ]
        + SScrollBox::Slot()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(LOCTEXT("PaintToolsLabel", "Paint Tool"))
            .ToolTip(LOCTEXT("PaintToolsTip", "What a left-drag in the viewport does. Shift erases for Shape and Tags."))
        ]
        + SScrollBox::Slot()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
        [
            ToolSelector
        ]
        + SScrollBox::Slot()
        [
            Build_TagsSection()
        ]
        + SScrollBox::Slot()
        [
            Build_BlockerSection()
        ]
        + SScrollBox::Slot()
        [
            Build_DetailsSection()
        ]
        + SScrollBox::Slot()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            Build_ControlsSection()
        ]
    ];

    FModeToolkit::Init(InToolkitHost, InOwningMode);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Build_GridsInLevelSection() -> TSharedRef<SWidget>
{
    SAssignNew(GridsInLevelContainer, SVerticalBox);
    Rebuild_GridsInLevelList();

    auto Body = SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
        [
            SNew(STextBlock)
            .Text(this, &FCk_2dGridSystem_EdModeToolkit::Get_GridsInLevelSummaryText)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            GridsInLevelContainer.ToSharedRef()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SNew(SCkDebug_IconToggle)
            .IconId(ECk_Icon::Visibility)
            .Label(LOCTEXT("ShowAllGrids", "Show All Grids"))
            .ShowLabel(true)
            .ToolTip(LOCTEXT("ShowAllGridsTip", "Also draw the authored overlay of every UNSELECTED grid in the level. The targeted grid keeps its selection highlights and drag preview; the others draw as plain overlays."))
            .IsOn(this, &FCk_2dGridSystem_EdModeToolkit::Get_ShowAllGrids)
            .OnStateChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_ShowAllGridsChanged)
        ];

    return ck_grid_toolkit::Make_Group(LOCTEXT("GridsInLevelTitle", "Grids In Level"), Body);
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_GridsInLevelSummaryText() const -> FText
{
    // Live-bound each repaint, so the roster rebuild piggybacks here; the signature guard keeps it cheap.
    // const_cast: it owns the toolkit's view state.
    {
        const auto Signature = Compute_GridsInLevelSignature();
        if (Signature != SeededGridsInLevelSignature)
        {
            auto* MutableThis = const_cast<FCk_2dGridSystem_EdModeToolkit*>(this);
            MutableThis->SeededGridsInLevelSignature = Signature;
            MutableThis->Rebuild_GridsInLevelList();
        }
    }

    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr || Mode->Get_SelectedGridSpawnerActor() == nullptr)
    { return LOCTEXT("GridsInLevelNoTarget", "Click a grid to target it"); }

    return LOCTEXT("GridsInLevelHasTarget", "Click another grid to retarget");
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Compute_GridsInLevelSignature() const -> FString
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return FString{}; }

    auto* World = Mode->GetWorld();
    if (World == nullptr)
    { return FString{}; }

    const auto* ActiveSpawner = Mode->Get_SelectedGridSpawnerActor();

    auto Entries = TArray<FString>{};
    for (auto It = TActorIterator<ACk_EntitySpawner_UE>(World); It; ++It)
    {
        auto* Spawner = *It;
        const auto* Spec = ck::grid_editor::Resolve_SpecFromSpawner(Spawner);
        if (Spec == nullptr)
        { continue; }

        Entries.Add(FString::Printf(TEXT("%s|%s|%dx%d|%d"),
            *Spawner->GetName(),
            *Spawner->GetActorNameOrLabel(),
            Spec->Dimensions.X, Spec->Dimensions.Y,
            Spawner == ActiveSpawner ? 1 : 0));
    }

    // TActorIterator order is not guaranteed stable across ticks; sorting makes the signature — and the
    // rebuilt row order — deterministic.
    Entries.Sort();
    return FString::Join(Entries, TEXT(";"));
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Rebuild_GridsInLevelList() -> void
{
    if (! GridsInLevelContainer.IsValid())
    { return; }

    GridsInLevelContainer->ClearChildren();

    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    auto* World = Mode != nullptr ? Mode->GetWorld() : nullptr;
    const auto* ActiveSpawner = Mode != nullptr ? Mode->Get_SelectedGridSpawnerActor() : nullptr;

    struct FGridEntry
    {
        TWeakObjectPtr<AActor> Spawner;
        FString                Label;
        FIntPoint              Dimensions = FIntPoint::ZeroValue;
        bool                   IsActive   = false;
    };

    auto Entries = TArray<FGridEntry>{};

    if (World != nullptr)
    {
        for (auto It = TActorIterator<ACk_EntitySpawner_UE>(World); It; ++It)
        {
            auto* Spawner = *It;
            const auto* Spec = ck::grid_editor::Resolve_SpecFromSpawner(Spawner);
            if (Spec == nullptr)
            { continue; }

            auto Entry       = FGridEntry{};
            Entry.Spawner    = Spawner;
            Entry.Label      = Spawner->GetActorNameOrLabel();
            Entry.Dimensions = Spec->Dimensions;
            Entry.IsActive   = Spawner == ActiveSpawner;
            Entries.Add(MoveTemp(Entry));
        }
    }

    Entries.Sort([](const FGridEntry& InLhs, const FGridEntry& InRhs)
    {
        return InLhs.Label < InRhs.Label;
    });

    if (Entries.IsEmpty())
    {
        GridsInLevelContainer->AddSlot()
        .AutoHeight()
        [
            ck_grid_toolkit::Make_BodyText(LOCTEXT("GridsInLevelNone", "No grids in level"), CkStyle::TextMute())
        ];
        return;
    }

    // A 2px accent band on the targeted row's leading edge. Inactive rows keep the same band in
    // transparent, so every label in the roster starts at one x — the stripe never shifts the text.
    constexpr auto StripeWidth = 2.0f;

    for (const auto& Entry : Entries)
    {
        const auto LabelColor  = Entry.IsActive ? CkStyle::Accent() : CkStyle::Text();
        const auto DotColor    = Entry.IsActive ? CkStyle::Accent() : CkStyle::TextMute();
        const auto StripeColor = Entry.IsActive ? CkStyle::Accent() : FLinearColor::Transparent;
        const auto RowFill     = Entry.IsActive
            ? CkStyle::OverlayOf(CkStyle::Accent(), CkStyle::AlphaFaint())
            : FLinearColor::Transparent;
        const auto DimsFill    = Entry.IsActive
            ? CkStyle::OverlayOf(CkStyle::Accent(), CkStyle::AlphaFaint())
            : CkStyle::Bg3();

        // The whole label area is one borderless button rather than a list row, so the per-row frame
        // action can sit beside it without a table row swallowing the selection click.
        auto RowButton = SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ContentPadding(FMargin(0.0f, 1.0f))
            .HAlign(HAlign_Fill)
            .ToolTipText(LOCTEXT("GridsInLevelSelectTip", "Select this grid spawner — the paint mode then targets its grid"))
            .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_SelectGridSpawner, Entry.Spawner)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Fill)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SBox)
                    .WidthOverride(StripeWidth)
                    [
                        SNew(SImage)
                        .Image(CkStyle::GetRoundedBrush_Small())
                        .ColorAndOpacity(FSlateColor(StripeColor))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SCkDebug_CategoryDot)
                    .Color(DotColor)
                    .Diameter(10.0f)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Entry.Label))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
                    .ColorAndOpacity(FSlateColor(LabelColor))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_CountBadge)
                    .ValueText(FText::Format(LOCTEXT("GridsInLevelDims", "{0} x {1}"),
                        FText::AsNumber(Entry.Dimensions.X), FText::AsNumber(Entry.Dimensions.Y)))
                    .ValueColor(Entry.IsActive ? CkStyle::Accent() : CkStyle::TextMute())
                    .BackgroundColor(DimsFill)
                    .BorderColor(CkStyle::Border())
                ]
            ];

        auto Row = SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                // Click-passive tint band: the targeted row reads as selected without a second
                // widget between the pointer and the row button underneath it.
                SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush_Small())
                .BorderBackgroundColor(FSlateColor(RowFill))
                .Padding(FMargin(CkStyle::SpaceXS, 0.0f))
                [
                    RowButton
                ]
            ];

        if (Entry.IsActive)
        {
            Row->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_StatusPill)
                    .Text(LOCTEXT("GridsInLevelActive", "TARGET"))
                    .Tone(ECk_Tone::Accent)
                ];
        }

        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(SCkDebug_IconButton)
                .IconId(ECk_Icon::FrameActor)
                .Label(LOCTEXT("GridsInLevelFrameTip", "Frame this grid in the level viewport"))
                .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_FrameGridSpawner, Entry.Spawner)
            ];

        GridsInLevelContainer->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 1.0f)
        [
            Row
        ];
    }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_SelectGridSpawner(
        TWeakObjectPtr<AActor> InSpawner) -> FReply
{
    auto* Spawner = InSpawner.Get();
    if (GEditor == nullptr || Spawner == nullptr)
    { return FReply::Handled(); }

    // The paint mode resolves its target grid from the actor selection, so selecting IS targeting.
    GEditor->SelectNone(false, true);
    GEditor->SelectActor(Spawner, true, false);
    GEditor->NoteSelectionChange();

    return FReply::Handled();
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_FrameGridSpawner(
        TWeakObjectPtr<AActor> InSpawner) -> FReply
{
    auto* Spawner = InSpawner.Get();
    if (GEditor == nullptr || Spawner == nullptr)
    { return FReply::Handled(); }

    constexpr auto ActiveViewportOnly = false;
    GEditor->MoveViewportCamerasToActor(*Spawner, ActiveViewportOnly);

    return FReply::Handled();
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_ShowAllGrids() const -> bool
{
    if (const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { return Mode->Get_ShowAllGrids(); }

    return false;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_ShowAllGridsChanged(
        bool InShowAll) -> void
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Set_ShowAllGrids(InShowAll); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Build_GridSection() -> TSharedRef<SWidget>
{
    const auto MakeAxisLabel = [](const FText& InLabel) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Text(InLabel)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()));
    };

    auto Body = SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
            [
                MakeAxisLabel(LOCTEXT("GridDimensions", "Dimensions"))
            ]
            + SHorizontalBox::Slot().FillWidth(0.29f).Padding(CkStyle::SpaceXS, 0.0f)
            [
                SNew(SNumericEntryBox<int32>)
                .Value(this, &FCk_2dGridSystem_EdModeToolkit::Get_GridDimensionX)
                .OnValueCommitted(this, &FCk_2dGridSystem_EdModeToolkit::On_GridDimensionXCommitted)
                .AllowSpin(true)
                .MinValue(1)
                .MinSliderValue(1)
                .MaxSliderValue(64)
            ]
            + SHorizontalBox::Slot().FillWidth(0.29f).Padding(CkStyle::SpaceXS, 0.0f)
            [
                SNew(SNumericEntryBox<int32>)
                .Value(this, &FCk_2dGridSystem_EdModeToolkit::Get_GridDimensionY)
                .OnValueCommitted(this, &FCk_2dGridSystem_EdModeToolkit::On_GridDimensionYCommitted)
                .AllowSpin(true)
                .MinValue(1)
                .MinSliderValue(1)
                .MaxSliderValue(64)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
            [
                MakeAxisLabel(LOCTEXT("GridCellSize", "Cell Size"))
            ]
            + SHorizontalBox::Slot().FillWidth(0.29f).Padding(CkStyle::SpaceXS, 0.0f)
            [
                SNew(SNumericEntryBox<double>)
                .Value(this, &FCk_2dGridSystem_EdModeToolkit::Get_GridCellSizeX)
                .OnValueCommitted(this, &FCk_2dGridSystem_EdModeToolkit::On_GridCellSizeXCommitted)
                .AllowSpin(true)
                .MinValue(1.0)
                .MinSliderValue(1.0)
                .MaxSliderValue(1000.0)
            ]
            + SHorizontalBox::Slot().FillWidth(0.29f).Padding(CkStyle::SpaceXS, 0.0f)
            [
                SNew(SNumericEntryBox<double>)
                .Value(this, &FCk_2dGridSystem_EdModeToolkit::Get_GridCellSizeY)
                .OnValueCommitted(this, &FCk_2dGridSystem_EdModeToolkit::On_GridCellSizeYCommitted)
                .AllowSpin(true)
                .MinValue(1.0)
                .MinSliderValue(1.0)
                .MaxSliderValue(1000.0)
            ]
        ];

    return ck_grid_toolkit::Make_Group(LOCTEXT("GridSectionTitle", "Grid"), Body);
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_GridDimensionX() const -> TOptional<int32>
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Spec == nullptr)
    { return TOptional<int32>{}; }
    return Spec->Dimensions.X;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_GridDimensionY() const -> TOptional<int32>
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Spec == nullptr)
    { return TOptional<int32>{}; }
    return Spec->Dimensions.Y;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_GridCellSizeX() const -> TOptional<double>
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Spec == nullptr)
    { return TOptional<double>{}; }
    return Spec->CellSize.X;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_GridCellSizeY() const -> TOptional<double>
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Spec == nullptr)
    { return TOptional<double>{}; }
    return Spec->CellSize.Y;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_GridDimensionXCommitted(
        int32             InValue,
        ETextCommit::Type InCommitType) -> void
{
    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Mode == nullptr || Spec == nullptr)
    { return; }
    Mode->Set_GridDimensions(FIntPoint(InValue, Spec->Dimensions.Y));
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_GridDimensionYCommitted(
        int32             InValue,
        ETextCommit::Type InCommitType) -> void
{
    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Mode == nullptr || Spec == nullptr)
    { return; }
    Mode->Set_GridDimensions(FIntPoint(Spec->Dimensions.X, InValue));
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_GridCellSizeXCommitted(
        double            InValue,
        ETextCommit::Type InCommitType) -> void
{
    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Mode == nullptr || Spec == nullptr)
    { return; }
    Mode->Set_GridCellSize(FVector2D(InValue, Spec->CellSize.Y));
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_GridCellSizeYCommitted(
        double            InValue,
        ETextCommit::Type InCommitType) -> void
{
    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Mode == nullptr || Spec == nullptr)
    { return; }
    Mode->Set_GridCellSize(FVector2D(Spec->CellSize.X, InValue));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Build_TagsSection() -> TSharedRef<SWidget>
{
    using FScopeControl = SSegmentedControl<ECk_GridPaint_TagScope>;

    const auto ScopeSelector = SNew(FScopeControl)
        .Value(this, &FCk_2dGridSystem_EdModeToolkit::Get_ActiveTagScope)
        .OnValueChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_ActiveTagScopeChanged)
        + FScopeControl::Slot(ECk_GridPaint_TagScope::PerCellBulk)
            .Text(LOCTEXT("ScopePerCell", "Per-Cell Bulk"))
            .ToolTip(LOCTEXT("ScopePerCellTip", "A paint stroke writes the active tag onto each painted cell."))
        + FScopeControl::Slot(ECk_GridPaint_TagScope::GridDefault)
            .Text(LOCTEXT("ScopeGridDefault", "Grid Default"))
            .ToolTip(LOCTEXT("ScopeGridDefaultTip", "The Apply/Remove buttons edit the grid-wide default cell tags instead of painting."));

    const auto TagCombo = SNew(SGameplayTagCombo)
        .Filter(TEXT("Grid"))
        .Tag(this, &FCk_2dGridSystem_EdModeToolkit::Get_ActivePaintTag)
        .OnTagChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_PaintTagChanged);

    SAssignNew(TagLegendContainer, SVerticalBox);
    Rebuild_TagLegend();

    auto Body = SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(LOCTEXT("PaintTagLabel", "Paint Tag"))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            TagCombo
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(LOCTEXT("TagColorsLabel", "Tag Colors"))
            .ToolTip(LOCTEXT("TagColorsTip", "Every per-cell tag authored on this grid, with how many cells carry it."))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            TagLegendContainer.ToSharedRef()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(LOCTEXT("TagScopeLabel", "Scope"))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            ScopeSelector
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            SNew(SUniformGridPanel)
            .SlotPadding(FMargin(CkStyle::SpaceXS))
            + SUniformGridPanel::Slot(0, 0)
            [
                SNew(SButton)
                .HAlign(HAlign_Center)
                .IsEnabled(this, &FCk_2dGridSystem_EdModeToolkit::Get_GridDefaultButtonsEnabled)
                .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_ApplyGridDefaultTag)
                .ToolTipText(LOCTEXT("ApplyGridDefaultTip", "Add the active tag to the grid's DefaultCellTags"))
                [
                    SNew(STextBlock).Text(LOCTEXT("ApplyGridDefault", "Apply Grid-Default Tag"))
                ]
            ]
            + SUniformGridPanel::Slot(1, 0)
            [
                SNew(SButton)
                .HAlign(HAlign_Center)
                .IsEnabled(this, &FCk_2dGridSystem_EdModeToolkit::Get_GridDefaultButtonsEnabled)
                .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_RemoveGridDefaultTag)
                .ToolTipText(LOCTEXT("RemoveGridDefaultTip", "Remove the active tag from the grid's DefaultCellTags"))
                [
                    SNew(STextBlock).Text(LOCTEXT("RemoveGridDefault", "Remove"))
                ]
            ]
        ];

    return SNew(SBox)
        .Visibility(this, &FCk_2dGridSystem_EdModeToolkit::Get_TagsSectionVisibility)
        [
            ck_grid_toolkit::Make_Group(LOCTEXT("TagsSectionTitle", "Tags"), Body)
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Compute_TagLegendSignature() const -> FString
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return FString{}; }

    const auto* Spec = Mode->Get_SelectedSpec();
    if (Spec == nullptr)
    { return FString{}; }

    auto Sig = FString{};
    for (const auto& Pair : ck::grid_editor::Collect_PerCellTagsWithCounts(Spec))
    { Sig += Pair.Key.GetTagName().ToString() + FString::Printf(TEXT(":%d;"), Pair.Value); }
    return Sig;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Rebuild_TagLegend() -> void
{
    if (! TagLegendContainer.IsValid())
    { return; }

    TagLegendContainer->ClearChildren();

    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    const auto Entries = Spec != nullptr
        ? ck::grid_editor::Collect_PerCellTagsWithCounts(Spec)
        : TArray<TPair<FGameplayTag, int32>>{};

    if (Entries.Num() == 0)
    {
        TagLegendContainer->AddSlot().AutoHeight()
        [
            ck_grid_toolkit::Make_BodyText(LOCTEXT("LegendNone", "No per-cell tags"), CkStyle::TextMute())
        ];
        return;
    }

    for (const auto& Entry : Entries)
    {
        TagLegendContainer->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
        [
            ck_grid_toolkit::Make_TagRow(Entry.Key, Entry.Value, nullptr)
        ];
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Build_BlockerSection() -> TSharedRef<SWidget>
{
    const auto NewBlockerTagCombo = SNew(SGameplayTagCombo)
        .Filter(TEXT("Grid"))
        .Tag(this, &FCk_2dGridSystem_EdModeToolkit::Get_ActiveBlockerTag)
        .OnTagChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_NewBlockerTagChanged);

    const auto SelectedBlockerTagCombo = SNew(SGameplayTagCombo)
        .Filter(TEXT("Grid"))
        .Tag(this, &FCk_2dGridSystem_EdModeToolkit::Get_SelectedBlockerTag)
        .OnTagChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_SelectedBlockerTagChanged);

    auto Body = SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(LOCTEXT("NewBlockerTagLabel", "New Blocker Tag"))
            .ToolTip(LOCTEXT("NewBlockerTagTip", "Stamped onto the next blocker placed with a drag-rect. Leave unset for an anonymous blocker."))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            NewBlockerTagCombo
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, CkStyle::SpaceXS)
        [
            SNew(STextBlock)
            .Text(this, &FCk_2dGridSystem_EdModeToolkit::Get_SelectedBlockerText)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
            .AutoWrapText(true)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBox)
            .Visibility(this, &FCk_2dGridSystem_EdModeToolkit::Get_SelectedBlockerEditorVisibility)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_SectionHeader)
                    .Label(LOCTEXT("SelectedBlockerTagLabel", "Selected Blocker Tag"))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SelectedBlockerTagCombo
                ]
            ]
        ];

    return SNew(SBox)
        .Visibility(this, &FCk_2dGridSystem_EdModeToolkit::Get_BlockerSectionVisibility)
        [
            ck_grid_toolkit::Make_Group(LOCTEXT("BlockerSectionTitle", "Blocker"), Body)
        ];
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_BlockerSectionVisibility() const -> EVisibility
{
    return Get_ActiveTool() == ECk_GridPaint_Tool::Blocker
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_ActiveBlockerTag() const -> FGameplayTag
{
    if (const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { return Mode->Get_ActiveBlockerTag(); }

    return FGameplayTag{};
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_NewBlockerTagChanged(
        FGameplayTag InTag) -> void
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Set_ActiveBlockerTag(InTag); }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_SelectedBlockerTag() const -> FGameplayTag
{
    if (const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { return Mode->Get_SelectedBlockerName(); }

    return FGameplayTag{};
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_SelectedBlockerTagChanged(
        FGameplayTag InTag) -> void
{
    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return; }

    Mode->Set_SelectedBlockerName(InTag);
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_SelectedBlockerEditorVisibility() const -> EVisibility
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return EVisibility::Collapsed; }

    return Mode->Get_SelectedBlockerIndex() != INDEX_NONE
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_SelectedBlockerText() const -> FText
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return LOCTEXT("SelectedBlockerNone", "Selected blocker: none selected"); }

    const auto Index = Mode->Get_SelectedBlockerIndex();

    if (Index == INDEX_NONE)
    { return LOCTEXT("SelectedBlockerNone", "Selected blocker: none selected"); }

    const auto SelectedTag = Mode->Get_SelectedBlockerName();
    if (! SelectedTag.IsValid())
    {
        return FText::Format(LOCTEXT("SelectedBlockerUnnamed", "Selected blocker: #{0} (unnamed)"),
            FText::AsNumber(Index));
    }

    return FText::Format(LOCTEXT("SelectedBlockerNamed", "Selected blocker: #{0} (tag: {1})"),
        FText::AsNumber(Index), FText::FromName(SelectedTag.GetTagName()));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Build_DetailsSection() -> TSharedRef<SWidget>
{
    const auto MakeRow = [](const FText& InLabel, TAttribute<FText> InValue) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_KeyValueRow)
            .KeyText(InLabel)
            .ValueText(InValue)
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::Text());
    };

    // Value is pinned empty so the combo reads as an "add" control: picking a tag writes it straight
    // through to the cell and the chip goes back to empty. The row list is the source of truth.
    const auto AddCellTagCombo = SNew(SGameplayTagCombo)
        .Filter(TEXT("Grid"))
        .Tag(FGameplayTag{})
        .OnTagChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_AddCellTagChanged);

    SAssignNew(PerCellTagListContainer, SVerticalBox);

    const auto DetailsBlockerTagCombo = SNew(SGameplayTagCombo)
        .Filter(TEXT("Grid"))
        .Tag(this, &FCk_2dGridSystem_EdModeToolkit::Get_SelectedBlockerTag)
        .OnTagChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_SelectedBlockerTagChanged);

    Rebuild_PerCellTagList();

    const auto CellEditor = SNew(SBox)
        .Visibility(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsCellEditorVisibility)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 2.0f)
            [
                MakeRow(LOCTEXT("DetailsCoordinate", "Coordinate:"),
                    TAttribute<FText>(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsCoordinateText))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 2.0f)
            [
                SNew(SBox)
                .Visibility(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsStateVisibility)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("DetailsState", "State:"))
                        .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
                        .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Left)
                    [
                        SNew(SCkDebug_StatusPill)
                        .Text(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsStateText)
                        .Tone(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsStateTone)
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceS, 0.0f, CkStyle::SpaceS)
            [
                SNew(SCkDebug_IconToggle)
                .IconId(ECk_Icon::Locked)
                .Label(LOCTEXT("DetailsDisabledToggle", "Disabled"))
                .ShowLabel(true)
                .ToolTip(LOCTEXT("DetailsDisabledToggleTip", "Take this cell out of the grid's footprint — the same state the Shape tool paints."))
                .IsOn(this, &FCk_2dGridSystem_EdModeToolkit::Get_SelectedCellDisabled)
                .OnStateChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_SelectedCellDisabledChanged)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                SNew(SCkDebug_SectionHeader)
                .Label(LOCTEXT("DetailsCellTagsLabel", "Cell Tags"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                PerCellTagListContainer.ToSharedRef()
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                SNew(SCkDebug_SectionHeader)
                .Label(LOCTEXT("DetailsAddCellTagLabel", "Add Cell Tag"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                AddCellTagCombo
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 6.0f, 0.0f, 0.0f)
            [
                MakeRow(LOCTEXT("DetailsGridDefaultTags", "Grid-default tags:"),
                    TAttribute<FText>(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsGridDefaultTagsText))
            ]
        ];

    const auto BlockerEditor = SNew(SBox)
        .Visibility(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsBlockerEditorVisibility)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                .Text(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsBlockerText)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SCkDebug_SectionHeader)
                .Label(LOCTEXT("DetailsBlockerTagLabel", "Blocker Tag"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                DetailsBlockerTagCombo
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
            [
                SNew(SButton)
                .HAlign(HAlign_Center)
                .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_DeleteSelectedBlocker)
                .ToolTipText(LOCTEXT("DetailsDeleteBlockerTip", "Remove this blocker from the grid"))
                [
                    SNew(STextBlock).Text(LOCTEXT("DetailsDeleteBlocker", "Delete Blocker"))
                ]
            ]
        ];

    SAssignNew(BlockerListView, SListView<TSharedPtr<FCk_GridBlockerListItem>>)
        .ListItemsSource(&BlockerListItems)
        .SelectionMode(ESelectionMode::Single)
        .OnGenerateRow(this, &FCk_2dGridSystem_EdModeToolkit::OnGenerate_BlockerRow)
        .OnSelectionChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_BlockerRowSelected);

    SAssignNew(TagListView, SListView<TSharedPtr<FCk_GridTagListItem>>)
        .ListItemsSource(&TagListItems)
        .SelectionMode(ESelectionMode::Single)
        .OnGenerateRow(this, &FCk_2dGridSystem_EdModeToolkit::OnGenerate_TagRow)
        .OnSelectionChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_TagRowSelected);

    Rebuild_SelectLists();

    auto Body = SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(LOCTEXT("BlockersListLabel", "Blockers"))
            .ToolTip(LOCTEXT("BlockersListTip", "Every blocker footprint on this grid. Selecting one highlights it in the viewport."))
            .RightContent()
            [
                SNew(SCkDebug_CountBadge)
                .ValueText(this, &FCk_2dGridSystem_EdModeToolkit::Get_BlockerCountText)
                .ValueColor(CkStyle::TextDim())
                .BackgroundColor(CkStyle::Bg3())
                .BorderColor(CkStyle::Border())
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .MaxHeight(140.0f)
        [
            BlockerListView.ToSharedRef()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(LOCTEXT("TagsListLabel", "Tags"))
            .ToolTip(LOCTEXT("TagsListTip", "Every per-cell tag on this grid. Selecting one highlights the cells carrying it."))
            .RightContent()
            [
                SNew(SCkDebug_CountBadge)
                .ValueText(this, &FCk_2dGridSystem_EdModeToolkit::Get_TagCountText)
                .ValueColor(CkStyle::TextDim())
                .BackgroundColor(CkStyle::Bg3())
                .BorderColor(CkStyle::Border())
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .MaxHeight(140.0f)
        [
            TagListView.ToSharedRef()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            CellEditor
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            Build_MultiCellEditor()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            BlockerEditor
        ];

    return SNew(SBox)
        .Visibility(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsSectionVisibility)
        [
            ck_grid_toolkit::Make_Group(LOCTEXT("DetailsLabel", "Cell Details"), Body)
        ];
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsSectionVisibility() const -> EVisibility
{
    // Repaint-driven refresh (the const-cast-in-getter idiom); the signature guard keeps it cheap.
    if (const auto Sig = Compute_SelectListsSignature(); Sig != SeededSelectListsSignature)
    {
        const_cast<FCk_2dGridSystem_EdModeToolkit*>(this)->SeededSelectListsSignature = Sig;
        const_cast<FCk_2dGridSystem_EdModeToolkit*>(this)->Rebuild_SelectLists();
    }

    return Get_ActiveTool() == ECk_GridPaint_Tool::Select
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Compute_SelectListsSignature() const -> FString
{
    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Spec == nullptr)
    { return FString{}; }

    // Name + range are included because count alone misses an in-place edit via the blocker tag picker.
    auto Sig = FString::Printf(TEXT("B%d|"), Spec->Blockers.Num());
    for (const auto& Blocker : Spec->Blockers)
    {
        Sig += FString::Printf(TEXT("%s@%d,%d:%d,%d;"),
            *Blocker.Name.GetTagName().ToString(),
            Blocker.RangeMin.X, Blocker.RangeMin.Y, Blocker.RangeMax.X, Blocker.RangeMax.Y);
    }
    Sig += TEXT("|");
    for (const auto& Pair : ck::grid_editor::Collect_PerCellTagsWithCounts(Spec))
    { Sig += FString::Printf(TEXT("%s:%d;"), *Pair.Key.GetTagName().ToString(), Pair.Value); }
    return Sig;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Rebuild_SelectLists() -> void
{
    BlockerListItems.Reset();
    TagListItems.Reset();

    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto* Spec = Mode != nullptr ? Mode->Get_SelectedSpec() : nullptr;
    if (Spec != nullptr)
    {
        for (auto Index = 0; Index < Spec->Blockers.Num(); ++Index)
        {
            const auto& B = Spec->Blockers[Index];
            auto Item      = MakeShared<FCk_GridBlockerListItem>();
            Item->Index    = Index;
            Item->Name     = B.Name;
            Item->RangeMin = B.RangeMin;
            Item->RangeMax = B.RangeMax;
            BlockerListItems.Add(Item);
        }

        for (const auto& Pair : ck::grid_editor::Collect_PerCellTagsWithCounts(Spec))
        {
            auto Item       = MakeShared<FCk_GridTagListItem>();
            Item->Tag       = Pair.Key;
            Item->CellCount = Pair.Value;
            TagListItems.Add(Item);
        }
    }

    if (BlockerListView.IsValid())
    { BlockerListView->RequestListRefresh(); }
    if (TagListView.IsValid())
    { TagListView->RequestListRefresh(); }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_BlockerCountText() const -> FText
{
    return FText::AsNumber(BlockerListItems.Num());
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_TagCountText() const -> FText
{
    return FText::AsNumber(TagListItems.Num());
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    OnGenerate_BlockerRow(
        TSharedPtr<FCk_GridBlockerListItem> InItem,
        const TSharedRef<STableViewBase>&   InOwner) -> TSharedRef<ITableRow>
{
    const auto IsAnonymous = ! InItem->Name.IsValid();

    const auto RangeText = FText::FromString(FString::Printf(TEXT("(%d,%d)..(%d,%d)"),
        InItem->RangeMin.X, InItem->RangeMin.Y, InItem->RangeMax.X, InItem->RangeMax.Y));

    // A named blocker gets the same chip treatment as a tag, but tinted with the blocker color the
    // overlay draws its footprint in — not the name's hashed tag color, which is not what the world
    // shows. An anonymous blocker has no name to chip, so it stays muted text.
    auto NameContent = TSharedRef<SWidget>{SNullWidget::NullWidget};
    if (IsAnonymous)
    {
        NameContent = SNew(STextBlock)
            .Text(LOCTEXT("BlockerRowAnonymous", "(unnamed)"))
            .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()));
    }
    else
    {
        NameContent = ck_grid_toolkit::Make_Chip(
            FText::FromName(InItem->Name.GetTagName()), ck::grid_editor::ColorBlocker);
    }

    // Every widget here is click-passive so the STableRow still sees the selection click.
    return SNew(STableRow<TSharedPtr<FCk_GridBlockerListItem>>, InOwner)
        .Padding(FMargin(0.0f, 1.0f))
        .ShowSelection(true)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(SCkDebug_CategoryDot)
            .Color(ck::grid_editor::ColorBlocker)
            .Diameter(10.0f)
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).HAlign(HAlign_Left)
        [
            NameContent
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_CountBadge)
            .ValueText(RangeText)
            .ValueColor(CkStyle::TextMute())
            .BackgroundColor(CkStyle::OverlayOf(ck::grid_editor::ColorBlocker, CkStyle::AlphaFaint()))
            .BorderColor(CkStyle::Border())
        ]
    ];
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    OnGenerate_TagRow(
        TSharedPtr<FCk_GridTagListItem>   InItem,
        const TSharedRef<STableViewBase>& InOwner) -> TSharedRef<ITableRow>
{
    return SNew(STableRow<TSharedPtr<FCk_GridTagListItem>>, InOwner)
        .Padding(FMargin(0.0f, 1.0f))
        .ShowSelection(true)
    [
        ck_grid_toolkit::Make_TagRow(InItem->Tag, InItem->CellCount, nullptr)
    ];
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_BlockerRowSelected(
        TSharedPtr<FCk_GridBlockerListItem> InItem,
        ESelectInfo::Type                   InSelectInfo) -> void
{
    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr || ! InItem.IsValid())
    { return; }

    Mode->Set_SelectedBlockerIndex(InItem->Index);

    if (TagListView.IsValid())
    { TagListView->ClearSelection(); }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_TagRowSelected(
        TSharedPtr<FCk_GridTagListItem> InItem,
        ESelectInfo::Type               InSelectInfo) -> void
{
    auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr || ! InItem.IsValid())
    { return; }

    Mode->Set_SelectedTag(TOptional<FGameplayTag>(InItem->Tag));

    if (BlockerListView.IsValid())
    { BlockerListView->ClearSelection(); }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsCellEditorVisibility() const -> EVisibility
{
    if (Get_ActiveTool() != ECk_GridPaint_Tool::Select)
    { return EVisibility::Collapsed; }

    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return EVisibility::Collapsed; }

    if (Mode->Get_HasBlockerSelection())
    { return EVisibility::Collapsed; }

    return Mode->Get_SelectedCells().Num() > 1
        ? EVisibility::Collapsed
        : EVisibility::Visible;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsMultiCellEditorVisibility() const -> EVisibility
{
    if (Get_ActiveTool() != ECk_GridPaint_Tool::Select)
    { return EVisibility::Collapsed; }

    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return EVisibility::Collapsed; }

    if (Mode->Get_HasBlockerSelection())
    { return EVisibility::Collapsed; }

    return Mode->Get_SelectedCells().Num() > 1
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsBlockerEditorVisibility() const -> EVisibility
{
    if (Get_ActiveTool() != ECk_GridPaint_Tool::Select)
    { return EVisibility::Collapsed; }

    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return EVisibility::Collapsed; }

    return Mode->Get_HasBlockerSelection()
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsCoordinateText() const -> FText
{
    // Live-bound each repaint, so the per-cell tag-list rebuild piggybacks here and the removable rows
    // track live edits without a manual refresh. const_cast: it owns the toolkit's view state.
    {
        const auto Signature = Compute_PerCellTagListSignature();
        if (Signature != SeededPerCellTagSignature)
        {
            auto* MutableThis = const_cast<FCk_2dGridSystem_EdModeToolkit*>(this);
            MutableThis->SeededPerCellTagSignature = Signature;
            MutableThis->Rebuild_PerCellTagList();
        }
    }

    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return LOCTEXT("DetailsNoCell", "Select a cell"); }

    const auto Info = Mode->Resolve_SelectedCellInfo();
    if (! Info.bHasSelection)
    { return LOCTEXT("DetailsNoCell", "Select a cell"); }

    return FText::Format(LOCTEXT("DetailsCoordValue", "({0}, {1})"),
        FText::AsNumber(Info.Coordinate.X), FText::AsNumber(Info.Coordinate.Y));
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsStateText() const -> FText
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return FText::GetEmpty(); }

    const auto Info = Mode->Resolve_SelectedCellInfo();
    if (! Info.bHasSelection)
    { return FText::GetEmpty(); }

    switch (Info.State)
    {
        case UCk_2dGridSystem_EdMode::ECellState::Disabled:
        {
            return LOCTEXT("DetailsStateDisabled", "Disabled");
        }
        case UCk_2dGridSystem_EdMode::ECellState::Blocked:
        {
            const auto TagText = Info.BlockerName.IsValid()
                ? FText::FromName(Info.BlockerName.GetTagName())
                : LOCTEXT("DetailsBlockerUnnamed", "(unnamed)");

            return FText::Format(LOCTEXT("DetailsStateBlocked", "Blocked (blocker #{0}, tag: {1})"),
                FText::AsNumber(Info.BlockerIndex), TagText);
        }
        default:
        {
            return LOCTEXT("DetailsStateEnabled", "Enabled");
        }
    }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsStateTone() const -> ECk_Tone
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return ECk_Tone::Neutral; }

    const auto Info = Mode->Resolve_SelectedCellInfo();
    if (! Info.bHasSelection)
    { return ECk_Tone::Neutral; }

    switch (Info.State)
    {
        case UCk_2dGridSystem_EdMode::ECellState::Disabled:
        {
            return ECk_Tone::Err;
        }
        case UCk_2dGridSystem_EdMode::ECellState::Blocked:
        {
            return ECk_Tone::Warn;
        }
        default:
        {
            return ECk_Tone::Ok;
        }
    }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsStateVisibility() const -> EVisibility
{
    // Get_DetailsStateText is empty with no cell picked, and an empty pill still paints its fill.
    return Get_DetailsStateText().IsEmpty()
        ? EVisibility::Collapsed
        : EVisibility::Visible;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_SelectedCellDisabled() const -> bool
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return false; }

    return Mode->Get_SelectedCellDisabled();
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_SelectedCellDisabledChanged(
        bool InIsDisabled) -> void
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Set_SelectedCellDisabled(InIsDisabled); }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Compute_PerCellTagListSignature() const -> FString
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return FString{}; }

    const auto Info = Mode->Resolve_SelectedCellInfo();
    if (! Info.bHasSelection)
    { return FString{}; }

    // A blocker selection collapses the cell editor, so blocker state is deliberately not in the signature.
    return FString::Printf(TEXT("%d,%d:%s"),
        Info.Coordinate.X, Info.Coordinate.Y, *Info.CellTags.ToStringSimple());
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Rebuild_PerCellTagList() -> void
{
    if (! PerCellTagListContainer.IsValid())
    { return; }

    PerCellTagListContainer->ClearChildren();

    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto Tags  = (Mode != nullptr) ? Mode->Get_SelectedCellTags() : FGameplayTagContainer{};

    if (Tags.IsEmpty())
    {
        PerCellTagListContainer->AddSlot()
        .AutoHeight()
        [
            ck_grid_toolkit::Make_BodyText(LOCTEXT("DetailsTagsNone", "No tags on this cell"), CkStyle::TextMute())
        ];
        return;
    }

    for (const auto& Tag : Tags)
    {
        const auto RemoveButton = SNew(SCkDebug_IconButton)
            .IconId(ECk_Icon::Delete)
            .Label(LOCTEXT("DetailsRemoveCellTagTip", "Remove this tag from the cell"))
            .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_RemoveCellTag, Tag);

        PerCellTagListContainer->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 1.0f)
        [
            ck_grid_toolkit::Make_TagRow(Tag, TOptional<int32>{}, RemoveButton)
        ];
    }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_AddCellTagChanged(
        FGameplayTag InTag) -> void
{
    if (! InTag.IsValid())
    { return; }

    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Add_SelectedCellTag(InTag); }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_RemoveCellTag(
        FGameplayTag InTag) -> FReply
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Remove_SelectedCellTag(InTag); }

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Build_MultiCellEditor() -> TSharedRef<SWidget>
{
    const auto BulkAddCombo = SNew(SGameplayTagCombo)
        .Filter(TEXT("Grid"))
        .Tag(this, &FCk_2dGridSystem_EdModeToolkit::Get_BulkAddTag)
        .OnTagChanged(this, &FCk_2dGridSystem_EdModeToolkit::On_BulkAddTagChanged);

    SAssignNew(MultiCellTagListContainer, SVerticalBox);
    Rebuild_MultiCellTagList();

    return SNew(SBox)
        .Visibility(this, &FCk_2dGridSystem_EdModeToolkit::Get_DetailsMultiCellEditorVisibility)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Left)
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SNew(SCkDebug_StatusPill)
                .Text(this, &FCk_2dGridSystem_EdModeToolkit::Get_MultiCellSummaryText)
                .Tone(ECk_Tone::Info)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SCkDebug_SectionHeader)
                .Label(LOCTEXT("MultiCellTagsLabel", "Tags In Selection"))
                .ToolTip(LOCTEXT("MultiCellTagsTip", "The union of the selection's per-cell tags, with how many selected cells carry each."))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                MultiCellTagListContainer.ToSharedRef()
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                SNew(SCkDebug_SectionHeader)
                .Label(LOCTEXT("MultiCellAddTagLabel", "Add Tag To Selection"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    BulkAddCombo
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .HAlign(HAlign_Center)
                    .IsEnabled(this, &FCk_2dGridSystem_EdModeToolkit::Get_BulkAddButtonEnabled)
                    .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_AddTagToSelection)
                    .ToolTipText(LOCTEXT("MultiCellAddTagTip", "Add the chosen tag to every selected cell"))
                    [
                        SNew(STextBlock).Text(LOCTEXT("MultiCellAddTag", "Add"))
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
            [
                SNew(SUniformGridPanel)
                .SlotPadding(FMargin(CkStyle::SpaceXS))
                + SUniformGridPanel::Slot(0, 0)
                [
                    SNew(SButton)
                    .HAlign(HAlign_Center)
                    .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_DisableSelection)
                    .ToolTipText(LOCTEXT("MultiCellDisableTip", "Mark every selected cell disabled"))
                    [
                        SNew(STextBlock).Text(LOCTEXT("MultiCellDisable", "Disable All"))
                    ]
                ]
                + SUniformGridPanel::Slot(1, 0)
                [
                    SNew(SButton)
                    .HAlign(HAlign_Center)
                    .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_EnableSelection)
                    .ToolTipText(LOCTEXT("MultiCellEnableTip", "Clear the disabled state on every selected cell"))
                    [
                        SNew(STextBlock).Text(LOCTEXT("MultiCellEnable", "Enable All"))
                    ]
                ]
            ]
        ];
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_MultiCellSummaryText() const -> FText
{
    // Live-bound each repaint, so the tag-union rebuild piggybacks here; the single-cell editor's driver is
    // collapsed whenever this one is visible. const_cast: it owns the toolkit's view state.
    {
        const auto Signature = Compute_MultiCellTagListSignature();
        if (Signature != SeededMultiCellTagSignature)
        {
            auto* MutableThis = const_cast<FCk_2dGridSystem_EdModeToolkit*>(this);
            MutableThis->SeededMultiCellTagSignature = Signature;
            MutableThis->Rebuild_MultiCellTagList();
        }
    }

    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return FText::GetEmpty(); }

    return FText::Format(LOCTEXT("MultiCellSummary", "{0} cells selected"),
        FText::AsNumber(Mode->Get_SelectedCells().Num()));
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Compute_MultiCellTagListSignature() const -> FString
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return FString{}; }

    auto Sig = FString::Printf(TEXT("N%d|"), Mode->Get_SelectedCells().Num());
    for (const auto& Pair : Mode->Collect_SelectedCellsTagCounts())
    { Sig += FString::Printf(TEXT("%s:%d;"), *Pair.Key.GetTagName().ToString(), Pair.Value); }
    return Sig;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Rebuild_MultiCellTagList() -> void
{
    if (! MultiCellTagListContainer.IsValid())
    { return; }

    MultiCellTagListContainer->ClearChildren();

    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    const auto Entries = Mode != nullptr
        ? Mode->Collect_SelectedCellsTagCounts()
        : TArray<TPair<FGameplayTag, int32>>{};

    if (Entries.IsEmpty())
    {
        MultiCellTagListContainer->AddSlot()
        .AutoHeight()
        [
            ck_grid_toolkit::Make_BodyText(LOCTEXT("MultiCellTagsNone", "No tags on the selected cells"), CkStyle::TextMute())
        ];
        return;
    }

    for (const auto& Entry : Entries)
    {
        const auto RemoveButton = SNew(SCkDebug_IconButton)
            .IconId(ECk_Icon::Delete)
            .Label(LOCTEXT("MultiCellRemoveTagTip", "Remove this tag from every selected cell"))
            .OnClicked(this, &FCk_2dGridSystem_EdModeToolkit::On_RemoveTagFromSelection, Entry.Key);

        MultiCellTagListContainer->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 1.0f)
        [
            ck_grid_toolkit::Make_TagRow(Entry.Key, Entry.Value, RemoveButton)
        ];
    }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_RemoveTagFromSelection(
        FGameplayTag InTag) -> FReply
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Remove_SelectedCellsTag(InTag); }

    return FReply::Handled();
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_BulkAddTag() const -> FGameplayTag
{
    return BulkAddTag;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_BulkAddTagChanged(
        FGameplayTag InTag) -> void
{
    BulkAddTag = InTag;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_BulkAddButtonEnabled() const -> bool
{
    return BulkAddTag.IsValid();
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_AddTagToSelection() -> FReply
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Add_SelectedCellsTag(BulkAddTag); }

    return FReply::Handled();
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_DisableSelection() -> FReply
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Set_SelectedCellsDisabled(true); }

    return FReply::Handled();
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_EnableSelection() -> FReply
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Set_SelectedCellsDisabled(false); }

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsBlockerText() const -> FText
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return FText::GetEmpty(); }

    const auto Index = Mode->Get_SelectedBlockerIndex();
    if (Index == INDEX_NONE)
    { return FText::GetEmpty(); }

    auto Min = FIntPoint::ZeroValue;
    auto Max = FIntPoint::ZeroValue;
    if (! Mode->Get_SelectedBlockerRange(Min, Max))
    { return FText::GetEmpty(); }

    const auto SelectedTag = Mode->Get_SelectedBlockerName();
    const auto TagText = SelectedTag.IsValid()
        ? FText::FromName(SelectedTag.GetTagName())
        : LOCTEXT("DetailsBlockerUnnamed2", "(unnamed)");

    return FText::Format(
        LOCTEXT("DetailsBlockerInfo", "Blocker #{0}  range ({1},{2})..({3},{4})  tag: {5}"),
        FText::AsNumber(Index),
        FText::AsNumber(Min.X), FText::AsNumber(Min.Y),
        FText::AsNumber(Max.X), FText::AsNumber(Max.Y),
        TagText);
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_DeleteSelectedBlocker() -> FReply
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Delete_SelectedBlocker(); }

    return FReply::Handled();
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_DetailsGridDefaultTagsText() const -> FText
{
    const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get());
    if (Mode == nullptr)
    { return FText::GetEmpty(); }

    const auto Info = Mode->Resolve_SelectedCellInfo();
    if (! Info.bHasSelection)
    { return FText::GetEmpty(); }

    if (Info.GridDefaultTags.IsEmpty())
    { return LOCTEXT("DetailsTagsNone", "(none)"); }

    return FText::FromString(Info.GridDefaultTags.ToStringSimple());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Build_ControlsSection() -> TSharedRef<SWidget>
{
    const auto MakeBlock = [](TAttribute<EVisibility> InVisibility, TArray<TPair<FText, FText>> InHints) -> TSharedRef<SWidget>
    {
        auto Rows = SNew(SVerticalBox);
        for (const auto& Hint : InHints)
        {
            Rows->AddSlot()
                .AutoHeight()
                .Padding(0.0f, 1.0f)
                [
                    ck_grid_toolkit::Make_HintRow(Hint.Key, Hint.Value)
                ];
        }

        return SNew(SBox)
            .Visibility(InVisibility)
            [
                Rows
            ];
    };

    const auto GestureLmb      = LOCTEXT("HintLmb", "LMB");
    const auto GestureShiftLmb = LOCTEXT("HintShiftLmb", "Shift+LMB");
    const auto GestureLmbClick = LOCTEXT("HintLmbClick", "LMB click");
    const auto GestureLmbDrag  = LOCTEXT("HintLmbDrag", "LMB drag");

    // Every string below is the behaviour the EdMode actually implements: Shape/Tags paint through
    // Paint_Cell (plain adds, Shift erases), Blocker places on a drag and picks on a click, Select picks a
    // cell or its covering blocker on a click and marquees on a drag.
    const auto ShapeBlock = MakeBlock(
        TAttribute<EVisibility>(this, &FCk_2dGridSystem_EdModeToolkit::Get_ShapeHintsVisibility),
        {
            { GestureLmb,      LOCTEXT("HintShapePaint", "Click a cell or drag a rect to disable those cells") },
            { GestureShiftLmb, LOCTEXT("HintShapeErase", "Erase — re-enable the clicked or dragged cells") },
        });

    const auto TagsBlock = MakeBlock(
        TAttribute<EVisibility>(this, &FCk_2dGridSystem_EdModeToolkit::Get_TagsHintsVisibility),
        {
            { GestureLmb,      LOCTEXT("HintTagsPaint", "Click a cell or drag a rect to paint the active tag") },
            { GestureShiftLmb, LOCTEXT("HintTagsErase", "Erase — remove the active tag from those cells") },
        });

    const auto BlockerBlock = MakeBlock(
        TAttribute<EVisibility>(this, &FCk_2dGridSystem_EdModeToolkit::Get_BlockerHintsVisibility),
        {
            { GestureLmbDrag,  LOCTEXT("HintBlockerPlace", "Place a blocker over the dragged rect") },
            { GestureLmbClick, LOCTEXT("HintBlockerPick", "Select the blocker covering the clicked cell") },
            { LOCTEXT("HintDelete", "Delete"), LOCTEXT("HintBlockerDelete", "Remove the selected blocker") },
        });

    const auto SelectBlock = MakeBlock(
        TAttribute<EVisibility>(this, &FCk_2dGridSystem_EdModeToolkit::Get_SelectHintsVisibility),
        {
            { GestureLmbClick, LOCTEXT("HintSelectCell", "Select a cell, or the blocker covering it") },
            { GestureLmbDrag,  LOCTEXT("HintSelectMarquee", "Marquee-select every cell in the rect") },
        });

    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(LOCTEXT("ControlsLabel", "Controls"))
            .ToolTip(LOCTEXT("ControlsTip", "Viewport gestures the active tool answers to."))
        ]
        + SVerticalBox::Slot().AutoHeight() [ ShapeBlock ]
        + SVerticalBox::Slot().AutoHeight() [ TagsBlock ]
        + SVerticalBox::Slot().AutoHeight() [ BlockerBlock ]
        + SVerticalBox::Slot().AutoHeight() [ SelectBlock ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 1.0f)
        [
            ck_grid_toolkit::Make_HintRow(
                LOCTEXT("HintCameraKeys", "Ctrl/Alt/RMB"),
                LOCTEXT("HintCameraNav", "Camera navigation — these never paint or pick"))
        ];
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_ShapeHintsVisibility() const -> EVisibility
{
    return Get_ActiveTool() == ECk_GridPaint_Tool::Shape ? EVisibility::Visible : EVisibility::Collapsed;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_TagsHintsVisibility() const -> EVisibility
{
    return Get_ActiveTool() == ECk_GridPaint_Tool::Tags ? EVisibility::Visible : EVisibility::Collapsed;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_BlockerHintsVisibility() const -> EVisibility
{
    return Get_ActiveTool() == ECk_GridPaint_Tool::Blocker ? EVisibility::Visible : EVisibility::Collapsed;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_SelectHintsVisibility() const -> EVisibility
{
    return Get_ActiveTool() == ECk_GridPaint_Tool::Select ? EVisibility::Visible : EVisibility::Collapsed;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Set_ActiveTool(
        ECk_GridPaint_Tool InTool) -> void
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Set_ActiveTool(InTool); }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_ActiveTool() const -> ECk_GridPaint_Tool
{
    if (const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { return Mode->Get_ActiveTool(); }

    return ECk_GridPaint_Tool::Shape;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_TagsSectionVisibility() const -> EVisibility
{
    // Repaint-driven refresh (the const-cast-in-getter idiom); the signature guard keeps it cheap.
    if (const auto Sig = Compute_TagLegendSignature(); Sig != SeededTagLegendSignature)
    {
        const_cast<FCk_2dGridSystem_EdModeToolkit*>(this)->SeededTagLegendSignature = Sig;
        const_cast<FCk_2dGridSystem_EdModeToolkit*>(this)->Rebuild_TagLegend();
    }

    return Get_ActiveTool() == ECk_GridPaint_Tool::Tags
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_ActivePaintTag() const -> FGameplayTag
{
    if (const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { return Mode->Get_ActivePaintTag(); }

    return FGameplayTag{};
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_PaintTagChanged(
        FGameplayTag InTag) -> void
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Set_ActivePaintTag(InTag); }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_ActiveTagScope() const -> ECk_GridPaint_TagScope
{
    if (const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { return Mode->Get_TagScope(); }

    return ECk_GridPaint_TagScope::PerCellBulk;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_ActiveTagScopeChanged(
        ECk_GridPaint_TagScope InScope) -> void
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Set_TagScope(InScope); }
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    Get_GridDefaultButtonsEnabled() const -> bool
{
    if (const auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { return Mode->Get_TagScope() == ECk_GridPaint_TagScope::GridDefault; }

    return false;
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_ApplyGridDefaultTag() -> FReply
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Apply_GridDefaultTag(); }

    return FReply::Handled();
}

auto
    FCk_2dGridSystem_EdModeToolkit::
    On_RemoveGridDefaultTag() -> FReply
{
    if (auto* Mode = Cast<UCk_2dGridSystem_EdMode>(OwningMode.Get()))
    { Mode->Remove_GridDefaultTag(); }

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

void
    FCk_2dGridSystem_EdModeToolkit::
    GetToolPaletteNames(
        TArray<FName>& OutPaletteNames) const
{
    OutPaletteNames.Add(NAME_Default);
}

FName
    FCk_2dGridSystem_EdModeToolkit::
    GetToolkitFName() const
{
    return FName("Ck_2dGridSystem_EdMode");
}

FText
    FCk_2dGridSystem_EdModeToolkit::
    GetBaseToolkitName() const
{
    return LOCTEXT("DisplayName", "Grid Paint");
}

TSharedPtr<SWidget>
    FCk_2dGridSystem_EdModeToolkit::
    GetInlineContent() const
{
    return InlineContent;
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
