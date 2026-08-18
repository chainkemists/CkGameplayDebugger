#include "CkSaveDebugger/Window/SCkSaveDebuggerWindow.h"

#include "CkCore/EditorOnly/CkEditorOnly_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/MessageDialog/CkMessageDialog_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Snapshot/CkSaveKey_Fragment.h"

#include "CkSnapshot/SaveGame/CkSnapshot_SlotMeta.h" // Decode_PngAsTexture — the sidecar thumbnail

#include "CkSaveDebugger/Visualizer/CkSaveDebugger_Visualizer.h"
#include "CkSaveDebugger/Visualizer/CkSaveDebugger_VisualizerRetained.h"

#include "CkSnapshot/Inspection/CkSnapshot_Inspection.h"
#include "CkSnapshot/Inspection/CkSnapshot_Inspection_Diff.h"
#include "CkSnapshot/Inspection/CkSnapshot_Inspection_Sha256.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CopyableContainer.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkEditorTools/Style/CkIconStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameDepthCycler.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include <DesktopPlatformModule.h>
#include <Engine/World.h>
#include <Framework/Application/SlateApplication.h>
#include <Framework/MultiBox/MultiBoxBuilder.h>
#include <HAL/PlatformApplicationMisc.h>
#include <Misc/FileHelper.h>
#include <Misc/PackageName.h>
#include <Misc/Paths.h>

#if WITH_EDITOR
#include <Editor.h>
#include <EngineUtils.h>
#include <FileHelpers.h>
#include <GameFramework/Actor.h>
#include <Selection.h>
#endif
#include <Widgets/Images/SImage.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SSplitter.h>
#include <Widgets/Layout/SWidgetSwitcher.h>
#include <Widgets/Layout/SWrapBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Views/STableRow.h>

// --------------------------------------------------------------------------------------------------------------------

const FName SCkSaveDebuggerWindow::WindowId = FName(TEXT("CkSaveDebugger"));

// --------------------------------------------------------------------------------------------------------------------

namespace ck_save_debugger_window
{
    constexpr auto k_ProblemDotSize   = 8.0f;
    constexpr auto k_ProvenanceWidth  = 130.0f;
    constexpr auto k_SeverityWidth    = 70.0f;
    constexpr auto k_CodeWidth        = 220.0f;
    constexpr auto k_ByteColumnWidth  = 90.0f;
    constexpr auto k_DiffKindWidth    = 120.0f;
    constexpr auto k_DiffCountWidth   = 110.0f;
    constexpr auto k_DiffBytesWidth   = 140.0f;
    constexpr auto k_HexPanelHeight   = 130.0f;
    constexpr auto k_PanelIconSize    = 14.0f;
    constexpr auto k_RowIconSize      = 12.0f;

    const auto k_ProvenanceOrder = TArray<ECk_Snapshot_V3_Provenance>
    {
        ECk_Snapshot_V3_Provenance::EngineOwned,
        ECk_Snapshot_V3_Provenance::ConstructSpawned,
        ECk_Snapshot_V3_Provenance::RuntimeSpawned,
        ECk_Snapshot_V3_Provenance::DefinitionBuilt,
    };

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_RowTextColor(
            bool InIsSearchMatch)
        -> FSlateColor
    {
        return FSlateColor{InIsSearchMatch ? CkStyle::Text() : CkStyle::TextMute()};
    }

    // ----------------------------------------------------------------------------------------------------------------

    // The suite's translucent selection style, not the ENGINE "TableView.Row" — the engine brush is a saturated fill
    // that drowns the glyphs and pills these rows are built from.
    auto
        Get_RowStyle()
        -> const FTableRowStyle&
    {
        return FCkDebuggerStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("CkDebugger.TableView.Row"));
    }

    auto
        Get_IconBrush(
            ECk_Icon InIcon)
        -> const FSlateBrush*
    {
        return FCkIconStyle::Get_Brush(InIcon, ECk_Icon_BrushSize::Size_16x16);
    }

    auto
        Get_BadgeBrush()
        -> const FSlateBrush*
    {
        return FCkDebuggerStyle::Get().GetBrush(TEXT("CkDebugger.Badge.Rounded"));
    }

    auto
        Get_PanelBrush()
        -> const FSlateBrush*
    {
        return FCkDebuggerStyle::Get().GetBrush(TEXT("CkDebugger.Panel.Border"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ProvenanceCount(
            const FCk_SnapshotInspection_Census& InCensus,
            ECk_Snapshot_V3_Provenance InProvenance)
        -> int32
    {
        switch (InProvenance)
        {
            case ECk_Snapshot_V3_Provenance::EngineOwned:      return InCensus.Get_EngineOwnedCount();
            case ECk_Snapshot_V3_Provenance::ConstructSpawned: return InCensus.Get_ConstructSpawnedCount();
            case ECk_Snapshot_V3_Provenance::RuntimeSpawned:   return InCensus.Get_RuntimeSpawnedCount();
            case ECk_Snapshot_V3_Provenance::DefinitionBuilt:  return InCensus.Get_DefinitionBuiltCount();
            default:                                           return 0;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** A glyph-led command button. The wrapper already carries the explanation, so the glyph deliberately goes
     *  without a Meaning of its own — one surface, one tooltip. */
    auto
        Build_CommandButton(
            ECk_Icon InIconId,
            const FString& InLabel,
            const FString& InTooltip,
            FOnClicked InOnClicked,
            TAttribute<bool> InIsEnabled)
        -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ToolTipText(FText::FromString(InTooltip))
            .IsEnabled(InIsEnabled)
            .OnClicked(InOnClicked)
            .ContentPadding(FMargin{CkStyle::SpaceM, CkStyle::SpaceXS})
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(Get_IconBrush(InIconId))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    .Size(FVector2D{k_PanelIconSize, k_PanelIconSize})
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(InLabel))
                ]
            ];
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_EmptyState(
            ECk_Icon InIconId,
            const FString& InText)
        -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_Card)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(Get_IconBrush(InIconId))
                    .Meaning(FText::FromString(InText))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    .Size(FVector2D{k_PanelIconSize, k_PanelIconSize})
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(InText))
                    .AutoWrapText(true)
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ];
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** A count tile for the summary strip. Zero-valued Err/Warn tiles stay muted so a clean save does not paint red
     *  and amber at the reader. */
    auto
        Build_CountTile(
            const FString& InLabel,
            int32 InCount,
            ECk_Tone InTone)
        -> TSharedRef<SWidget>
    {
        const auto Color = InCount > 0 ? CkStyle::GetToneColor(InTone) : CkStyle::TextMute();

        return SNew(SCkDebug_StatPair)
            .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
            .Value(FText::FromString(ck::Format_UE(TEXT("{}"), InCount)))
            .Label(FText::FromString(InLabel))
            .ValueColor(FSlateColor{Color});
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_TransformText(
            const FTransform& InTransform)
        -> FString
    {
        const auto Location = InTransform.GetLocation();
        const auto Rotation = InTransform.Rotator();
        const auto Scale = InTransform.GetScale3D();

        return ck::Format_UE(TEXT("T({}, {}, {}) R({}, {}, {}) S({}, {}, {})"),
            Location.X, Location.Y, Location.Z,
            Rotation.Pitch, Rotation.Yaw, Rotation.Roll,
            Scale.X, Scale.Y, Scale.Z);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkSaveDebugger"))
        .CommandGroups({
            FCkDebug_CommandGroup::Primary(
                TEXT("SaveActions"),
                FText::FromString(TEXT("Save actions")),
                DoCreate_MenuActions()),
            FCkDebug_CommandGroup::Context(
                TEXT("File"),
                FText::FromString(TEXT("Save file commands")),
                DoCreate_FileControls()),
            FCkDebug_CommandGroup::Context(
                TEXT("Visualization"),
                FText::FromString(TEXT("Save visualization commands")),
                DoCreate_VisualizationControls()),
            FCkDebug_CommandGroup::Context(
                TEXT("Export"),
                FText::FromString(TEXT("Save export commands")),
                DoCreate_ExportControls()),
            FCkDebug_CommandGroup::Context(
                TEXT("FileStatus"),
                FText::FromString(TEXT("Open save status")),
                DoCreate_FileStatus())})
        .Content()
        [
            DoCreate_Body()
        ]
        .StatusContent()
        [
            DoCreate_Status()
        ]
    ];

    Register_WithGate();

#if WITH_EDITOR
    // AddSP self-unbinds when the widget dies; the explicit RemoveAll in the destructor is just determinism.
    USelection::SelectionChangedEvent.AddSP(this, &SCkSaveDebuggerWindow::DoOnEditorSelectionChanged);
    FEditorDelegates::OnMapOpened.AddSP(this, &SCkSaveDebuggerWindow::DoOnMapOpened);
#endif
}

// --------------------------------------------------------------------------------------------------------------------

SCkSaveDebuggerWindow::~SCkSaveDebuggerWindow()
{
#if WITH_EDITOR
    USelection::SelectionChangedEvent.RemoveAll(this);
    FEditorDelegates::OnMapOpened.RemoveAll(this);

    // On engine exit the level-editor mode stack is already tearing down — the auto-discovered EdMode is
    // unregistered by UAssetEditorSubsystem itself, so only the published state needs dropping.
    if (NOT IsEngineExitRequested())
    {
        ck::save_debugger_viz::Set_VisualizerEnabled(false);
        ck::save_debugger_viz_retained::Clear();
    }

    ck::save_debugger_viz::Unregister_OnRowClicked();
    ck::save_debugger_viz::Clear_Rows();
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    DoRebuild_Summary();
    DoRebuild_EntityDetail();
    DoRebuild_BlobDetail();
    DoRebuild_DiffDetail();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoCreate_MenuActions()
    -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_IconToggle)
        .IconId(ECk_Icon::Diagnostics)
        .Label(FText::FromString(TEXT("Problems only")))
        .ToolTip(FText::FromString(TEXT("Show only entities an Error or Warning diagnostic names.")))
        .IsOn_Lambda([this]() -> bool { return _Model.Get_ProblemsOnly(); })
        .OnStateChanged_Lambda([this](const bool InProblemsOnly)
        {
            if (_Model.Get_ProblemsOnly() == InProblemsOnly)
            { return; }

            _Model.Set_ProblemsOnly(InProblemsOnly);
            DoRefresh_Filters();
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoCreate_FileControls()
    -> TSharedRef<SWidget>
{
    using namespace ck_save_debugger_window;

    const auto HasFile = TAttribute<bool>::CreateLambda([this]() -> bool { return NOT _CurrentPath.IsEmpty(); });

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            Build_CommandButton(ECk_Icon::Storage,
                TEXT("Open Save..."),
                TEXT("Open a CK .sav snapshot file for offline inspection"),
                FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnOpenClicked),
                true)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            Build_CommandButton(ECk_Icon::Media,
                TEXT("Reload"),
                TEXT("Re-read the current file from disk"),
                FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnReloadClicked),
                HasFile)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f)
        [
            Build_CommandButton(ECk_Icon::Size,
                TEXT("Compare Against..."),
                TEXT("Pick an OLDER save as the baseline and diff the open file against it. The open file is the current side"),
                FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnCompareAgainstClicked),
                HasFile)
        ];
}

auto
    SCkSaveDebuggerWindow::
    DoCreate_VisualizationControls()
    -> TSharedRef<SWidget>
{
#if WITH_EDITOR
    using namespace ck_save_debugger_window;
    const auto HasFile = TAttribute<bool>::CreateLambda([this]() -> bool { return NOT _CurrentPath.IsEmpty(); });

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SButton)
            .ToolTipText(FText::FromString(TEXT(
                "Draw every placed entity of this save as a diamond in the level-editor viewport, with owner-chain "
                "lines. Click a diamond to select its row here; prompts to load the captured level when a different "
                "one is open")))
            .IsEnabled(HasFile)
            .OnClicked(FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnVisualizeClicked))
            .ContentPadding(FMargin{CkStyle::SpaceM, CkStyle::SpaceXS})
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(Get_IconBrush(ECk_Icon::Pin))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    .Size(FVector2D{k_PanelIconSize, k_PanelIconSize})
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([]() -> FText
                    {
                        return FText::FromString(ck::save_debugger_viz::Get_IsVisualizerEnabled()
                            ? TEXT("Stop Visualizing")
                            : TEXT("Visualize"));
                    })
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            Build_CommandButton(ECk_Icon::Aim,
                TEXT("Frame"),
                TEXT("Move the level-editor camera to the selected entity's diamond"),
                FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnFrameSelectedClicked),
                TAttribute<bool>::CreateLambda([]() -> bool
                {
                    return ck::save_debugger_viz::Get_IsVisualizerEnabled();
                }))
        ];
#else
    return SNew(SBox);
#endif
}

auto
    SCkSaveDebuggerWindow::
    DoCreate_ExportControls()
    -> TSharedRef<SWidget>
{
    using namespace ck_save_debugger_window;

    const auto HasFile = TAttribute<bool>::CreateLambda([this]() -> bool { return NOT _CurrentPath.IsEmpty(); });

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            Build_CommandButton(ECk_Icon::Log,
                TEXT("Export JSON..."),
                TEXT("Write the whole inspection as a deterministic JSON document (hashes always present, no raw blob bytes)"),
                FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnExportJsonClicked),
                HasFile)
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            Build_CommandButton(ECk_Icon::Report,
                TEXT("Copy Report"),
                TEXT("Copy the census and diagnostics to the clipboard"),
                FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnCopyReportClicked),
                HasFile)
        ];
}

auto
    SCkSaveDebuggerWindow::
    DoCreate_FileStatus()
    -> TSharedRef<SWidget>
{
    using namespace ck_save_debugger_window;

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCkDebug_Icon)
            .Brush(Get_IconBrush(ECk_Icon::SaveSlot))
            .Meaning(FText::FromString(TEXT("The .sav file this window currently has open")))
            .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
            .Size(FVector2D{k_PanelIconSize, k_PanelIconSize})
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SBox)
            .MaxDesiredWidth(280.0f)
            .Clipping(EWidgetClipping::ClipToBounds)
            [
                SAssignNew(_PathLabel, SCkDebug_SelectableLabel)
                .Text(FText::FromString(TEXT("(no file open)")))
                .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCkDebug_NameDepthCycler)
            .Depth_Lambda([this]() -> int32 { return _NameDepth; })
            .MaxDepth_Lambda([this]() -> int32 { return _MaxNameDepth; })
            .OnDepthChanged_Lambda([this](const int32 InDepth)
            {
                if (_NameDepth == InDepth)
                { return; }

                _NameDepth = InDepth;

                // Type paths appear in the payload list AND in the entity recipe rows, and only the latter is a
                // rebuilt panel — the list's texts are attribute-bound and follow on their own.
                DoRebuild_EntityDetail();
                DoRebuild_BlobDetail();
            })
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SCkDebug_StatusPill)
            .Text_Lambda([this]() -> FText
            {
                return FText::FromString(ck::Format_UE(TEXT("{} / {}"),
                    ck::snapshot::Get_ReadStatusText(_Model.Get_Document().Get_ReadStatus()),
                    ck::snapshot::Get_CompatibilityText(_Model.Get_Document().Get_Compatibility())));
            })
            .Tone_Lambda([this]() -> ECk_Tone
            {
                return ck_save_debugger_model::Get_CompatibilityTone(_Model.Get_Document());
            })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoCreate_ProvenanceChips()
    -> TSharedRef<SWidget>
{
    using namespace ck_save_debugger_window;

    auto WrapBox = SNew(SWrapBox)
        .UseAllottedSize(true);

    for (const auto Provenance : k_ProvenanceOrder)
    {
        const auto Label = FString{ck::snapshot::Get_ProvenanceText(Provenance)};
        const auto IconBrush = Get_IconBrush(ck_save_debugger_model::Get_ProvenanceIcon(Provenance));

        // The chip's on/off state drives its own tint, so the glyph and the label read the mask rather than
        // repeating it in a second colour.
        const auto ChipColor = TAttribute<FSlateColor>::CreateLambda([this, Provenance]() -> FSlateColor
        {
            const auto IsOn = ck_save_debugger_model::Passes_ProvenanceMask(Provenance, _Model.Get_ProvenanceMask());
            return FSlateColor{IsOn ? CkStyle::Text() : CkStyle::TextMute()};
        });

        WrapBox->AddSlot()
        .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS})
        [
            SNew(SCkDebug_ToggleSurface)
            .AccessibleText(FText::FromString(Label))
            .ToolTipText(FText::FromString(ck::Format_UE(TEXT("Show entities captured as {}"), Label)))
            .IsOn_Lambda([this, Provenance]() -> bool
            {
                return ck_save_debugger_model::Passes_ProvenanceMask(Provenance, _Model.Get_ProvenanceMask());
            })
            .OnStateChanged_Lambda([this, Provenance](const bool InIsOn)
            {
                const auto Bit = ck_save_debugger_model::Get_ProvenanceBit(Provenance);
                const auto Mask = _Model.Get_ProvenanceMask();
                const auto NewMask = static_cast<uint8>(InIsOn ? (Mask | Bit) : (Mask & ~Bit));

                if (NewMask == Mask)
                { return; }

                _Model.Set_ProvenanceMask(NewMask);
                DoRefresh_Filters();
            })
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    // No Meaning: the toggle surface already carries the richer explanation.
                    SNew(SCkDebug_Icon)
                    .Brush(IconBrush)
                    .ColorAndOpacity(ChipColor)
                    .Size(FVector2D{k_RowIconSize, k_RowIconSize})
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Label))
                    .ColorAndOpacity(ChipColor)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_CountBadge)
                    .ValueText_Lambda([this, Provenance]() -> FText
                    {
                        return FText::FromString(ck::Format_UE(TEXT("{}"),
                            Get_ProvenanceCount(_Model.Get_Document().Get_Census(), Provenance)));
                    })
                    .ValueColor(CkStyle::Text())
                    .SuffixColor(CkStyle::TextMute())
                    .BackgroundColor(CkStyle::Bg2())
                    .BorderColor(CkStyle::Border())
                ]
            ]
        ];
    }

    return WrapBox;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoCreate_Body()
    -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
        [
            SAssignNew(_SummaryBox, SVerticalBox)
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            // Vertical splitter, not fixed 0.72/0.28 weights: how much height diagnostics deserve depends
            // entirely on the file — a clean save wants none, a broken one wants most of the window.
            SNew(SSplitter)
            .Orientation(Orient_Vertical)

            + SSplitter::Slot()
            .Value(0.72f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal)

                + SSplitter::Slot()
                .Value(0.32f)
                [
                    SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(CkStyle::SpaceS)
                [
                    SNew(SCkDebug_DualSearchBar)
                    .FilterHintText(FText::FromString(TEXT("Filter entities...")))
                    .HighlightHintText(FText::FromString(TEXT("Highlight...")))
                    .OnFilterTextChanged_Lambda([this](const FString& InText)
                    {
                        if (_Model.Get_FilterString() == InText)
                        { return; }

                        _Model.Set_FilterString(InText);
                        DoRefresh_Filters();
                    })
                    .OnHighlightTextChanged_Lambda([this](const FString& InText)
                    {
                        if (_Model.Get_HighlightString() == InText)
                        { return; }

                        _Model.Set_HighlightString(InText);
                        DoRefresh_Filters();
                    })
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(CkStyle::SpaceS, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS)
                [
                    DoCreate_ProvenanceChips()
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SAssignNew(_EntityTree, STreeView<TSharedPtr<FCkSaveDebugger_TreeNode>>)
                    .TreeItemsSource(&_VisibleRoots)
                    .OnGenerateRow(this, &SCkSaveDebuggerWindow::DoGenerate_EntityRow)
                    .OnGetChildren(this, &SCkSaveDebuggerWindow::DoGet_EntityChildren)
                    .OnSelectionChanged(this, &SCkSaveDebuggerWindow::DoOnEntitySelectionChanged)
                    .OnContextMenuOpening(this, &SCkSaveDebuggerWindow::DoOnEntityContextMenu)
                    .OnKeyDownHandler(this, &SCkSaveDebuggerWindow::DoOnTreeKeyDown)
                    .SelectionMode(ESelectionMode::Single)
                ]
            ]

            + SSplitter::Slot()
            .Value(0.36f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .FillHeight(0.6f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                    [
                        SAssignNew(_EntityDetailBox, SVerticalBox)
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [
                        SNew(SCkDebug_Icon)
                        .Brush(ck_save_debugger_window::Get_IconBrush(ECk_Icon::Payload))
                        .Meaning(FText::FromString(TEXT("Replicated-data blobs saved against the selected entity")))
                        .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
                        .Size(FVector2D{ck_save_debugger_window::k_PanelIconSize, ck_save_debugger_window::k_PanelIconSize})
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SCkDebug_SectionHeader)
                        .Label(FText::FromString(TEXT("Payloads")))
                        .ToolTip(FText::FromString(TEXT("Every payload the save holds for the selected entity")))
                        .Underline(true)
                        .RightContent()
                        [
                            SNew(SCkDebug_CountBadge)
                            .ValueText_Lambda([this]() -> FText
                            {
                                return FText::FromString(ck::Format_UE(TEXT("{}"), _PayloadRows.Num()));
                            })
                            .ValueColor(CkStyle::Text())
                            .BackgroundColor(CkStyle::Bg2())
                            .BorderColor(CkStyle::Border())
                        ]
                    ]
                ]

                + SVerticalBox::Slot()
                .FillHeight(0.4f)
                [
                    SAssignNew(_PayloadList, SListView<TSharedPtr<FCkSaveDebugger_PayloadRow>>)
                    .ListItemsSource(&_PayloadRows)
                    .OnGenerateRow(this, &SCkSaveDebuggerWindow::DoGenerate_PayloadRow)
                    .OnSelectionChanged(this, &SCkSaveDebuggerWindow::DoOnPayloadSelectionChanged)
                    .OnContextMenuOpening(this, &SCkSaveDebuggerWindow::DoOnPayloadContextMenu)
                    .SelectionMode(ESelectionMode::Single)
                ]
            ]

            + SSplitter::Slot()
            .Value(0.32f)
            [
                SAssignNew(_RightColumnSwitcher, SWidgetSwitcher)
                .WidgetIndex_Lambda([this]() -> int32 { return _Model.Get_HasDiff() ? 1 : 0; })

                + SWidgetSwitcher::Slot()
                [
                    DoCreate_BlobColumn()
                ]

                + SWidgetSwitcher::Slot()
                [
                    DoCreate_DiffColumn()
                ]
            ]
            ]

            + SSplitter::Slot()
            .Value(0.28f)
            [
            SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(ck_save_debugger_window::Get_IconBrush(ECk_Icon::Diagnostics))
                .Meaning(FText::FromString(TEXT("Everything the inspection analyzer had to say about this file")))
                .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
                .Size(FVector2D{ck_save_debugger_window::k_PanelIconSize, ck_save_debugger_window::k_PanelIconSize})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SectionHeader)
                .Label(FText::FromString(TEXT("Diagnostics")))
                .ToolTip(FText::FromString(TEXT("Click a row to select the entity and payload it names")))
                .Underline(true)
                .RightContent()
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [
                        SNew(SCkDebug_CountBadge)
                        .ValueText_Lambda([this]() -> FText
                        {
                            return FText::FromString(ck::Format_UE(TEXT("{}"), _Model.Get_Document().Get_ErrorCount()));
                        })
                        .SuffixText(FText::FromString(TEXT("err")))
                        .ValueColor(CkStyle::Err())
                        .SuffixColor(CkStyle::TextMute())
                        .BackgroundColor(CkStyle::ErrDim())
                        .BorderColor(CkStyle::Err())
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                    [
                        SNew(SCkDebug_CountBadge)
                        .ValueText_Lambda([this]() -> FText
                        {
                            return FText::FromString(ck::Format_UE(TEXT("{}"), _Model.Get_Document().Get_WarningCount()));
                        })
                        .SuffixText(FText::FromString(TEXT("warn")))
                        .ValueColor(CkStyle::Warn())
                        .SuffixColor(CkStyle::TextMute())
                        .BackgroundColor(CkStyle::WarnDim())
                        .BorderColor(CkStyle::Warn())
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        DoCreate_DiagnosticSeverityPills()
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
                    [
                        // A long info run can bury the tree; collapsing keeps the counts and pills visible so the
                        // section still reports what it found while taking one row of height.
                        SNew(SCkDebug_ToggleSurface)
                        .AccessibleText(FText::FromString(TEXT("Diagnostics")))
                        .ToolTipText(FText::FromString(TEXT("Collapse or expand the diagnostics list")))
                        .IsOn_Lambda([this]() -> bool { return _DiagnosticsExpanded; })
                        .OnStateChanged_Lambda([this](const bool InIsOn) { _DiagnosticsExpanded = InIsOn; })
                        [
                            SNew(STextBlock)
                            .Text_Lambda([this]() -> FText
                            {
                                return FText::FromString(_DiagnosticsExpanded ? TEXT("HIDE") : TEXT("SHOW"));
                            })
                            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                        ]
                    ]
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SAssignNew(_DiagnosticList, SListView<TSharedPtr<FCkSaveDebugger_DiagnosticRow>>)
            .Visibility_Lambda([this]() -> EVisibility
            {
                return _DiagnosticsExpanded ? EVisibility::Visible : EVisibility::Collapsed;
            })
            .ListItemsSource(&_DiagnosticRows)
            .OnGenerateRow(this, &SCkSaveDebuggerWindow::DoGenerate_DiagnosticRow)
            .OnSelectionChanged(this, &SCkSaveDebuggerWindow::DoOnDiagnosticSelectionChanged)
            .OnContextMenuOpening(this, &SCkSaveDebuggerWindow::DoOnDiagnosticContextMenu)
            .SelectionMode(ESelectionMode::Single)
        ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoCreate_BlobColumn()
    -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .FillHeight(0.55f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SAssignNew(_BlobDetailBox, SVerticalBox)
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(0.45f)
        [
            SAssignNew(_ValueTree, STreeView<TSharedPtr<FCk_SnapshotInspection_ValueNode>>)
            .TreeItemsSource(&_ValueRoots)
            .OnGenerateRow(this, &SCkSaveDebuggerWindow::DoGenerate_ValueRow)
            .OnGetChildren(this, &SCkSaveDebuggerWindow::DoGet_ValueChildren)
            .SelectionMode(ESelectionMode::Single)
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoCreate_DiffColumn()
    -> TSharedRef<SWidget>
{
    using namespace ck_save_debugger_window;

    // Everything above the list is attribute-bound rather than rebuilt: the diff is immutable once computed, so the
    // only thing that ever moves here is the Show-Unchanged filter and the selected group's detail table.
    const auto DiffIsValid = TAttribute<EVisibility>::CreateLambda([this]() -> EVisibility
    {
        return _Model.Get_Diff().Get_Valid() ? EVisibility::Visible : EVisibility::Collapsed;
    });

    const auto DiffIsInvalid = TAttribute<EVisibility>::CreateLambda([this]() -> EVisibility
    {
        return _Model.Get_Diff().Get_Valid() ? EVisibility::Collapsed : EVisibility::Visible;
    });

    const auto MakeCensusTile = [](const FString& InLabel,
                                   TFunction<int64()> InBaseline,
                                   TFunction<int64()> InCurrent,
                                   bool InAsBytes) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_StatPair)
            .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
            .Label(FText::FromString(InLabel))
            .Value_Lambda([InBaseline, InCurrent, InAsBytes]() -> FText
            {
                return FText::FromString(InAsBytes
                    ? ck_save_debugger_model::Build_DiffBytesText(InBaseline(), InCurrent())
                    : ck_save_debugger_model::Build_DiffCountText(
                        static_cast<int32>(InBaseline()), static_cast<int32>(InCurrent())));
            })
            .ValueColor_Lambda([InBaseline, InCurrent]() -> FSlateColor
            {
                return FSlateColor{CkStyle::GetToneColor(
                    ck_save_debugger_model::Get_DiffDeltaTone(InCurrent() - InBaseline()))};
            });
    };

    const auto MakeKindBadge = [this](ECk_SnapshotInspection_DiffGroupKind InKind,
                                      const FString& InSuffix,
                                      ECk_Tone InTone) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_CountBadge)
            .ValueText_Lambda([this, InKind]() -> FText
            {
                return FText::FromString(ck::Format_UE(TEXT("{}"), _Model.Get_Diff().Get_GroupCountOfKind(InKind)));
            })
            .SuffixText(FText::FromString(InSuffix))
            .ValueColor(CkStyle::GetToneColor(InTone))
            .SuffixColor(CkStyle::TextMute())
            .BackgroundColor(CkStyle::GetToneDimColor(InTone))
            .BorderColor(CkStyle::GetToneColor(InTone));
    };

    auto Actions = SNew(SWrapBox).UseAllottedSize(true);

    Actions->AddSlot()
    .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS})
    [
        Build_CommandButton(ECk_Icon::Report,
            TEXT("Copy Diff Report"),
            TEXT("Copy the whole comparison to the clipboard: census deltas, every group, every payload type"),
            FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnCopyDiffReportClicked),
            true)
    ];

    Actions->AddSlot()
    .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS})
    [
        Build_CommandButton(ECk_Icon::Door,
            TEXT("Close Diff"),
            TEXT("Drop the comparison and give the column back to blob inspection"),
            FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnCloseDiffClicked),
            true)
    ];

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(Get_IconBrush(ECk_Icon::Size))
                .Meaning(FText::FromString(TEXT("The open save compared against a baseline file")))
                .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
                .Size(FVector2D{k_PanelIconSize, k_PanelIconSize})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SectionHeader)
                .Label(FText::FromString(TEXT("Save Diff")))
                .ToolTip(FText::FromString(TEXT("Entity groups whose population or payloads moved between the two saves, biggest change first")))
                .Underline(true)
                .RightContent()
                [
                    SNew(SCkDebug_IconToggle)
                    .IconId(ECk_Icon::Anchored)
                    .Label(FText::FromString(TEXT("Show unchanged")))
                    .ToolTip(FText::FromString(TEXT("Also list the groups that did not move between the two saves")))
                    .IsOn_Lambda([this]() -> bool { return _Model.Get_DiffShowUnchanged(); })
                    .OnStateChanged_Lambda([this](const bool InShowUnchanged)
                    {
                        if (_Model.Get_DiffShowUnchanged() == InShowUnchanged)
                        { return; }

                        _Model.Set_DiffShowUnchanged(InShowUnchanged);
                        DoRebuild_Diff();
                    })
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    return FText::FromString(ck::Format_UE(TEXT("{} -> {}"),
                        FPaths::GetCleanFilename(_Model.Get_DiffBaselineSourceDescription()),
                        FPaths::GetCleanFilename(_CurrentPath)));
                })
                .ToolTipText_Lambda([this]() -> FText
                {
                    return FText::FromString(ck::Format_UE(TEXT("baseline: {}\ncurrent: {}"),
                        _Model.Get_DiffBaselineSourceDescription(),
                        _Model.Get_Document().Get_SourceDescription()));
                })
                .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                Actions
            ]

            // A diff between two files that could not both be read has no census and no groups — only the reason.
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                SNew(SCkDebug_Card)
                .Visibility(DiffIsInvalid)
                .StripeColor(CkStyle::Err())
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
                    [
                        SNew(SCkDebug_StatusPill)
                        .Text(FText::FromString(TEXT("DIFF UNAVAILABLE")))
                        .Tone(ECk_Tone::Err)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() -> FText
                        {
                            return FText::FromString(_Model.Get_Diff().Get_InvalidReason());
                        })
                        .AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor{CkStyle::Err()})
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                SNew(SCkDebug_Card)
                .Visibility(DiffIsValid)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            MakeCensusTile(TEXT("ENTITIES"),
                                [this]() -> int64 { return _Model.Get_Diff().Get_EntityCountBaseline(); },
                                [this]() -> int64 { return _Model.Get_Diff().Get_EntityCountCurrent(); },
                                false)
                        ]

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            MakeCensusTile(TEXT("PAYLOADS"),
                                [this]() -> int64 { return _Model.Get_Diff().Get_PayloadCountBaseline(); },
                                [this]() -> int64 { return _Model.Get_Diff().Get_PayloadCountCurrent(); },
                                false)
                        ]

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            MakeCensusTile(TEXT("TABLE BYTES"),
                                [this]() -> int64 { return _Model.Get_Diff().Get_SnapshotBytesBaseline(); },
                                [this]() -> int64 { return _Model.Get_Diff().Get_SnapshotBytesCurrent(); },
                                true)
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            MakeKindBadge(ECk_SnapshotInspection_DiffGroupKind::Added, TEXT("added"), ECk_Tone::Warn)
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            MakeKindBadge(ECk_SnapshotInspection_DiffGroupKind::Removed, TEXT("removed"), ECk_Tone::Info)
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            MakeKindBadge(ECk_SnapshotInspection_DiffGroupKind::CountChanged, TEXT("count"), ECk_Tone::Warn)
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            MakeKindBadge(ECk_SnapshotInspection_DiffGroupKind::PayloadsChanged, TEXT("payloads"), ECk_Tone::Accent)
                        ]
                    ]
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(0.6f)
        [
            SAssignNew(_DiffGroupList, SListView<TSharedPtr<FCkSaveDebugger_DiffGroupRow>>)
            .Visibility(DiffIsValid)
            .ListItemsSource(&_DiffGroupRows)
            .OnGenerateRow(this, &SCkSaveDebuggerWindow::DoGenerate_DiffGroupRow)
            .OnSelectionChanged(this, &SCkSaveDebuggerWindow::DoOnDiffGroupSelectionChanged)
            .OnContextMenuOpening(this, &SCkSaveDebuggerWindow::DoOnDiffGroupContextMenu)
            .SelectionMode(ESelectionMode::Multi)
        ]

        + SVerticalBox::Slot()
        .FillHeight(0.4f)
        [
            SNew(SScrollBox)
            .Visibility(DiffIsValid)
            + SScrollBox::Slot()
            .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SAssignNew(_DiffDetailBox, SVerticalBox)
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoCreate_Status()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCkDebug_Icon)
            .Brush(CkStyle::GetRoundedBrush_Pill())
            .Meaning(FText::FromString(TEXT("Tone of the last thing this window did")))
            .ColorAndOpacity_Lambda([this]() -> FSlateColor
            {
                return FSlateColor{CkStyle::GetToneColor(_StatusTone)};
            })
            .Size(FVector2D{ck_save_debugger_window::k_ProblemDotSize, ck_save_debugger_window::k_ProblemDotSize})
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SAssignNew(_StatusText, STextBlock)
            .Text(FText::FromString(TEXT("No save open. Click \"Open Save...\" to begin.")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
        ];
}

// --------------------------------------------------------------------------------------------------------------------
// Commands
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnOpenClicked()
    -> FReply
{
    auto* DesktopPlatform = FDesktopPlatformModule::Get();
    if (ck::Is_NOT_Valid(DesktopPlatform, ck::IsValid_Policy_NullptrOnly{}))
    {
        DoSet_Status(TEXT("Desktop platform not available."), ECk_Tone::Err);
        return FReply::Handled();
    }

    auto OutFiles = TArray<FString>{};
    const auto Opened = DesktopPlatform->OpenFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Open CK Save"),
        FPaths::ProjectSavedDir() / TEXT("SaveGames"),
        TEXT(""),
        TEXT("CK Save Files (*.sav)|*.sav|All Files (*.*)|*.*"),
        EFileDialogFlags::None,
        OutFiles);

    if (NOT Opened || OutFiles.Num() == 0)
    { return FReply::Handled(); }

    DoOpen_Path(OutFiles[0]);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnReloadClicked()
    -> FReply
{
    if (_CurrentPath.IsEmpty())
    {
        DoSet_Status(TEXT("Nothing to reload."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    DoOpen_Path(_CurrentPath);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnExportJsonClicked()
    -> FReply
{
    auto* DesktopPlatform = FDesktopPlatformModule::Get();
    if (ck::Is_NOT_Valid(DesktopPlatform, ck::IsValid_Policy_NullptrOnly{}))
    {
        DoSet_Status(TEXT("Desktop platform not available."), ECk_Tone::Err);
        return FReply::Handled();
    }

    const auto DefaultName = ck::Format_UE(TEXT("{}_inspection.json"), FPaths::GetBaseFilename(_CurrentPath));

    auto OutFiles = TArray<FString>{};
    const auto Saved = DesktopPlatform->SaveFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Export Save Inspection JSON"),
        FPaths::ProjectSavedDir() / TEXT("SaveGames"),
        DefaultName,
        TEXT("JSON files (*.json)|*.json"),
        EFileDialogFlags::None,
        OutFiles);

    if (NOT Saved || OutFiles.Num() == 0)
    { return FReply::Handled(); }

    const auto Json = ck_save_debugger_model::Build_JsonExport(_Model);

    if (FFileHelper::SaveStringToFile(Json, *OutFiles[0], FFileHelper::EEncodingOptions::ForceUTF8))
    {
        DoSet_Status(ck::Format_UE(TEXT("Exported JSON: {}"), FPaths::GetCleanFilename(OutFiles[0])), ECk_Tone::Ok);
    }
    else
    {
        DoSet_Status(ck::Format_UE(TEXT("Failed to write: {}"), OutFiles[0]), ECk_Tone::Err);
    }

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnCopyReportClicked()
    -> FReply
{
    const auto Report = ck_save_debugger_model::Build_TextReport(_Model);
    FPlatformApplicationMisc::ClipboardCopy(*Report);

    DoSet_Status(TEXT("Report copied to clipboard."), ECk_Tone::Ok);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnCompareAgainstClicked()
    -> FReply
{
    if (_CurrentPath.IsEmpty())
    {
        DoSet_Status(TEXT("Open a save first: the open file is the diff's current side."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    auto* DesktopPlatform = FDesktopPlatformModule::Get();
    if (ck::Is_NOT_Valid(DesktopPlatform, ck::IsValid_Policy_NullptrOnly{}))
    {
        DoSet_Status(TEXT("Desktop platform not available."), ECk_Tone::Err);
        return FReply::Handled();
    }

    auto OutFiles = TArray<FString>{};
    const auto Opened = DesktopPlatform->OpenFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Compare Against (baseline save)"),
        FPaths::GetPath(_CurrentPath),
        TEXT(""),
        TEXT("CK Save Files (*.sav)|*.sav|All Files (*.*)|*.*"),
        EFileDialogFlags::None,
        OutFiles);

    if (NOT Opened || OutFiles.Num() == 0)
    { return FReply::Handled(); }

    // The baseline document dies with this scope: the diff is self-contained, so keeping a second whole document
    // alive on the model would buy nothing and give the no-live-handle window twice as much state to invalidate.
    const auto Baseline = ck::snapshot::Inspect_SaveFile(OutFiles[0]);
    const auto Diff = ck::snapshot::Diff_Documents(Baseline, _Model.Get_Document());

    _Model.Set_Diff(Diff, Baseline.Get_SourceDescription());
    _DiffSelectedIdentityPath.Reset();

    DoRebuild_Diff();

    if (NOT Diff.Get_Valid())
    {
        DoSet_Status(ck::Format_UE(TEXT("Diff unavailable: {}"), Diff.Get_InvalidReason()), ECk_Tone::Err);
        return FReply::Handled();
    }

    const auto MovedGroups = Diff.Get_EntityGroups().Num()
        - Diff.Get_GroupCountOfKind(ECk_SnapshotInspection_DiffGroupKind::Unchanged);

    DoSet_Status(ck::Format_UE(TEXT("Diff vs {}: {} of {} groups moved, entities {}"),
        FPaths::GetCleanFilename(OutFiles[0]),
        MovedGroups,
        Diff.Get_EntityGroups().Num(),
        ck_save_debugger_model::Build_DiffCountText(Diff.Get_EntityCountBaseline(), Diff.Get_EntityCountCurrent())),
        ck_save_debugger_model::Get_DiffDeltaTone(
            static_cast<int64>(Diff.Get_EntityCountCurrent()) - static_cast<int64>(Diff.Get_EntityCountBaseline())));

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnVisualizeClicked()
    -> FReply
{
#if WITH_EDITOR
    if (ck::save_debugger_viz::Get_IsVisualizerEnabled())
    {
        DoVisualize_Stop();
        DoSet_Status(TEXT("Visualizer stopped."), ECk_Tone::Neutral);
        return FReply::Handled();
    }

    const auto* EditorWorld = UCk_Utils_EditorOnly_UE::Get_OpenedEditorLevelWorld();
    const auto CurrentPackage = ck::IsValid(EditorWorld) ? EditorWorld->GetOutermost()->GetName() : FString{};

    const auto Match = ck_save_debugger_model::Get_LevelMatch(_Model.Get_Document(), CurrentPackage);
    if (Match == ECkSaveDebugger_LevelMatch::Mismatch)
    {
        const auto SavedPackage = UWorld::RemovePIEPrefix(
            _Model.Get_Document().Get_Header().Get_WorldAssetPath().GetLongPackageName());

        auto Buttons = TArray<UCk_Utils_MessageDialog_UE::DialogButton>{};
        Buttons.Add(UCk_Utils_MessageDialog_UE::DialogButton{
            FText::FromString(ck::Format_UE(TEXT("Load {}"), FPackageName::GetShortName(SavedPackage)))}
            .Set_IsPrimary(true));
        Buttons.Add(UCk_Utils_MessageDialog_UE::DialogButton{FText::FromString(TEXT("View unanchored"))});
        Buttons.Add(UCk_Utils_MessageDialog_UE::DialogButton{FText::FromString(TEXT("Cancel"))});

        const auto Choice = UCk_Utils_MessageDialog_UE::CustomDialog(
            FText::FromString(ck::Format_UE(
                TEXT("This save was captured in [{}] but the open level is [{}].\n\n"
                     "Load the captured level so the diamonds sit in their real surroundings, or view them "
                     "unanchored in the current level?"),
                SavedPackage, CurrentPackage)),
            FText::FromString(TEXT("Save Debugger — Visualize")),
            Buttons);

        constexpr auto ChoiceLoadLevel = 0;
        constexpr auto ChoiceUnanchored = 1;

        if (Choice == ChoiceLoadLevel)
        {
            constexpr auto LoadAsTemplate = false;
            constexpr auto ShowProgress = true;

            if (NOT FEditorFileUtils::LoadMap(SavedPackage, LoadAsTemplate, ShowProgress))
            {
                DoSet_Status(ck::Format_UE(TEXT("Failed to load level [{}]."), SavedPackage), ECk_Tone::Err);
                return FReply::Handled();
            }

            // The OnMapOpened path also refreshes, but the delegate order vs this handler is not a contract worth
            // leaning on — re-derive here so the publish below sees the new level's actors either way.
            DoRefresh_ActorAnnotations();
            DoRebuild_Tree();
        }
        else if (Choice != ChoiceUnanchored)
        { return FReply::Handled(); }
    }

    if (NOT DoVisualize_Publish())
    {
        DoSet_Status(TEXT("Nothing to visualize — no entity in this save carries a world transform."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    const auto WeakWindow = TWeakPtr<SCkSaveDebuggerWindow>{StaticCastSharedRef<SCkSaveDebuggerWindow>(AsShared())};
    ck::save_debugger_viz::Register_OnRowClicked([WeakWindow](const uint32 InSavedId)
    {
        if (const auto Window = WeakWindow.Pin())
        { Window->DoSelect_Entity(InSavedId); }
    });

    if (NOT ck::save_debugger_viz::Set_VisualizerEnabled(true))
    {
        ck::save_debugger_viz::Unregister_OnRowClicked();
        ck::save_debugger_viz::Clear_Rows();
        DoSet_Status(TEXT("Visualizer unavailable during PIE — stop the session first."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    const auto PlacedCount = ck::save_debugger_viz::Get_Rows()->Num();
    DoSet_Status(ck::Format_UE(TEXT("Visualizing {} of {} entities ({}) — click a diamond to select its row."),
        PlacedCount, _Model.Get_Document().Get_Entities().Num(),
        _VisualizeSummary.IsEmpty() ? FString{TEXT("diamonds only")} : _VisualizeSummary), ECk_Tone::Ok);
#endif

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnFrameSelectedClicked()
    -> FReply
{
#if WITH_EDITOR
    if (ck::save_debugger_viz::Frame_SelectedRow())
    { DoSet_Status(TEXT("Viewport framed on the selected entity."), ECk_Tone::Ok); }
    else
    { DoSet_Status(TEXT("Nothing to frame — select a PLACED entity while the visualizer is on."), ECk_Tone::Warn); }
#endif

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoVisualize_Publish()
    -> bool
{
#if WITH_EDITOR
    auto Rows = MakeShared<TArray<FCkSaveDebugger_VisualizationRow>>(
        ck_save_debugger_model::Build_VisualizationRows(_Model.Get_Document(), _Model.Get_SaveKeyAnnotations()));

    if (Rows->IsEmpty())
    {
        ck::save_debugger_viz_retained::Clear();
        ck::save_debugger_viz::Clear_Rows();
        return false;
    }

    ck::save_debugger_viz::Publish_Rows(Rows);
    ck::save_debugger_viz::Set_SelectedSavedId(_Model.Get_SelectedEntitySavedId());

    _VisualizeSummary.Reset();
    if (auto* EditorWorld = UCk_Utils_EditorOnly_UE::Get_OpenedEditorLevelWorld();
        ck::IsValid(EditorWorld))
    {
        if (const auto Stats = ck::save_debugger_viz_retained::Rebuild(EditorWorld, _Model.Get_Document(), *Rows);
            Stats.IsSet())
        {
            _VisualizeSummary = ck::Format_UE(TEXT("{} previews, {} ghost meshes"),
                Stats->PreviewCount, Stats->GhostMeshCount);

            if (const auto Skipped = Stats->UnresolvedClassCount + Stats->GhostsWithoutMeshCount;
                Skipped > 0)
            { _VisualizeSummary += ck::Format_UE(TEXT(", {} without visuals"), Skipped); }

            if (Stats->UnresolvedClassSamples.Num() > 0)
            {
                _VisualizeSummary += ck::Format_UE(TEXT(" — missing in this editor: {}"),
                    FString::Join(Stats->UnresolvedClassSamples, TEXT(", ")));
            }

            if (Stats->ParamsDecodeFailureCount > 0)
            { _VisualizeSummary += ck::Format_UE(TEXT(", {} params blobs undecodable"), Stats->ParamsDecodeFailureCount); }
        }
        else
        { _VisualizeSummary = TEXT("previews pending — editor ECS busy, re-click Visualize"); }
    }

    DoVisualize_SyncSelection();
    return true;
#else
    return false;
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoVisualize_Stop()
    -> void
{
#if WITH_EDITOR
    ck::save_debugger_viz::Set_VisualizerEnabled(false);
    ck::save_debugger_viz::Unregister_OnRowClicked();
    ck::save_debugger_viz::Clear_Rows();
    ck::save_debugger_viz_retained::Clear();
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoVisualize_SyncSelection()
    -> void
{
#if WITH_EDITOR
    if (NOT ck::save_debugger_viz::Get_IsVisualizerEnabled())
    { return; }

    ck::save_debugger_viz::Set_SelectedSavedId(_Model.Get_SelectedEntitySavedId());

    auto SelectedTransform = TOptional<FTransform>{};
    if (const auto Rows = ck::save_debugger_viz::Get_Rows();
        Rows.IsValid())
    {
        for (const auto& Row : *Rows)
        {
            if (Row.SavedId == _Model.Get_SelectedEntitySavedId())
            {
                SelectedTransform = Row.WorldTransform;
                break;
            }
        }
    }

    ck::save_debugger_viz_retained::Update_SelectionGizmo(
        UCk_Utils_EditorOnly_UE::Get_OpenedEditorLevelWorld(), SelectedTransform);
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRefresh_ActorAnnotations()
    -> void
{
#if WITH_EDITOR
    auto Annotations = TMap<FGuid, FString>{};

    if (auto* EditorWorld = UCk_Utils_EditorOnly_UE::Get_OpenedEditorLevelWorld();
        ck::IsValid(EditorWorld))
    {
        for (auto It = TActorIterator<AActor>{EditorWorld}; It; ++It)
        {
            const auto* Actor = *It;

            const auto Identity = ck::save_key::Get_LevelPlacedIdentity(Actor);
            if (Identity.IsEmpty())
            { continue; }

            Annotations.Add(
                FGuid::NewDeterministicGuid(Identity),
                ck::Format_UE(TEXT("{} ({})"), Actor->GetFName(), Actor->GetClass()->GetName()));
        }
    }

    _Model.Set_SaveKeyAnnotations(Annotations);
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnTreeKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent)
    -> FReply
{
#if WITH_EDITOR
    if (InKeyEvent.GetKey() == EKeys::F && ck::save_debugger_viz::Frame_SelectedRow())
    { return FReply::Handled(); }
#endif

    return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnEditorSelectionChanged(
        UObject* InSelectionObject)
    -> void
{
#if WITH_EDITOR
    if (NOT ck::save_debugger_viz::Get_IsVisualizerEnabled())
    { return; }

    if (GEditor == nullptr || InSelectionObject != GEditor->GetSelectedActors())
    { return; }

    const auto* TopActor = GEditor->GetSelectedActors()->GetTop<AActor>();
    if (ck::Is_NOT_Valid(TopActor, ck::IsValid_Policy_NullptrOnly{}))
    { return; }

    const auto Identity = ck::save_key::Get_LevelPlacedIdentity(TopActor);
    if (Identity.IsEmpty())
    { return; }

    const auto SavedId = ck_save_debugger_model::TryGet_SavedIdForSaveKey(
        _Model.Get_Document(), FGuid::NewDeterministicGuid(Identity));

    if (SavedId == ck::snapshot::k_NoSavedEntity)
    { return; }

    DoSelect_Entity(SavedId);
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnMapOpened(
        const FString& InFilename,
        bool InAsTemplate)
    -> void
{
#if WITH_EDITOR
    if (_CurrentPath.IsEmpty())
    { return; }

    DoRefresh_ActorAnnotations();
    DoRebuild_Tree();

    if (ck::save_debugger_viz::Get_IsVisualizerEnabled())
    { DoVisualize_Publish(); }
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnCloseDiffClicked()
    -> FReply
{
    _Model.Clear_Diff();
    _DiffSelectedIdentityPath.Reset();

    DoRebuild_Diff();
    DoSet_Status(TEXT("Diff closed."), ECk_Tone::Neutral);

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnCopyDiffReportClicked()
    -> FReply
{
    if (NOT _Model.Get_HasDiff())
    {
        DoSet_Status(TEXT("No diff to copy."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    const auto Report = ck_save_debugger_model::Build_DiffReportText(_Model);
    FPlatformApplicationMisc::ClipboardCopy(*Report);

    DoSet_Status(TEXT("Diff report copied to clipboard."), ECk_Tone::Ok);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnTryDecodeClicked()
    -> FReply
{
    DoDecode_SelectedPayload(ECk_SnapshotInspection_DecodePolicy::AvailableTypesOnly);
    return FReply::Handled();
}

auto
    SCkSaveDebuggerWindow::
    DoOnTryLoadTypeClicked()
    -> FReply
{
    DoDecode_SelectedPayload(ECk_SnapshotInspection_DecodePolicy::AllowTypeLoad);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnCopyTypePathClicked()
    -> FReply
{
    const auto* Payload = _Model.TryGet_SelectedPayload();
    if (Payload == nullptr)
    {
        DoSet_Status(TEXT("No payload selected."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    FPlatformApplicationMisc::ClipboardCopy(*Payload->Get_Entry().Get_TypePath());
    DoSet_Status(TEXT("Type path copied to clipboard."), ECk_Tone::Ok);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnCopyMetadataClicked()
    -> FReply
{
    const auto Metadata = DoBuild_SelectedBlobMetadata();
    if (Metadata.IsEmpty())
    {
        DoSet_Status(TEXT("Nothing selected."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    FPlatformApplicationMisc::ClipboardCopy(*Metadata);
    DoSet_Status(TEXT("Blob metadata copied to clipboard."), ECk_Tone::Ok);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnExportBlobClicked()
    -> FReply
{
    const auto Bytes = DoGet_SelectedBlobBytes();
    if (Bytes.Num() == 0)
    {
        DoSet_Status(TEXT("Selected blob is empty."), ECk_Tone::Warn);
        return FReply::Handled();
    }

    auto* DesktopPlatform = FDesktopPlatformModule::Get();
    if (ck::Is_NOT_Valid(DesktopPlatform, ck::IsValid_Policy_NullptrOnly{}))
    {
        DoSet_Status(TEXT("Desktop platform not available."), ECk_Tone::Err);
        return FReply::Handled();
    }

    const auto* Payload = _Model.TryGet_SelectedPayload();
    const auto DefaultName = Payload != nullptr
        ? ck::Format_UE(TEXT("payload_{}.bin"), Payload->Get_TableIndex())
        : ck::Format_UE(TEXT("actorsavefields_{}.bin"), static_cast<int64>(_Model.Get_SelectedEntitySavedId()));

    auto OutFiles = TArray<FString>{};
    const auto Saved = DesktopPlatform->SaveFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Export Raw Blob"),
        FPaths::ProjectSavedDir() / TEXT("SaveGames"),
        DefaultName,
        TEXT("Binary files (*.bin)|*.bin|All Files (*.*)|*.*"),
        EFileDialogFlags::None,
        OutFiles);

    if (NOT Saved || OutFiles.Num() == 0)
    { return FReply::Handled(); }

    if (FFileHelper::SaveArrayToFile(Bytes, *OutFiles[0]))
    {
        DoSet_Status(ck::Format_UE(TEXT("Exported blob: {}"), FPaths::GetCleanFilename(OutFiles[0])), ECk_Tone::Ok);
    }
    else
    {
        DoSet_Status(ck::Format_UE(TEXT("Failed to write: {}"), OutFiles[0]), ECk_Tone::Err);
    }

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOpen_Path(
        const FString& InAbsolutePath)
    -> void
{
    if (InAbsolutePath.IsEmpty())
    {
        DoSet_Status(TEXT("No path to open."), ECk_Tone::Warn);
        return;
    }

    _CurrentPath = InAbsolutePath;

    // A malformed, foreign or stale file is expected input here — the inspection API reports a status and never
    // ensures, so there is nothing to guard against beyond rendering what came back.
    const auto Document = ck::snapshot::Inspect_SaveFile(InAbsolutePath);
    _Model.Set_Document(Document);

    if (ck::IsValid(_PathLabel))
    { _PathLabel->SetText(FText::FromString(InAbsolutePath)); }

    DoRefresh_ActorAnnotations();
    DoRebuild_All();

#if WITH_EDITOR
    // A live visualizer follows the document: republish the new file's rows, or shut down when it has none — a
    // viewport still showing the previous file's diamonds would be a lie.
    if (ck::save_debugger_viz::Get_IsVisualizerEnabled())
    {
        if (NOT DoVisualize_Publish())
        { DoVisualize_Stop(); }
    }
#endif

    const auto Tone = ck_save_debugger_model::Get_CompatibilityTone(_Model.Get_Document());
    DoSet_Status(ck::Format_UE(TEXT("{}: {} entities, {} payloads, {} errors, {} warnings"),
        ck::snapshot::Get_ReadStatusText(_Model.Get_Document().Get_ReadStatus()),
        _Model.Get_Document().Get_Entities().Num(),
        _Model.Get_Document().Get_Payloads().Num(),
        _Model.Get_Document().Get_ErrorCount(),
        _Model.Get_Document().Get_WarningCount()), Tone);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoDecode_SelectedPayload(
        ECk_SnapshotInspection_DecodePolicy InPolicy)
    -> void
{
    const auto PayloadIndex = _Model.Get_SelectedPayloadIndex();
    if (NOT _Model.Get_Document().Get_Payloads().IsValidIndex(PayloadIndex))
    {
        DoSet_Status(TEXT("No payload selected."), ECk_Tone::Warn);
        return;
    }

    const auto Result = ck::snapshot::TryDecode_Payload(_Model.Get_Document(), PayloadIndex, InPolicy);
    _Model.Store_PayloadDecode(PayloadIndex, Result);

    DoRebuild_BlobDetail();

    DoSet_Status(ck::Format_UE(TEXT("Payload {} decode: {}"),
        PayloadIndex,
        ck::snapshot::Get_DecodeStatusText(Result.Get_Status())),
        ck_save_debugger_model::Get_DecodeStatusTone(Result.Get_Status()));
}

// --------------------------------------------------------------------------------------------------------------------
// Rebuilds
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRebuild_All()
    -> void
{
    // The depth cycler's ceiling is the longest type path in the document. Computed here rather than bound, so the
    // toolbar never walks the payload table on a paint pass.
    _MaxNameDepth = 1;
    for (const auto& Payload : _Model.Get_Document().Get_Payloads())
    {
        _MaxNameDepth = FMath::Max(_MaxNameDepth,
            SCkDebug_NameLabel::Get_SegmentCount(Payload.Get_Entry().Get_TypePath()));
    }

    DoRebuild_Summary();
    DoRebuild_Tree();
    DoRebuild_Diagnostics();
    DoRebuild_EntityDetail();
    DoRebuild_BlobDetail();
    DoRebuild_Diff();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRebuild_Summary()
    -> void
{
    using namespace ck_save_debugger_window;

    if (NOT _SummaryBox.IsValid())
    { return; }

    _SummaryBox->ClearChildren();

    const auto& Document = _Model.Get_Document();
    const auto& Header = Document.Get_Header();
    const auto& Census = Document.Get_Census();

    auto HeaderRows = SNew(SVerticalBox);

    const auto AddRow = [&HeaderRows](const FString& InKey, const FString& InValue) -> void
    {
        HeaderRows->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(InKey))
            .ValueText(FText::FromString(InValue))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::Text())
        ];
    };

    AddRow(TEXT("Source"), Document.Get_SourceDescription());
    AddRow(TEXT("Format version"), ck::Format_UE(TEXT("{}"), static_cast<int32>(Header.Get_FormatVersion())));
    AddRow(TEXT("Engine"), Header.Get_EngineVersion());
    AddRow(TEXT("Plugin build hash"), Header.Get_PluginBuildHash().ToString(EGuidFormats::DigitsWithHyphens));
    AddRow(TEXT("Captured (UTC)"), Header.Get_TimestampUTC().ToIso8601());
    AddRow(TEXT("Map"), Header.Get_WorldAssetPath().ToString());
    AddRow(TEXT("File bytes"), ck::Format_UE(TEXT("{}"), Document.Get_SourceByteCount()));
    AddRow(TEXT("File sha256"), Document.Get_SourceHashHex());
    AddRow(TEXT("Table bytes"), ck::Format_UE(TEXT("{}"), Document.Get_SnapshotByteCount()));
    AddRow(TEXT("Table sha256"), Document.Get_SnapshotHashHex());

    const auto OpaqueCount = ck_save_debugger_model::Get_OpaquePayloadCount(Document);

    _SummaryBox->AddSlot()
    .AutoHeight()
    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
    [
        SNew(SCkDebug_Card)
        .StripeColor(CkStyle::GetToneColor(ck_save_debugger_model::Get_CompatibilityTone(Document)))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(Get_IconBrush(ECk_Icon::SaveSlot))
                .Meaning(FText::FromString(TEXT("Census of the save currently open")))
                .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
                .Size(FVector2D{24.0f, 24.0f})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                Build_CountTile(TEXT("ENTITIES"), Census.Get_EntityCount(), ECk_Tone::Info)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                Build_CountTile(TEXT("PAYLOADS"), Census.Get_PayloadCount(), ECk_Tone::Info)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                // Opaque is an observation about THIS editor, never a defect in the file — neutral, never red.
                Build_CountTile(TEXT("OPAQUE"), OpaqueCount, ECk_Tone::Neutral)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                Build_CountTile(TEXT("ERRORS"), Document.Get_ErrorCount(), ECk_Tone::Err)
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                Build_CountTile(TEXT("WARNINGS"), Document.Get_WarningCount(), ECk_Tone::Warn)
            ]
        ]
    ];

    // Header and sidecar sit SIDE BY SIDE, not stacked: each is a narrow column of key/value rows, so stacking
    // them left the middle of a wide window empty and pushed the ownership tree off the bottom. The splitter
    // makes the division draggable rather than a fixed guess at how wide either column wants to be.
    _SummaryBox->AddSlot()
    .AutoHeight()
    [
        SNew(SSplitter)
        .Orientation(Orient_Horizontal)

        + SSplitter::Slot()
        .Value(0.5f)
        [
            SNew(SCkDebug_InspectorPanel)
            .Title(FText::FromString(TEXT("File & Header")))
            .IconBrush(Get_IconBrush(ECk_Icon::SaveSlot))
            .IconColor(CkStyle::Accent())
            .CountText(FText::FromString(FPaths::GetCleanFilename(_CurrentPath)))
            .StatusPillText(FText::FromString(FString{ck::snapshot::Get_CompatibilityText(Document.Get_Compatibility())}))
            .StatusPillTone(ck_save_debugger_model::Get_CompatibilityTone(Document))
            // Expanded, matching the sidecar beside it. It was collapsed while the two panels were STACKED, where
            // nine header rows pushed the ownership tree down; side by side they share one row's height, so
            // collapsing this one buys nothing and just leaves the pair mismatched.
            .StartExpanded(true)
            .Body()
            [
                HeaderRows
            ]
        ]

        + SSplitter::Slot()
        .Value(0.5f)
        [
            DoCreate_SlotMetaPanel()
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoCreate_DiagnosticSeverityPills()
    -> TSharedRef<SWidget>
{
    using namespace ck_save_debugger_window;

    auto Row = SNew(SHorizontalBox);

    // Visual Studio's Error List shape: one latch per severity, all on by default, and turning every one off
    // shows an empty list rather than silently reverting to "all" — an empty result IS the honest answer to
    // "show me nothing".
    const auto AddPill = [&Row, this](ECk_SnapshotInspection_Severity InSeverity, const TCHAR* InLabel,
        const FLinearColor& InColor) -> void
    {
        Row->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
        [
            SNew(SCkDebug_ToggleSurface)
            .AccessibleText(FText::FromString(InLabel))
            .ToolTipText(FText::FromString(ck::Format_UE(TEXT("Show {} diagnostics"), InLabel)))
            .IsOn_Lambda([this, InSeverity]() -> bool { return Get_SeverityShown(InSeverity); })
            .OnStateChanged_Lambda([this, InSeverity](const bool InIsOn)
            {
                Set_SeverityShown(InSeverity, InIsOn);
                DoRebuild_Diagnostics();
            })
            [
                SNew(STextBlock)
                .Text(FText::FromString(InLabel))
                .ColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([this, InSeverity, InColor]() -> FSlateColor
                {
                    // The latch tints its own glyph rather than repeating the state in a second control.
                    return FSlateColor{Get_SeverityShown(InSeverity) ? InColor : CkStyle::TextMute()};
                }))
            ]
        ];
    };

    AddPill(ECk_SnapshotInspection_Severity::Error, TEXT("ERR"), CkStyle::Err());
    AddPill(ECk_SnapshotInspection_Severity::Warning, TEXT("WARN"), CkStyle::Warn());
    AddPill(ECk_SnapshotInspection_Severity::Info, TEXT("INFO"), CkStyle::Text());

    return Row;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    Get_SeverityShown(
        ECk_SnapshotInspection_Severity InSeverity) const
    -> bool
{
    return (_DiagnosticSeverityMask & (1u << static_cast<uint8>(InSeverity))) != 0u;
}

auto
    SCkSaveDebuggerWindow::
    Set_SeverityShown(
        ECk_SnapshotInspection_Severity InSeverity,
        bool InShown)
    -> void
{
    const auto Bit = static_cast<uint8>(1u << static_cast<uint8>(InSeverity));
    _DiagnosticSeverityMask = static_cast<uint8>(InShown
        ? (_DiagnosticSeverityMask | Bit)
        : (_DiagnosticSeverityMask & ~Bit));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoCreate_SlotMetaPanel()
    -> TSharedRef<SWidget>
{
    using namespace ck_save_debugger_window;

    // The decoded thumbnail is dropped FIRST: it belongs to the previously open save, and a brush outliving its
    // document would paint the wrong picture beside the new one.
    _SlotMetaThumbnailBrush.Reset();
    _SlotMetaThumbnail.Reset();

    const auto& Meta = _Model.Get_Document().Get_SlotMeta();

    // A save with no sidecar is ORDINARY — a bare Request_Save writes none — so this reads Neutral and states the
    // fact, the same rule the type-availability pills follow. It is never an error tone.
    const auto Found = Meta.Get_Found();

    auto MetaRows = SNew(SVerticalBox);

    const auto AddRow = [&MetaRows](const FString& InKey, const FString& InValue) -> void
    {
        MetaRows->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(InKey))
            .ValueText(FText::FromString(InValue))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::Text())
        ];
    };

    if (Found)
    {
        AddRow(TEXT("Sidecar"), Meta.Get_SourceDescription());
        AddRow(TEXT("Title"), Meta.Get_Title().ToString());
        AddRow(TEXT("Saved (UTC)"), Meta.Get_TimestampUTC().ToIso8601());
        AddRow(TEXT("Map"), Meta.Get_WorldAssetPath());

        // Game-defined and opaque to CkSnapshot: render whatever the game recorded rather than naming fields this
        // module cannot know about. Sorted so two saves diff by eye.
        auto FieldKeys = TArray<FName>{};
        Meta.Get_CustomFields().GenerateKeyArray(FieldKeys);
        FieldKeys.Sort(FNameLexicalLess{});

        for (const auto& FieldKey : FieldKeys)
        { AddRow(FieldKey.ToString(), Meta.Get_CustomFields()[FieldKey]); }

        if (Meta.Get_ScreenshotByteCount() > 0)
        {
            AddRow(TEXT("Screenshot bytes"), ck::Format_UE(TEXT("{}"), Meta.Get_ScreenshotByteCount()));
            AddRow(TEXT("Screenshot sha256"), Meta.Get_ScreenshotHashHex());

            // Decoding belongs to CkSnapshot (this module creates no UObjects); the BRUSH is ours. The texture is
            // transient and unrooted, so it is held strongly here and handed to FDeferredCleanupSlateBrush, which
            // keeps the resource alive for exactly as long as Slate is still drawing it.
            _SlotMetaThumbnail.Reset(ck::snapshot::slot_meta::Decode_PngAsTexture(Meta.Get_ScreenshotPng()));

            if (_SlotMetaThumbnail.IsValid())
            { _SlotMetaThumbnailBrush = FDeferredCleanupSlateBrush::CreateBrush(_SlotMetaThumbnail.Get()); }
        }
    }
    else
    {
        AddRow(TEXT("Sidecar"), TEXT("none beside this save"));
    }

    auto Body = SNew(SHorizontalBox);

    if (_SlotMetaThumbnailBrush.IsValid())
    {
        Body->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Top)
        .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
        [
            // A button, not a bare image: the inline thumbnail is deliberately small enough to sit beside the
            // rows, which is too small to actually READ a scene, so clicking opens it at full size.
            SNew(SButton)
            .ButtonStyle(FCoreStyle::Get(), TEXT("NoBorder"))
            .ToolTipText(FText::FromString(TEXT("Click to view this screenshot full size")))
            .OnClicked(this, &SCkSaveDebuggerWindow::DoOnThumbnailClicked)
            [
                SNew(SBox)
                .WidthOverride(192.0f)
                .HeightOverride(108.0f)
                [
                    SNew(SImage)
                    .Image(_SlotMetaThumbnailBrush->GetSlateBrush())
                ]
            ]
        ];
    }

    Body->AddSlot()
    .FillWidth(1.0f)
    [
        MetaRows
    ];

    return SNew(SCkDebug_InspectorPanel)
        .Title(FText::FromString(TEXT("Slot Metadata")))
        .IconBrush(Get_IconBrush(ECk_Icon::SaveSlot))
        .IconColor(CkStyle::Accent())
        .CountText(FText::FromString(Found ? Meta.Get_Title().ToString() : FString{}))
        .StatusPillText(FText::FromString(Found ? TEXT("SIDECAR") : TEXT("NO SIDECAR")))
        .StatusPillTone(Found ? ECk_Tone::Ok : ECk_Tone::Neutral)
        .StartExpanded(Found)
        .Body()
        [
            Body
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnThumbnailClicked()
    -> FReply
{
    if (NOT _SlotMetaThumbnailBrush.IsValid() || NOT _SlotMetaThumbnail.IsValid())
    { return FReply::Handled(); }

    const auto Texture = _SlotMetaThumbnail.Get();
    const auto NativeSize = FVector2D{static_cast<double>(Texture->GetSizeX()), static_cast<double>(Texture->GetSizeY())};

    // Sized to the screenshot's own aspect, capped so a 4K capture cannot open larger than the desktop. The brush
    // is SHARED with the inline thumbnail rather than decoded again — same texture, two views.
    const auto DesktopSize = FSlateApplication::Get().GetPreferredWorkArea().GetSize();
    const auto MaxSize = FVector2D{DesktopSize.X * 0.9, DesktopSize.Y * 0.9};
    const auto Scale = FMath::Min(1.0, FMath::Min(MaxSize.X / NativeSize.X, MaxSize.Y / NativeSize.Y));

    const auto Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("CK Save — Screenshot")))
        .ClientSize(NativeSize * Scale)
        .SupportsMaximize(true)
        .SupportsMinimize(false)
        [
            SNew(SImage)
            .Image(_SlotMetaThumbnailBrush->GetSlateBrush())
        ];

    // Non-modal on purpose: the point is comparing the picture against the rows behind it, which a modal blocks.
    FSlateApplication::Get().AddWindow(Window);

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRebuild_Tree()
    -> void
{
    const auto SetChanged = _Model.Rebuild_Tree();

    DoRefresh_Filters();

    if (NOT SetChanged || NOT _EntityTree.IsValid())
    { return; }

    // Ownership depth is untrusted input — walk with an explicit stack, never recursion.
    auto Stack = _VisibleRoots;
    while (Stack.Num() > 0)
    {
        const auto Node = Stack.Pop();
        if (NOT Node.IsValid())
        { continue; }

        _EntityTree->SetItemExpansion(Node, true);
        Stack.Append(Node->Children);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRefresh_Filters()
    -> void
{
    _Model.Apply_Filters();

    _VisibleRoots.Reset();
    for (const auto& Root : _Model.Get_TreeRoots())
    {
        if (Root.IsValid() && Root->IsVisible)
        { _VisibleRoots.Add(Root); }
    }

    if (_EntityTree.IsValid())
    { _EntityTree->RequestTreeRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRebuild_EntityDetail()
    -> void
{
    using namespace ck_save_debugger_window;

    if (NOT _EntityDetailBox.IsValid())
    { return; }

    _EntityDetailBox->ClearChildren();

    _PayloadRows.Reset();

    const auto* Summary = _Model.TryGet_SelectedEntity();
    if (Summary == nullptr)
    {
        _EntityDetailBox->AddSlot()
        .AutoHeight()
        [
            Build_EmptyState(ECk_Icon::Minimap,
                TEXT("Select an entity to inspect its recipe, ownership and payloads."))
        ];

        if (_PayloadList.IsValid())
        { _PayloadList->RequestListRefresh(); }

        return;
    }

    const auto& Entry = Summary->Get_Entry();

    auto IdentityRows  = SNew(SVerticalBox);
    auto OwnershipRows = SNew(SVerticalBox);
    auto RecipeRows    = SNew(SVerticalBox);
    auto TransformRows = SNew(SVerticalBox);
    auto BlobRows      = SNew(SVerticalBox);

    const auto AddRow = [](const TSharedRef<SVerticalBox>& InBox, const FString& InKey, const FString& InValue) -> void
    {
        InBox->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(InKey))
            .ValueText(FText::FromString(InValue))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::Text())
        ];
    };

    // A class path shortens to its class name (last dot segment), not the toolbar's underscore-token depth —
    // suffixed class names collapse to noise under token splitting ("..._M2bProbe_Replicated" → "Replicated").
    // Depth Full still shows the whole path, and the full path stays one hover away regardless.
    const auto AddPathRow = [this](const TSharedRef<SVerticalBox>& InBox, const FString& InKey, const FString& InPath) -> void
    {
        const auto Display = InPath.IsEmpty()
            ? FString{TEXT("(none)")}
            : _NameDepth == 0
                ? InPath
                : ck_save_debugger_model::Build_ShortTypeName(InPath);

        InBox->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(InKey))
            .ValueText(FText::FromString(Display))
            .ToolTipText(FText::FromString(InPath.IsEmpty() ? InKey : InPath))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::Value_Object())
        ];
    };

    // Owner links are ROW SELECTIONS, never entity navigation: the target is a saved id in a file, not a live
    // entity, so SCkDebug_EntityRef (whose click opens the ECS debugger on a live handle) must never appear here.
    const auto AddLinkRow = [this, &OwnershipRows](const FString& InKey, uint32 InSavedId) -> void
    {
        const auto Resolvable = _Model.Get_Document().TryGet_EntityIndexForSavedId(InSavedId) != INDEX_NONE;

        // Left unbound when the owner is absent from the table: the row still reports the id (in the error tone)
        // but there is nothing to select, and KeyValueRow renders a plain label rather than a button.
        auto OnKeyClicked = FOnCkDebugKeyValueRow_KeyClicked{};
        if (Resolvable)
        {
            OnKeyClicked = FOnCkDebugKeyValueRow_KeyClicked::CreateLambda([this, InSavedId]()
            {
                DoSelect_Entity(InSavedId);
            });
        }

        OwnershipRows->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(InKey))
            .ValueText(FText::FromString(ck_save_debugger_model::Build_SavedIdText(InSavedId)))
            .ToolTipText(FText::FromString(Resolvable
                ? TEXT("Click the key to select this owner's row in the ownership tree")
                : TEXT("This owner id names no row in the save's entity table")))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(Resolvable ? CkStyle::Text() : CkStyle::Err())
            .OnKeyClicked(OnKeyClicked)
        ];
    };

    AddRow(IdentityRows, TEXT("Saved id"), ck_save_debugger_model::Build_SavedIdText(Entry.Get_SavedId()));
    AddRow(IdentityRows, TEXT("Table index"), ck::Format_UE(TEXT("{}"), Summary->Get_TableIndex()));
    AddRow(IdentityRows, TEXT("Provenance"), ck::snapshot::Get_ProvenanceText(Entry.Get_Provenance()));
    AddRow(IdentityRows, TEXT("Identity"), Summary->Get_IdentityText());
    AddRow(IdentityRows, TEXT("Label"), Entry.Get_Label());
    AddRow(IdentityRows, TEXT("Save key"), Entry.Get_SaveKey().IsValid()
        ? Entry.Get_SaveKey().ToString(EGuidFormats::DigitsWithHyphens)
        : FString{TEXT("(unset)")});

    // The save records only the rendezvous key for an EngineOwned row; the NAME lives in the level. When the
    // matching level is open, the window's reverse map answers what the file cannot.
    if (const auto* Annotation = _Model.Get_SaveKeyAnnotations().Find(Entry.Get_SaveKey());
        Annotation != nullptr && Entry.Get_SaveKey().IsValid())
    { AddRow(IdentityRows, TEXT("Editor actor"), *Annotation); }
    AddRow(IdentityRows, TEXT("Player id"), Entry.Get_PlayerId());

    AddLinkRow(TEXT("Lifetime owner"), Entry.Get_LifetimeOwnerSavedId());
    AddLinkRow(TEXT("Context owner"), Entry.Get_ContextOwnerSavedId());

    AddPathRow(RecipeRows, TEXT("Script class"), Entry.Get_ScriptClassPath());
    AddPathRow(RecipeRows, TEXT("Actor class"), Entry.Get_ActorClassPath());

    for (auto StepIndex = 0; StepIndex < Entry.Get_BuildRecipe().Num(); ++StepIndex)
    {
        const auto& Step = Entry.Get_BuildRecipe()[StepIndex];
        AddPathRow(RecipeRows, ck::Format_UE(TEXT("step {}"), StepIndex), Step.Get_ScriptClassPath());
        AddPathRow(RecipeRows, ck::Format_UE(TEXT("step {} archetype"), StepIndex), Step.Get_ArchetypePath());
    }

    AddRow(TransformRows, TEXT("Actor spawn"), Build_TransformText(Entry.Get_ActorSpawnTransform()));
    AddRow(TransformRows, TEXT("Saved world"), Build_TransformText(Entry.Get_SavedWorldTransform()));

    AddRow(BlobRows, TEXT("Spawn params bytes"), ck::Format_UE(TEXT("{}"), Summary->Get_SpawnParamsByteCount()));

    if (const auto* SpawnDecode = _Model.TryGet_SpawnParamsDecode(Summary->Get_TableIndex()))
    {
        AddRow(BlobRows, TEXT("Spawn params decode"), ck::Format_UE(TEXT("{} {}"),
            ck::snapshot::Get_DecodeStatusText(SpawnDecode->Get_Status()),
            SpawnDecode->Get_ErrorText()));
    }

    AddRow(BlobRows, TEXT("Actor SaveGame bytes"), ck::Format_UE(TEXT("{}"), Summary->Get_ActorSaveFieldByteCount()));

    if (Summary->Get_SpawnParamsByteCount() > 0)
    {
        const auto EntityIndex = Summary->Get_TableIndex();
        BlobRows->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
        [
            Build_CommandButton(ECk_Icon::Settings,
                TEXT("Try Decode Spawn Params"),
                TEXT("Project the spawn-params blob into a value tree using only types this editor already has loaded"),
                FOnClicked::CreateLambda([this, EntityIndex]() -> FReply
                {
                    const auto Result = ck::snapshot::TryDecode_SpawnParams(
                        _Model.Get_Document(), EntityIndex, ECk_SnapshotInspection_DecodePolicy::AvailableTypesOnly);

                    _Model.Store_SpawnParamsDecode(EntityIndex, Result);

                    DoSet_Status(ck::Format_UE(TEXT("Spawn params decode: {}"),
                        ck::snapshot::Get_DecodeStatusText(Result.Get_Status())),
                        ck_save_debugger_model::Get_DecodeStatusTone(Result.Get_Status()));

                    DoRebuild_EntityDetail();
                    return FReply::Handled();
                }),
                true)
        ];
    }

    const auto AddPanel = [this](const FString& InTitle,
                                 ECk_Icon InIconId,
                                 const FString& InCountText,
                                 const TSharedRef<SVerticalBox>& InBody) -> void
    {
        _EntityDetailBox->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_InspectorPanel)
            .Title(FText::FromString(InTitle))
            .IconBrush(ck_save_debugger_window::Get_IconBrush(InIconId))
            .IconColor(CkStyle::Accent())
            .CountText(FText::FromString(InCountText))
            .Body()
            [
                InBody
            ]
        ];
    };

    _EntityDetailBox->AddSlot()
    .AutoHeight()
    [
        SNew(SCkDebug_InspectorPanel)
        .Title(FText::FromString(TEXT("Identity")))
        .IconBrush(Get_IconBrush(ECk_Icon::SaveKey))
        .IconColor(CkStyle::Accent())
        .StatusPillText(FText::FromString(FString{ck::snapshot::Get_ProvenanceText(Entry.Get_Provenance())}))
        .StatusPillTone(ECk_Tone::Neutral)
        .Body()
        [
            IdentityRows
        ]
    ];

    AddPanel(TEXT("Ownership"), ECk_Icon::Web, FString{}, OwnershipRows);
    AddPanel(TEXT("Recipe"), ECk_Icon::Catalog,
        ck::Format_UE(TEXT("{} steps"), Summary->Get_BuildStepCount()), RecipeRows);
    AddPanel(TEXT("Transforms"), ECk_Icon::Compass, FString{}, TransformRows);
    AddPanel(TEXT("Blobs"), ECk_Icon::Payload,
        ck::Format_UE(TEXT("{} B"), Summary->Get_SpawnParamsByteCount() + Summary->Get_ActorSaveFieldByteCount()),
        BlobRows);

    // ---- Payload rows for this entity ----
    const auto Rows = _Model.Get_PayloadRows_ForEntity(Entry.Get_SavedId());
    auto Existing = MoveTemp(_PayloadRowsByIndex);
    _PayloadRowsByIndex.Reset();

    for (const auto& Row : Rows)
    {
        auto Item = TSharedPtr<FCkSaveDebugger_PayloadRow>{};
        if (auto* Found = Existing.Find(Row.PayloadIndex))
        { Item = *Found; }
        else
        { Item = MakeShared<FCkSaveDebugger_PayloadRow>(); }

        *Item = Row;
        _PayloadRows.Add(Item);
        _PayloadRowsByIndex.Add(Row.PayloadIndex, Item);
    }

    if (_PayloadList.IsValid())
    { _PayloadList->RequestListRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRebuild_BlobDetail()
    -> void
{
    using namespace ck_save_debugger_window;

    if (NOT _BlobDetailBox.IsValid())
    { return; }

    _BlobDetailBox->ClearChildren();
    _ValueRoots.Reset();

    const auto* Payload = _Model.TryGet_SelectedPayload();
    const auto* Entity = _Model.TryGet_SelectedEntity();

    const auto HasActorBlob = Entity != nullptr && Entity->Get_ActorSaveFieldByteCount() > 0;

    if (Payload == nullptr && NOT HasActorBlob)
    {
        _BlobDetailBox->AddSlot()
        .AutoHeight()
        [
            Build_EmptyState(ECk_Icon::Payload, TEXT("Select a payload to inspect its blob."))
        ];

        if (_ValueTree.IsValid())
        { _ValueTree->RequestTreeRefresh(); }

        return;
    }

    auto BlobRows = SNew(SVerticalBox);
    auto DecodeRows = SNew(SVerticalBox);

    const auto AddRow = [](const TSharedRef<SVerticalBox>& InBox, const FString& InKey, const FString& InValue) -> void
    {
        InBox->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(InKey))
            .ValueText(FText::FromString(InValue))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::Text())
        ];
    };

    const auto Bytes = DoGet_SelectedBlobBytes();

    auto DecodeStatusText = FString{};
    auto DecodeStatusTone = ECk_Tone::Neutral;

    if (Payload != nullptr)
    {
        const auto* Decode = _Model.TryGet_PayloadDecode(Payload->Get_TableIndex());

        if (Decode != nullptr)
        {
            DecodeStatusText = ck::snapshot::Get_DecodeStatusText(Decode->Get_Status());
            DecodeStatusTone = ck_save_debugger_model::Get_DecodeStatusTone(Decode->Get_Status());
        }
        else if (NOT Payload->Get_TypeAvailable())
        {
            // Neutral on purpose: the file is fine, this editor simply cannot see the type it names.
            DecodeStatusText = TEXT("TYPE UNAVAILABLE");
            DecodeStatusTone = ECk_Tone::Neutral;
        }
        else
        {
            DecodeStatusText = ck::snapshot::Get_DecodeStatusText(ECk_SnapshotInspection_DecodeStatus::NotRequested);
            DecodeStatusTone = ECk_Tone::Neutral;
        }
    }

    if (Payload != nullptr)
    {
        const auto& TypePath = Payload->Get_Entry().Get_TypePath();

        AddRow(BlobRows, TEXT("Owner saved id"), ck_save_debugger_model::Build_SavedIdText(Payload->Get_Entry().Get_OwnerSavedId()));

        BlobRows->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(TEXT("Declared type")))
            .ValueText(FText::FromString(SCkDebug_NameLabel::Get_ShortName(TypePath, _NameDepth)))
            .ToolTipText(FText::FromString(TypePath))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::Value_Object())
        ];

        AddRow(BlobRows, TEXT("Type available"), Payload->Get_TypeAvailable() ? TEXT("yes") : TEXT("no (not loaded in this editor)"));
        AddRow(BlobRows, TEXT("SHA-256"), Payload->Get_PayloadHashHex());

        if (const auto* Decode = _Model.TryGet_PayloadDecode(Payload->Get_TableIndex()))
        {
            AddRow(DecodeRows, TEXT("Decoded type"), Decode->Get_DecodedTypePath());

            if (NOT Decode->Get_ErrorText().IsEmpty())
            { AddRow(DecodeRows, TEXT("Decode error"), Decode->Get_ErrorText()); }

            if (Decode->Get_ValueTreeRoot().IsValid())
            { _ValueRoots.Add(Decode->Get_ValueTreeRoot()); }
        }
        else
        {
            AddRow(DecodeRows, TEXT("Decoded type"), TEXT("(not requested)"));
        }
    }
    else
    {
        AddRow(BlobRows, TEXT("Blob"), TEXT("Actor UPROPERTY(SaveGame) fields"));
        AddRow(BlobRows, TEXT("Actor class"), Entity->Get_Entry().Get_ActorClassPath());
        AddRow(BlobRows, TEXT("Why opaque"), TEXT("class-scoped SerializeScriptProperties capture — needs a live object instance"));
        AddRow(BlobRows, TEXT("SHA-256"), ck::snapshot::Get_Sha256Hex(Bytes));
    }

    AddRow(BlobRows, TEXT("Byte count"), ck::Format_UE(TEXT("{}"), Bytes.Num()));

    const auto AddAction = [](const TSharedRef<SWrapBox>& InBox,
                              ECk_Icon InIconId,
                              const FString& InLabel,
                              const FString& InTooltip,
                              FOnClicked InOnClicked,
                              bool InEnabled) -> void
    {
        InBox->AddSlot()
        .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS})
        [
            ck_save_debugger_window::Build_CommandButton(InIconId, InLabel, InTooltip, InOnClicked, InEnabled)
        ];
    };

    auto DecodeActions = SNew(SWrapBox).UseAllottedSize(true);

    AddAction(DecodeActions, ECk_Icon::Settings, TEXT("Try Decode"),
        TEXT("Project this payload into a value tree using only types this editor already has loaded"),
        FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnTryDecodeClicked),
        Payload != nullptr);

    AddAction(DecodeActions, ECk_Icon::Lighting, TEXT("Try Load Type"),
        TEXT("Same decode, but allow the type's package to be loaded first"),
        FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnTryLoadTypeClicked),
        Payload != nullptr);

    AddAction(DecodeActions, ECk_Icon::Report, TEXT("Copy Type Path"),
        TEXT("Copy the type path the save declares for this payload"),
        FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnCopyTypePathClicked),
        Payload != nullptr);

    DecodeRows->AddSlot()
    .AutoHeight()
    .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
    [
        DecodeActions
    ];

    auto ByteActions = SNew(SWrapBox).UseAllottedSize(true);

    AddAction(ByteActions, ECk_Icon::Report, TEXT("Copy Metadata"),
        TEXT("Copy every readable fact about this blob to the clipboard"),
        FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnCopyMetadataClicked),
        true);

    AddAction(ByteActions, ECk_Icon::Log, TEXT("Export Raw Blob..."),
        TEXT("Write the blob's bytes to disk exactly as the save holds them"),
        FOnClicked::CreateSP(this, &SCkSaveDebuggerWindow::DoOnExportBlobClicked),
        Bytes.Num() > 0);

    const auto HexPreview = ck_save_debugger_model::Format_HexPreview(
        Bytes, ck_save_debugger_model::k_MaxHexPreviewBytes);

    auto ByteRows = SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            // Multi-line, so a SelectableLabel cannot host it — right-click copy carries the whole preview instead.
            SNew(SCkDebug_CopyableContainer)
            .CopyText(HexPreview)
            [
                SNew(SBorder)
                .BorderImage(Get_PanelBrush())
                .BorderBackgroundColor(FSlateColor{FLinearColor::White})
                .Padding(FMargin{CkStyle::SpaceS})
                [
                    SNew(SBox)
                    .HeightOverride(k_HexPanelHeight)
                    [
                        SNew(SScrollBox)
                        + SScrollBox::Slot()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(HexPreview))
                            .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                        ]
                    ]
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
        [
            ByteActions
        ];

    // The blob panel reports what the SAVE says about the type; the decode panel below reports what THIS EDITOR
    // could make of it. Neither is red for a type the editor simply does not have loaded.
    const auto BlobPillText = Payload == nullptr
        ? FString{TEXT("OPAQUE")}
        : Payload->Get_TypeAvailable() ? FString{TEXT("TYPE OK")} : FString{TEXT("TYPE UNAVAILABLE")};

    const auto BlobPillTone = Payload != nullptr && Payload->Get_TypeAvailable()
        ? ECk_Tone::Ok
        : ECk_Tone::Neutral;

    _BlobDetailBox->AddSlot()
    .AutoHeight()
    [
        SNew(SCkDebug_InspectorPanel)
        .Title(FText::FromString(Payload != nullptr ? TEXT("Payload") : TEXT("Actor SaveGame Blob")))
        .IconBrush(Get_IconBrush(Payload != nullptr ? ECk_Icon::Payload : ECk_Icon::Locked))
        .IconColor(CkStyle::Accent())
        .StatusPillText(FText::FromString(BlobPillText))
        .StatusPillTone(BlobPillTone)
        .Body()
        [
            BlobRows
        ]
    ];

    if (Payload != nullptr)
    {
        _BlobDetailBox->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_InspectorPanel)
            .Title(FText::FromString(TEXT("Decode")))
            .IconBrush(Get_IconBrush(ECk_Icon::Settings))
            .IconColor(CkStyle::Accent())
            .StatusPillText(FText::FromString(DecodeStatusText))
            .StatusPillTone(DecodeStatusTone)
            .Body()
            [
                DecodeRows
            ]
        ];
    }

    _BlobDetailBox->AddSlot()
    .AutoHeight()
    [
        SNew(SCkDebug_InspectorPanel)
        .Title(FText::FromString(TEXT("Raw Bytes")))
        .IconBrush(Get_IconBrush(ECk_Icon::Locked))
        .IconColor(CkStyle::Accent())
        .CountText(FText::FromString(ck::Format_UE(TEXT("{} B"), Bytes.Num())))
        .StartExpanded(false)
        .Body()
        [
            ByteRows
        ]
    ];

    if (_ValueTree.IsValid())
    {
        _ValueTree->RequestTreeRefresh();

        auto Stack = _ValueRoots;
        while (Stack.Num() > 0)
        {
            const auto Node = Stack.Pop();
            if (NOT Node.IsValid())
            { continue; }

            _ValueTree->SetItemExpansion(Node, true);
            Stack.Append(Node->Get_Children());
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRebuild_Diagnostics()
    -> void
{
    _DiagnosticRows.Reset();

    auto Existing = MoveTemp(_DiagnosticRowsByIndex);
    _DiagnosticRowsByIndex.Reset();

    for (const auto& Row : _Model.Build_DiagnosticRows())
    {
        // Filtered HERE rather than in the model: the severity latches are a view preference, and the model's
        // row set stays the full analysis so the header counts keep reporting what the file actually contains.
        if (NOT Get_SeverityShown(Row.Severity))
        { continue; }

        auto Item = TSharedPtr<FCkSaveDebugger_DiagnosticRow>{};
        if (auto* Found = Existing.Find(Row.DiagnosticIndex))
        { Item = *Found; }
        else
        { Item = MakeShared<FCkSaveDebugger_DiagnosticRow>(); }

        *Item = Row;
        _DiagnosticRows.Add(Item);
        _DiagnosticRowsByIndex.Add(Row.DiagnosticIndex, Item);
    }

    if (_DiagnosticList.IsValid())
    { _DiagnosticList->RequestListRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRebuild_Diff()
    -> void
{
    _DiffGroupRows.Reset();

    auto Existing = MoveTemp(_DiffGroupRowsByPath);
    _DiffGroupRowsByPath.Reset();

    auto SetChanged = false;

    // Identity path is the key a group survives a re-diff under; saved ids are not stable across captures, which is
    // exactly why the analysis groups by identity in the first place.
    for (const auto& Row : _Model.Build_DiffGroupRows())
    {
        auto Item = TSharedPtr<FCkSaveDebugger_DiffGroupRow>{};
        if (auto* Found = Existing.Find(Row.IdentityPath))
        {
            Item = *Found;
            Existing.Remove(Row.IdentityPath);
        }
        else
        {
            Item = MakeShared<FCkSaveDebugger_DiffGroupRow>();
            SetChanged = true;
        }

        *Item = Row;
        _DiffGroupRows.Add(Item);
        _DiffGroupRowsByPath.Add(Row.IdentityPath, Item);
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    if (NOT _DiffGroupRowsByPath.Contains(_DiffSelectedIdentityPath))
    { _DiffSelectedIdentityPath.Reset(); }

    if (SetChanged && _DiffGroupList.IsValid())
    { _DiffGroupList->RequestListRefresh(); }

    DoRebuild_DiffDetail();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoRebuild_DiffDetail()
    -> void
{
    using namespace ck_save_debugger_window;

    if (NOT _DiffDetailBox.IsValid())
    { return; }

    _DiffDetailBox->ClearChildren();

    const auto* Selected = _DiffGroupRowsByPath.Find(_DiffSelectedIdentityPath);
    if (Selected == nullptr || NOT Selected->IsValid())
    {
        _DiffDetailBox->AddSlot()
        .AutoHeight()
        [
            Build_EmptyState(ECk_Icon::Size,
                TEXT("Select a group to see which payload types moved inside it."))
        ];

        return;
    }

    const auto& Row = **Selected;
    const auto& Groups = _Model.Get_Diff().Get_EntityGroups();

    if (NOT Groups.IsValidIndex(Row.GroupIndex))
    { return; }

    const auto& Group = Groups[Row.GroupIndex];

    auto TypeRows = SNew(SVerticalBox);

    for (const auto& PayloadType : Group.Get_PayloadTypes())
    {
        TypeRows->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(ck_save_debugger_model::Build_ShortTypeName(PayloadType.Get_TypePath())))
            .ValueText(FText::FromString(ck_save_debugger_model::Build_DiffPayloadTypeText(PayloadType)))
            .ToolTipText(FText::FromString(PayloadType.Get_TypePath()))
            .Tone(ECkDebug_KeyValueTone::Custom)
            // Content that differs at an identical byte count is the case only the hash can see — give it the
            // accent so it does not read as an unchanged row.
            .CustomValueColor(PayloadType.Get_ContentDiffers() ? CkStyle::Accent() : CkStyle::Text())
        ];
    }

    if (Group.Get_PayloadTypes().Num() == 0)
    {
        TypeRows->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(TEXT("payloads")))
            .ValueText(FText::FromString(TEXT("(none on either side)")))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::TextMute())
        ];
    }

    auto IdentityRows = SNew(SVerticalBox);

    IdentityRows->AddSlot()
    .AutoHeight()
    [
        SNew(SCkDebug_KeyValueRow)
        .KeyText(FText::FromString(TEXT("Identity path")))
        .ValueText(FText::FromString(Row.IdentityPath))
        .Tone(ECkDebug_KeyValueTone::Custom)
        .CustomValueColor(CkStyle::Value_Object())
    ];

    IdentityRows->AddSlot()
    .AutoHeight()
    [
        SNew(SCkDebug_KeyValueRow)
        .KeyText(FText::FromString(TEXT("Count")))
        .ValueText(FText::FromString(ck_save_debugger_model::Build_DiffCountText(Row.CountBaseline, Row.CountCurrent)))
        .Tone(ECkDebug_KeyValueTone::Custom)
        .CustomValueColor(CkStyle::GetToneColor(Row.Tone))
    ];

    IdentityRows->AddSlot()
    .AutoHeight()
    [
        SNew(SCkDebug_KeyValueRow)
        .KeyText(FText::FromString(TEXT("Payload bytes")))
        .ValueText(FText::FromString(ck_save_debugger_model::Build_DiffBytesText(
            Row.PayloadBytesBaseline, Row.PayloadBytesCurrent)))
        .Tone(ECkDebug_KeyValueTone::Custom)
        .CustomValueColor(CkStyle::Text())
    ];

    _DiffDetailBox->AddSlot()
    .AutoHeight()
    [
        SNew(SCkDebug_InspectorPanel)
        .Title(FText::FromString(Row.DisplayName))
        .IconBrush(Get_IconBrush(ECk_Icon::Size))
        .IconColor(CkStyle::Accent())
        .StatusPillText(FText::FromString(Row.KindText))
        .StatusPillTone(Row.Tone)
        .Body()
        [
            IdentityRows
        ]
    ];

    _DiffDetailBox->AddSlot()
    .AutoHeight()
    [
        SNew(SCkDebug_InspectorPanel)
        .Title(FText::FromString(TEXT("Payload Types")))
        .IconBrush(Get_IconBrush(ECk_Icon::Payload))
        .IconColor(CkStyle::Accent())
        .CountText(FText::FromString(ck::Format_UE(TEXT("{}"), Group.Get_PayloadTypes().Num())))
        .Body()
        [
            TypeRows
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoSelect_Entity(
        uint32 InSavedId)
    -> void
{
    if (_Model.Get_SelectedEntitySavedId() == InSavedId)
    { return; }

    _Model.Set_SelectedEntitySavedId(InSavedId);
    _Model.Set_SelectedPayloadIndex(INDEX_NONE);

    if (_EntityTree.IsValid())
    {
        // The tree owns no key index of its own — walk the visible forest once and select the matching row.
        auto Stack = _VisibleRoots;
        while (Stack.Num() > 0)
        {
            const auto Node = Stack.Pop();
            if (NOT Node.IsValid())
            { continue; }

            if (Node->Kind == ECkSaveDebugger_NodeKind::Entity && Node->SavedId == InSavedId)
            {
                const auto Current = _EntityTree->GetSelectedItems();
                const auto AlreadySelected = Current.Num() == 1 && Current[0] == Node;

                if (NOT AlreadySelected)
                {
                    _SuppressSelectionEcho = true;
                    _EntityTree->SetItemSelection(Node, true, ESelectInfo::Direct);
                    _EntityTree->RequestScrollIntoView(Node);
                    _SuppressSelectionEcho = false;
                }

                break;
            }

            Stack.Append(Node->Children);
        }
    }

    DoVisualize_SyncSelection();

    DoRebuild_EntityDetail();
    DoRebuild_BlobDetail();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoSelect_Payload(
        int32 InPayloadIndex)
    -> void
{
    _Model.Set_SelectedPayloadIndex(InPayloadIndex);

    if (_PayloadList.IsValid())
    {
        if (const auto* Found = _PayloadRowsByIndex.Find(InPayloadIndex))
        {
            const auto Current = _PayloadList->GetSelectedItems();
            const auto AlreadySelected = Current.Num() == 1 && Current[0] == *Found;

            if (NOT AlreadySelected)
            {
                _SuppressSelectionEcho = true;
                _PayloadList->SetItemSelection(*Found, true, ESelectInfo::Direct);
                _PayloadList->RequestScrollIntoView(*Found);
                _SuppressSelectionEcho = false;
            }
        }
    }

    DoRebuild_BlobDetail();
}

// --------------------------------------------------------------------------------------------------------------------
// Views
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoGenerate_EntityRow(
        TSharedPtr<FCkSaveDebugger_TreeNode> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_save_debugger_window;

    const auto WeakNode = TWeakPtr<FCkSaveDebugger_TreeNode>{InItem};

    // The glyph is resolved once from the row's own model-assigned id; a row's kind and provenance never change
    // under a stable key, so only the tints need to be attribute-bound.
    const auto IconBrush = InItem.IsValid() ? Get_IconBrush(InItem->Icon) : nullptr;

    const auto IconMeaning = NOT InItem.IsValid()
        ? FString{}
        : InItem->Kind == ECkSaveDebugger_NodeKind::Entity
            ? ck::Format_UE(TEXT("Captured as {}"), InItem->ProvenanceText)
            : InItem->DisplayText;

    return SNew(STableRow<TSharedPtr<FCkSaveDebugger_TreeNode>>, InOwnerTable)
        .Style(&Get_RowStyle())
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        .Content()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(CkStyle::GetRoundedBrush_Pill())
                .Meaning(FText::FromString(TEXT("This row, or something under it, is named by an Error or Warning diagnostic")))
                .ColorAndOpacity_Lambda([WeakNode]() -> FSlateColor
                {
                    const auto Node = WeakNode.Pin();
                    if (NOT Node.IsValid() || NOT Node->HasProblems)
                    { return FSlateColor{FLinearColor::Transparent}; }

                    return FSlateColor{CkStyle::Err()};
                })
                .Size(FVector2D{k_ProblemDotSize, k_ProblemDotSize})
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(IconBrush)
                .Meaning(FText::FromString(IconMeaning))
                .ColorAndOpacity_Lambda([WeakNode]() -> FSlateColor
                {
                    const auto Node = WeakNode.Pin();
                    if (NOT Node.IsValid())
                    { return FSlateColor{CkStyle::TextMute()}; }

                    // A Non-Persisted Owner group is normal top-level shape — only a cycle earns the error tone.
                    const auto Tone = ck_save_debugger_model::Get_NodeKindTone(Node->Kind);
                    return FSlateColor{Tone == ECk_Tone::Neutral ? CkStyle::TextMute() : CkStyle::GetToneColor(Tone)};
                })
                .Size(FVector2D{k_RowIconSize, k_RowIconSize})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([WeakNode]() -> FText
                {
                    const auto Node = WeakNode.Pin();
                    return Node.IsValid() ? FText::FromString(Node->DisplayText) : FText::GetEmpty();
                })
                .ColorAndOpacity_Lambda([WeakNode]() -> FSlateColor
                {
                    const auto Node = WeakNode.Pin();
                    if (NOT Node.IsValid())
                    { return Get_RowTextColor(false); }

                    if (ck_save_debugger_model::Get_NodeKindTone(Node->Kind) == ECk_Tone::Err)
                    { return FSlateColor{CkStyle::Err()}; }

                    return Get_RowTextColor(Node->IsSearchMatch);
                })
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(k_ProvenanceWidth)
                [
                    SNew(STextBlock)
                    .Text_Lambda([WeakNode]() -> FText
                    {
                        const auto Node = WeakNode.Pin();
                        return Node.IsValid() ? FText::FromString(Node->ProvenanceText) : FText::GetEmpty();
                    })
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoGet_EntityChildren(
        TSharedPtr<FCkSaveDebugger_TreeNode> InItem,
        TArray<TSharedPtr<FCkSaveDebugger_TreeNode>>& OutChildren)
    -> void
{
    if (NOT InItem.IsValid())
    { return; }

    for (const auto& Child : InItem->Children)
    {
        if (Child.IsValid() && Child->IsVisible)
        { OutChildren.Add(Child); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnEntitySelectionChanged(
        TSharedPtr<FCkSaveDebugger_TreeNode> InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (_SuppressSelectionEcho || InSelectInfo == ESelectInfo::Direct)
    { return; }

    if (NOT InItem.IsValid() || InItem->Kind != ECkSaveDebugger_NodeKind::Entity)
    { return; }

    _Model.Set_SelectedEntitySavedId(InItem->SavedId);
    _Model.Set_SelectedPayloadIndex(INDEX_NONE);

    DoVisualize_SyncSelection();

    DoRebuild_EntityDetail();
    DoRebuild_BlobDetail();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnEntityContextMenu()
    -> TSharedPtr<SWidget>
{
    if (NOT _EntityTree.IsValid())
    { return nullptr; }

    const auto Selected = _EntityTree->GetSelectedItems();
    if (Selected.Num() == 0)
    { return nullptr; }

    auto Names = TArray<FString>{};
    auto Ids = TArray<FString>{};

    for (const auto& Node : Selected)
    {
        if (NOT Node.IsValid())
        { continue; }

        Names.Add(Node->DisplayText);
        Ids.Add(ck_save_debugger_model::Build_SavedIdText(Node->SavedId));
    }

    if (Names.Num() == 0)
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Identity")),
        FText::FromString(TEXT("Copy the selected row's identity text")),
        FString::Join(Names, TEXT("\n")));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Saved Id")),
        FText::FromString(TEXT("Copy the selected row's saved id")),
        FString::Join(Ids, TEXT("\n")));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoGenerate_PayloadRow(
        TSharedPtr<FCkSaveDebugger_PayloadRow> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_save_debugger_window;

    const auto WeakRow = TWeakPtr<FCkSaveDebugger_PayloadRow>{InItem};

    return SNew(STableRow<TSharedPtr<FCkSaveDebugger_PayloadRow>>, InOwnerTable)
        .Style(&Get_RowStyle())
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        .ToolTipText_Lambda([WeakRow]() -> FText
        {
            const auto Row = WeakRow.Pin();
            return Row.IsValid() ? FText::FromString(Row->TypePath) : FText::GetEmpty();
        })
        .Content()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(Get_IconBrush(ECk_Icon::Payload))
                .Meaning(FText::FromString(TEXT("A replicated-data blob the save holds for this entity")))
                .ColorAndOpacity_Lambda([WeakRow]() -> FSlateColor
                {
                    const auto Row = WeakRow.Pin();
                    return FSlateColor{Row.IsValid() && Row->TypeAvailable ? CkStyle::Ok() : CkStyle::TextMute()};
                })
                .Size(FVector2D{k_RowIconSize, k_RowIconSize})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([this, WeakRow]() -> FText
                {
                    const auto Row = WeakRow.Pin();
                    if (NOT Row.IsValid())
                    { return FText::GetEmpty(); }

                    return FText::FromString(SCkDebug_NameLabel::Get_ShortName(Row->TypePath, _NameDepth));
                })
                .ColorAndOpacity_Lambda([WeakRow]() -> FSlateColor
                {
                    const auto Row = WeakRow.Pin();
                    return FSlateColor{Row.IsValid() && NOT Row->TypeAvailable ? CkStyle::TextMute() : CkStyle::Text()};
                })
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(k_ByteColumnWidth)
                [
                    SNew(STextBlock)
                    .Text_Lambda([WeakRow]() -> FText
                    {
                        const auto Row = WeakRow.Pin();
                        return Row.IsValid()
                            ? FText::FromString(ck::Format_UE(TEXT("{} B"), Row->ByteCount))
                            : FText::GetEmpty();
                    })
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_StatusPill)
                .Text_Lambda([WeakRow]() -> FText
                {
                    const auto Row = WeakRow.Pin();
                    if (NOT Row.IsValid())
                    { return FText::GetEmpty(); }

                    return FText::FromString(Row->TypeAvailable ? TEXT("TYPE OK") : TEXT("TYPE UNAVAILABLE"));
                })
                .Tone_Lambda([WeakRow]() -> ECk_Tone
                {
                    const auto Row = WeakRow.Pin();
                    return Row.IsValid() && Row->TypeAvailable ? ECk_Tone::Ok : ECk_Tone::Neutral;
                })
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnPayloadSelectionChanged(
        TSharedPtr<FCkSaveDebugger_PayloadRow> InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (_SuppressSelectionEcho || InSelectInfo == ESelectInfo::Direct)
    { return; }

    if (NOT InItem.IsValid())
    { return; }

    _Model.Set_SelectedPayloadIndex(InItem->PayloadIndex);
    DoRebuild_BlobDetail();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnPayloadContextMenu()
    -> TSharedPtr<SWidget>
{
    if (NOT _PayloadList.IsValid())
    { return nullptr; }

    const auto Selected = _PayloadList->GetSelectedItems();
    if (Selected.Num() == 0 || NOT Selected[0].IsValid())
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Type Path")),
        FText::FromString(TEXT("Copy the type path the save declares for this payload")),
        Selected[0]->TypePath);

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoGenerate_DiagnosticRow(
        TSharedPtr<FCkSaveDebugger_DiagnosticRow> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_save_debugger_window;

    const auto WeakRow = TWeakPtr<FCkSaveDebugger_DiagnosticRow>{InItem};

    const auto SeverityColor = TAttribute<FSlateColor>::CreateLambda([WeakRow]() -> FSlateColor
    {
        const auto Row = WeakRow.Pin();
        if (NOT Row.IsValid())
        { return FSlateColor{CkStyle::TextMute()}; }

        return FSlateColor{CkStyle::GetToneColor(ck_save_debugger_model::Get_SeverityTone(Row->Severity))};
    });

    return SNew(STableRow<TSharedPtr<FCkSaveDebugger_DiagnosticRow>>, InOwnerTable)
        .Style(&Get_RowStyle())
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        .Content()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(CkStyle::GetRoundedBrush_Pill())
                .Meaning(FText::FromString(TEXT("Severity the inspection analyzer assigned this diagnostic")))
                .ColorAndOpacity(SeverityColor)
                .Size(FVector2D{k_ProblemDotSize, k_ProblemDotSize})
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(k_SeverityWidth)
                [
                    SNew(STextBlock)
                    .Text_Lambda([WeakRow]() -> FText
                    {
                        const auto Row = WeakRow.Pin();
                        return Row.IsValid()
                            ? FText::FromString(ck::snapshot::Get_SeverityText(Row->Severity))
                            : FText::GetEmpty();
                    })
                    .ColorAndOpacity(SeverityColor)
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(k_CodeWidth)
                [
                    // Registry brush, tinted per row — a badge allocated here would leak once per generated row.
                    SNew(SBorder)
                    .BorderImage(Get_BadgeBrush())
                    .BorderBackgroundColor_Lambda([WeakRow]() -> FSlateColor
                    {
                        const auto Row = WeakRow.Pin();
                        if (NOT Row.IsValid())
                        { return FSlateColor{CkStyle::Bg2()}; }

                        return FSlateColor{CkStyle::GetToneDimColor(
                            ck_save_debugger_model::Get_SeverityTone(Row->Severity))};
                    })
                    .Padding(FMargin{CkStyle::SpaceS, 0.0f})
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([WeakRow]() -> FText
                        {
                            const auto Row = WeakRow.Pin();
                            return Row.IsValid() ? FText::FromString(Row->CodeText) : FText::GetEmpty();
                        })
                        .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                        .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    ]
                ]
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([WeakRow]() -> FText
                {
                    const auto Row = WeakRow.Pin();
                    return Row.IsValid() ? FText::FromString(Row->Message) : FText::GetEmpty();
                })
                .ColorAndOpacity_Lambda([WeakRow]() -> FSlateColor
                {
                    const auto Row = WeakRow.Pin();
                    if (NOT Row.IsValid())
                    { return FSlateColor{CkStyle::Text()}; }

                    // Info reads as ordinary body text; only Error and Warning earn their tone on the message.
                    const auto Tone = ck_save_debugger_model::Get_SeverityTone(Row->Severity);
                    return FSlateColor{Tone == ECk_Tone::Info ? CkStyle::Text() : CkStyle::GetToneColor(Tone)};
                })
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnDiagnosticSelectionChanged(
        TSharedPtr<FCkSaveDebugger_DiagnosticRow> InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (_SuppressSelectionEcho || InSelectInfo == ESelectInfo::Direct)
    { return; }

    if (NOT InItem.IsValid())
    { return; }

    if (InItem->Target.Get_HasEntity())
    { DoSelect_Entity(InItem->Target.EntitySavedId); }

    if (InItem->Target.Get_HasPayload())
    { DoSelect_Payload(InItem->Target.PayloadIndex); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnDiagnosticContextMenu()
    -> TSharedPtr<SWidget>
{
    if (NOT _DiagnosticList.IsValid())
    { return nullptr; }

    const auto Selected = _DiagnosticList->GetSelectedItems();
    if (Selected.Num() == 0)
    { return nullptr; }

    auto Lines = TArray<FString>{};
    for (const auto& Row : Selected)
    {
        if (NOT Row.IsValid())
        { continue; }

        Lines.Add(ck::Format_UE(TEXT("[{}] {} - {}"),
            ck::snapshot::Get_SeverityText(Row->Severity),
            Row->CodeText,
            Row->Message));
    }

    if (Lines.Num() == 0)
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Diagnostic")),
        FText::FromString(TEXT("Copy the selected diagnostic line(s)")),
        FString::Join(Lines, TEXT("\n")));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoGenerate_DiffGroupRow(
        TSharedPtr<FCkSaveDebugger_DiffGroupRow> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_save_debugger_window;

    const auto WeakRow = TWeakPtr<FCkSaveDebugger_DiffGroupRow>{InItem};

    const auto ToneColor = TAttribute<FSlateColor>::CreateLambda([WeakRow]() -> FSlateColor
    {
        const auto Row = WeakRow.Pin();
        if (NOT Row.IsValid())
        { return FSlateColor{CkStyle::TextMute()}; }

        return FSlateColor{CkStyle::GetToneColor(Row->Tone)};
    });

    return SNew(STableRow<TSharedPtr<FCkSaveDebugger_DiffGroupRow>>, InOwnerTable)
        .Style(&Get_RowStyle())
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        // The identity path is what the two saves were matched BY — it belongs on the row, not squeezed into it.
        .ToolTipText_Lambda([WeakRow]() -> FText
        {
            const auto Row = WeakRow.Pin();
            return Row.IsValid() ? FText::FromString(Row->IdentityPath) : FText::GetEmpty();
        })
        .Content()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(k_DiffKindWidth)
                [
                    // Registry brush, tinted per row — a badge allocated here would leak once per generated row.
                    SNew(SBorder)
                    .BorderImage(Get_BadgeBrush())
                    .BorderBackgroundColor_Lambda([WeakRow]() -> FSlateColor
                    {
                        const auto Row = WeakRow.Pin();
                        if (NOT Row.IsValid())
                        { return FSlateColor{CkStyle::Bg2()}; }

                        return FSlateColor{CkStyle::GetToneDimColor(Row->Tone)};
                    })
                    .Padding(FMargin{CkStyle::SpaceS, 0.0f})
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([WeakRow]() -> FText
                        {
                            const auto Row = WeakRow.Pin();
                            return Row.IsValid() ? FText::FromString(Row->KindText) : FText::GetEmpty();
                        })
                        .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                        .ColorAndOpacity(ToneColor)
                    ]
                ]
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([WeakRow]() -> FText
                {
                    const auto Row = WeakRow.Pin();
                    return Row.IsValid() ? FText::FromString(Row->DisplayName) : FText::GetEmpty();
                })
                .ColorAndOpacity(FSlateColor{CkStyle::Text()})
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(k_DiffCountWidth)
                [
                    SNew(STextBlock)
                    .Text_Lambda([WeakRow]() -> FText
                    {
                        const auto Row = WeakRow.Pin();
                        if (NOT Row.IsValid())
                        { return FText::GetEmpty(); }

                        return FText::FromString(ck_save_debugger_model::Build_DiffCountText(
                            Row->CountBaseline, Row->CountCurrent));
                    })
                    .ColorAndOpacity(ToneColor)
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(k_DiffBytesWidth)
                [
                    SNew(STextBlock)
                    .Text_Lambda([WeakRow]() -> FText
                    {
                        const auto Row = WeakRow.Pin();
                        if (NOT Row.IsValid())
                        { return FText::GetEmpty(); }

                        return FText::FromString(ck_save_debugger_model::Build_DiffBytesText(
                            Row->PayloadBytesBaseline, Row->PayloadBytesCurrent));
                    })
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnDiffGroupSelectionChanged(
        TSharedPtr<FCkSaveDebugger_DiffGroupRow> InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (_SuppressSelectionEcho || InSelectInfo == ESelectInfo::Direct)
    { return; }

    if (NOT InItem.IsValid())
    { return; }

    _DiffSelectedIdentityPath = InItem->IdentityPath;

    // A Removed group names nothing the OPEN save contains, so there is no tree row to land on — the detail panel
    // is the whole answer there.
    if (InItem->Get_HasCurrentMember())
    { DoSelect_Entity(InItem->FirstCurrentSavedId); }

    DoRebuild_DiffDetail();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoOnDiffGroupContextMenu()
    -> TSharedPtr<SWidget>
{
    if (NOT _DiffGroupList.IsValid())
    { return nullptr; }

    const auto Selected = _DiffGroupList->GetSelectedItems();
    if (Selected.Num() == 0)
    { return nullptr; }

    auto Lines = TArray<FString>{};
    auto Paths = TArray<FString>{};

    for (const auto& Row : Selected)
    {
        if (NOT Row.IsValid())
        { continue; }

        Lines.Add(ck::Format_UE(TEXT("[{}] {} | {} | {} | {}"),
            Row->KindText,
            Row->DisplayName,
            ck_save_debugger_model::Build_DiffCountText(Row->CountBaseline, Row->CountCurrent),
            ck_save_debugger_model::Build_DiffBytesText(Row->PayloadBytesBaseline, Row->PayloadBytesCurrent),
            Row->IdentityPath));

        Paths.Add(Row->IdentityPath);
    }

    if (Lines.Num() == 0)
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Group")),
        FText::FromString(TEXT("Copy the selected group's identity path and counts")),
        FString::Join(Lines, TEXT("\n")));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Identity Path")),
        FText::FromString(TEXT("Copy the selected group's identity path")),
        FString::Join(Paths, TEXT("\n")));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoGenerate_ValueRow(
        TSharedPtr<FCk_SnapshotInspection_ValueNode> InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_save_debugger_window;

    const auto WeakNode = TWeakPtr<FCk_SnapshotInspection_ValueNode>{InItem};

    return SNew(STableRow<TSharedPtr<FCk_SnapshotInspection_ValueNode>>, InOwnerTable)
        .Style(&Get_RowStyle())
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        .Content()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([WeakNode]() -> FText
                {
                    const auto Node = WeakNode.Pin();
                    return Node.IsValid() ? FText::FromString(Node->Get_FieldName()) : FText::GetEmpty();
                })
                .ColorAndOpacity(FSlateColor{CkStyle::Text()})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([WeakNode]() -> FText
                {
                    const auto Node = WeakNode.Pin();
                    return Node.IsValid() ? FText::FromString(Node->Get_ValueText()) : FText::GetEmpty();
                })
                .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor{CkStyle::Value_String()})
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBorder)
                .BorderImage(Get_BadgeBrush())
                .BorderBackgroundColor(FSlateColor{CkStyle::Bg2()})
                .Padding(FMargin{CkStyle::SpaceS, 0.0f})
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([WeakNode]() -> FText
                    {
                        const auto Node = WeakNode.Pin();
                        return Node.IsValid() ? FText::FromString(Node->Get_TypeName()) : FText::GetEmpty();
                    })
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoGet_ValueChildren(
        TSharedPtr<FCk_SnapshotInspection_ValueNode> InItem,
        TArray<TSharedPtr<FCk_SnapshotInspection_ValueNode>>& OutChildren)
    -> void
{
    if (NOT InItem.IsValid())
    { return; }

    OutChildren.Append(InItem->Get_Children());
}

// --------------------------------------------------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoSet_Status(
        const FString& InText,
        ECk_Tone InTone)
    -> void
{
    _StatusTone = InTone;

    if (ck::IsValid(_StatusText))
    { _StatusText->SetText(FText::FromString(InText)); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoBuild_SelectedBlobMetadata() const
    -> FString
{
    const auto* Payload = _Model.TryGet_SelectedPayload();
    const auto Bytes = DoGet_SelectedBlobBytes();

    auto Lines = TArray<FString>{};

    if (Payload != nullptr)
    {
        Lines.Add(ck::Format_UE(TEXT("payload index : {}"), Payload->Get_TableIndex()));
        Lines.Add(ck::Format_UE(TEXT("owner saved id: {}"),
            ck_save_debugger_model::Build_SavedIdText(Payload->Get_Entry().Get_OwnerSavedId())));
        Lines.Add(ck::Format_UE(TEXT("declared type : {}"), Payload->Get_Entry().Get_TypePath()));
        Lines.Add(ck::Format_UE(TEXT("type available: {}"), Payload->Get_TypeAvailable() ? TEXT("yes") : TEXT("no")));
        Lines.Add(ck::Format_UE(TEXT("sha256        : {}"), Payload->Get_PayloadHashHex()));

        if (const auto* Decode = _Model.TryGet_PayloadDecode(Payload->Get_TableIndex()))
        {
            Lines.Add(ck::Format_UE(TEXT("decode status : {}"), ck::snapshot::Get_DecodeStatusText(Decode->Get_Status())));
            Lines.Add(ck::Format_UE(TEXT("decoded type  : {}"), Decode->Get_DecodedTypePath()));
            Lines.Add(ck::Format_UE(TEXT("decode error  : {}"), Decode->Get_ErrorText()));
        }
    }
    else
    {
        const auto* Entity = _Model.TryGet_SelectedEntity();
        if (Entity == nullptr)
        { return {}; }

        Lines.Add(TEXT("blob          : actor UPROPERTY(SaveGame) fields"));
        Lines.Add(ck::Format_UE(TEXT("owner saved id: {}"),
            ck_save_debugger_model::Build_SavedIdText(Entity->Get_Entry().Get_SavedId())));
        Lines.Add(ck::Format_UE(TEXT("actor class   : {}"), Entity->Get_Entry().Get_ActorClassPath()));
        Lines.Add(ck::Format_UE(TEXT("sha256        : {}"), ck::snapshot::Get_Sha256Hex(Bytes)));
        Lines.Add(TEXT("decode status : UnsupportedBlob (needs a live object instance)"));
    }

    Lines.Add(ck::Format_UE(TEXT("byte count    : {}"), Bytes.Num()));
    Lines.Add(TEXT("hex preview:"));
    Lines.Add(ck_save_debugger_model::Format_HexPreview(Bytes, ck_save_debugger_model::k_MaxHexPreviewBytes));

    return FString::Join(Lines, TEXT("\n"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSaveDebuggerWindow::
    DoGet_SelectedBlobBytes() const
    -> TArray<uint8>
{
    if (const auto* Payload = _Model.TryGet_SelectedPayload())
    { return Payload->Get_Entry().Get_PayloadBytes(); }

    if (const auto* Entity = _Model.TryGet_SelectedEntity())
    { return Entity->Get_Entry().Get_ActorSaveFieldBytes(); }

    return {};
}

// --------------------------------------------------------------------------------------------------------------------
