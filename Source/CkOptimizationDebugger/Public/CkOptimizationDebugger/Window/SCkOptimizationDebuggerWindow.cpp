#include "CkOptimizationDebugger/Window/SCkOptimizationDebuggerWindow.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEditorTools/Style/CkIconStyle.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CategoryDot.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_UnderlineTabs.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_CleanupScan.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_LevelScan.h"
#include "CkOptimizationDebugger/Commands/CkOptimizationDebugger_CleanupCommands.h"
#include "CkOptimizationDebugger/Commands/CkOptimizationDebugger_ProfileCommands.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_MemoryScan.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_SnapshotCapture.h"
#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"
#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_Fixes.h"
#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_Navigation.h"
#include "CkOptimizationDebugger/Settings/CkOptimizationDebuggerSettings.h"

#include "CkEditorTools/Style/CkStyle.h"

#include <Framework/Commands/UIAction.h>
#include <Framework/MultiBox/MultiBoxBuilder.h>
#include <Textures/SlateIcon.h>
#include <Misc/DateTime.h>
#include <Misc/MessageDialog.h>
#include <Styling/AppStyle.h>
#include <UObject/Class.h>
#include <UObject/UnrealType.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SCheckBox.h>
#include <Widgets/Input/SEditableTextBox.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SSplitter.h>
#include <Widgets/Layout/SWidgetSwitcher.h>
#include <Widgets/Layout/SWrapBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/SNullWidget.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Views/SHeaderRow.h>
#include <Widgets/Views/STableRow.h>

// --------------------------------------------------------------------------------------------------------------------

const FName SCkOptimizationDebuggerWindow::WindowId = FName(TEXT("CkOptimizationDebugger"));

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_window
{
    constexpr auto k_StatusDotSize   = 8.0f;
    constexpr auto k_PanelIconSize   = 14.0f;
    constexpr auto k_RowIconSize     = 12.0f;
    constexpr auto k_CategoryDotSize = 8.0f;
    constexpr auto k_RowIndent       = 16.0f;

    // The over-budget filter's one step. It is the SAME multiple `Get_GraduatedSeverity` escalates at, so the
    // narrowing and the colouring in the list agree about what "badly over" means rather than being two opinions.
    constexpr auto k_OverBudgetRatioStep = 2.0f;

    // Dashboard geometry.
    constexpr auto k_EmptyStateIconSize      = 24.0f;
    constexpr auto k_MeterWidth              = 220.0f;
    constexpr auto k_MeterHeight             = 5.0f;
    constexpr auto k_LevelActorColumnWidth   = 70.0f;

    // Memory table geometry. The numeric columns are fixed because a right-aligned monospace figure that changes
    // width with the window is a figure the reader cannot scan down.
    constexpr auto k_MemoryTypeColumnWidth      = 150.0f;
    constexpr auto k_MemoryDimensionColumnWidth = 230.0f;
    constexpr auto k_MemorySizeColumnWidth      = 130.0f;
    constexpr auto k_MemoryGpuColumnWidth       = 110.0f;

    // The snapshot facts column. Fixed so the picture beside it does not resize as the facts change length.
    constexpr auto k_SnapshotFactsWidth = 260.0f;
    constexpr auto k_MemoryStreamingColumnWidth = 120.0f;
    constexpr auto k_MemoryRowMeterWidth        = 110.0f;
    constexpr auto k_MemoryRowMeterHeight       = 3.0f;

    // Cleanup list geometry. Hand-laid rather than an `SHeaderRow`, because this list carries group header LINES for
    // the duplicates category and a header row has nowhere to put one.
    constexpr auto k_CleanupClassColumnWidth = 140.0f;
    constexpr auto k_CleanupSizeColumnWidth  = 100.0f;

    // The findings list keys group headers under this prefix. A finding's own key is `<check id>|<target>`, and no
    // check id is literally "group", so the two key spaces cannot collide.
    const auto k_GroupKeyPrefix = FString{TEXT("group|")};

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_IconBrush(
            ECk_Icon InIcon)
        -> const FSlateBrush*
    {
        return FCkIconStyle::Get_Brush(InIcon, ECk_Icon_BrushSize::Size_16x16);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_RowStyle()
        -> const FTableRowStyle&
    {
        return FCkDebuggerStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("CkDebugger.TableView.Row"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The glyph a category is named by, everywhere it is named: the filter toggle, the group header and the row.
     *  One mapping so a category cannot mean two different pictures in two places. Every id resolves against
     *  the typed registry; `Ck.OptimizationDebugger.Window.CategoryIcons` pins the set as distinct. */
    auto
        Get_CategoryIcon(
            ECkOptimizationDebugger_Category InCategory)
        -> ECk_Icon
    {
        switch (InCategory)
        {
            case ECkOptimizationDebugger_Category::Mesh:            return ECk_Icon::Entity;
            case ECkOptimizationDebugger_Category::Texture:         return ECk_Icon::TextureAsset;
            case ECkOptimizationDebugger_Category::Material:        return ECk_Icon::MaterialAsset;
            case ECkOptimizationDebugger_Category::Lighting:        return ECk_Icon::Lighting;
            case ECkOptimizationDebugger_Category::Actor:           return ECk_Icon::Actor;
            case ECkOptimizationDebugger_Category::Blueprint:       return ECk_Icon::Fragment;
            case ECkOptimizationDebugger_Category::ProjectSettings: return ECk_Icon::Settings;
            default:                                                return ECk_Icon::Entity;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Severity's glyph comes off the SAME axis its colour does.
     *
     *  It used to be `Skull` / `Flame` / `Note`, picked out of the decorative `Icons/General/**` set. Two problems,
     *  and the second is the one that mattered: those are not the pictures UE uses for severity anywhere else, so a
     *  reader had to learn them here; and `Skull` was simultaneously "Critical" in this tool and "Failed" in the
     *  gallery and something else again in three sibling debuggers sharing one launcher.
     *
     *  `Get_SeverityTone` is already the model's rule, so routing the glyph through it means colour and picture
     *  cannot drift: one severity, one tone, one pair. */
    auto
        Get_SeverityIcon(
            ECkOptimizationDebugger_Severity InSeverity)
        -> ECk_Icon
    {
        return ck::debug_axes::Get_ToneIcon(
            ck_optimization_debugger_model::Get_SeverityTone(InSeverity));
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** One glyph per profiling shelf, not one per entry. Five Nanite modes drawn with five different pictures would
     *  be five pictures that mean nothing; the LABEL is what tells them apart, and the glyph says which shelf the
     *  reader's eye is on. Every identifier below is a compile-checked ECk_Icon. */
    auto
        Get_ProfileGroupIcon(
            ECkOptimizationDebugger_ProfileGroup InGroup)
        -> ECk_Icon
    {
        switch (InGroup)
        {
            case ECkOptimizationDebugger_ProfileGroup::Timing:            return ECk_Icon::ProfileTiming;
            case ECkOptimizationDebugger_ProfileGroup::Gpu:               return ECk_Icon::Hardware;
            case ECkOptimizationDebugger_ProfileGroup::ViewModes:         return ECk_Icon::Display;
            case ECkOptimizationDebugger_ProfileGroup::Nanite:            return ECk_Icon::Rarity;
            // Lumen IS lighting, and the findings page already spends this glyph on the lighting category — one
            // picture, one meaning, across both pages.
            case ECkOptimizationDebugger_ProfileGroup::Lumen:             return ECk_Icon::Lighting;
            case ECkOptimizationDebugger_ProfileGroup::VirtualShadowMaps: return ECk_Icon::Dormant;
            default:                                                     return ECk_Icon::ProfileTiming;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** What the viewport is showing right now, in one line. Read live on every paint — which is the point: a view
     *  mode somebody set from the viewport's own menu, or from the console, moves this too. */
    auto
        Get_ActiveViewportModeText()
        -> FString
    {
        using namespace ck_optimization_debugger_profile_commands;

        if (NOT Get_CanExecute())
        { return FString{TEXT("no editor viewport")}; }

        // The LIVE variant: this runs on the paint path and the state-struct form copies the enabled-stat array to
        // answer a question about view modes that never reads it.
        const auto ActiveId = TryGet_ActiveViewportCommandIdLive();

        if (ActiveId.IsNone())
        {
            // A view mode this page does not offer is a perfectly ordinary state — somebody picked Wireframe from
            // the viewport menu. Saying "something else" beats claiming Lit.
            return FString{TEXT("viewport: a mode this page does not list")};
        }

        const auto* Command = TryGet_Command(ActiveId);

        return Command != nullptr
            ? ck::Format_UE(TEXT("viewport: {}"), Command->DisplayName)
            : FString{TEXT("viewport: unknown")};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CategoryColor(
            ECkOptimizationDebugger_Category InCategory)
        -> FLinearColor
    {
        // Never a hand-written hex — the shared categorical ramp is derived from the same style roles everything
        // else in the suite is, so a palette edit moves these dots with it.
        return ck::debug_axes::Get_CategoricalColor(static_cast<int32>(InCategory));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_RowTextColor(
            bool InIsHighlightMatch)
        -> FSlateColor
    {
        return FSlateColor{InIsHighlightMatch ? CkStyle::Text() : CkStyle::TextMute()};
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** A glyph-led command button whose label can move. The wrapper already carries the explanation, so the glyph
     *  deliberately goes without a Meaning of its own — one surface, one tooltip. */
    auto
        Build_ActionButton(
            ECk_Icon InIconId,
            TAttribute<FText> InLabel,
            TAttribute<FText> InTooltip,
            FOnClicked InOnClicked,
            TAttribute<bool> InIsEnabled)
        -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ToolTipText(InTooltip)
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
                    .Text(InLabel)
                ]
            ];
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The fixed-label form. Every toolbar command whose wording never changes goes through this. */
    auto
        Build_CommandButton(
            ECk_Icon InIconId,
            const FString& InLabel,
            const FString& InTooltip,
            FOnClicked InOnClicked,
            TAttribute<bool> InIsEnabled)
        -> TSharedRef<SWidget>
    {
        return Build_ActionButton(InIconId,
            FText::FromString(InLabel),
            FText::FromString(InTooltip),
            InOnClicked,
            InIsEnabled);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** A dashboard stat tile: the number, its UPPERCASE label, and the change since the previous scan underneath.
     *
     *  The delta line is always PRESENT, even with nothing to compare against — a caption that appears and disappears
     *  makes the tile row jump by a line the first time a second scan lands, and a row that moves under the reader is
     *  a row they re-read. It says "—" instead. */
    auto
        Build_StatTile(
            const FString& InLabel,
            TAttribute<FText> InValue,
            TAttribute<FText> InDeltaText,
            TAttribute<FSlateColor> InDeltaColor,
            const FString& InToolTip)
        -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .ToolTipText(FText::FromString(InToolTip))
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                [
                    SNew(SCkDebug_StatPair)
                    .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                    .Value(InValue)
                    .Label(FText::FromString(InLabel))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(InDeltaText)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(InDeltaColor)
                ]
            ];
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** One threshold the dashboard can edit inline, discovered from the settings class's own reflection rather than
     *  from a hand-written list here.
     *
     *  A second list would be a second place to forget: add a `UPROPERTY` to the settings and this panel grows the
     *  row for free, with the label, explanation and minimum the settings object already declares. Editor
     *  Preferences and this panel therefore cannot disagree about what a threshold is called or what it means. */
    struct FThresholdEntry
    {
        // Captured by the row's delegates and read for the life of the panel. An `FProperty` on a NATIVE class
        // lives as long as its `UClass`, which outlives every widget here — the pointer is only invalidated by a
        // live-coding reload, which tears this window down with it.
        const FIntProperty* Property = nullptr;

        FString Label;
        FString ToolTip;

        double MinValue = 0.0;
    };

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ThresholdEntries()
        -> TArray<FThresholdEntry>
    {
        auto Entries = TArray<FThresholdEntry>{};

        const auto* SettingsClass = UCkOptimizationDebuggerSettings::StaticClass();

        if (SettingsClass == nullptr)
        { return Entries; }

        for (auto It = TFieldIterator<FIntProperty>{SettingsClass}; It; ++It)
        {
            const auto* Property = *It;

            if (Property == nullptr)
            { continue; }

            // Config-backed integers only. A future non-persisted integer on this class is not a threshold, and
            // offering an editor for something that is thrown away on restart would be a lie.
            if (NOT Property->HasAnyPropertyFlags(CPF_Config))
            { continue; }

#if WITH_EDITORONLY_DATA
            // And only the ones the settings class files under "Thresholds". This class also carries config ints
            // that are NOT budgets — the snapshot cap and capture width — and a panel headed "Analysis thresholds"
            // listing them would claim the scan judges something against them.
            //
            // Metadata is stripped from a cooked build, so in packaged Development/DebugGame the panel falls back to
            // listing every config int under its raw property name. That is the same fallback the labels above
            // already take, and the alternative — a hard-coded name list — is a second copy of this class's contents
            // waiting to drift from it.
            static const auto k_CategoryKey = FName{TEXT("Category")};

            if (Property->GetMetaData(k_CategoryKey) != TEXT("Thresholds"))
            { continue; }
#endif

            auto Entry = FThresholdEntry{};
            Entry.Property = Property;
            Entry.Label = Property->GetName();

#if WITH_EDITORONLY_DATA
            // Metadata is stripped from non-editor builds, so the property's own name is the fallback rather than a
            // second hard-coded label table that would go stale.
            static const auto k_DisplayNameKey = FName{TEXT("DisplayName")};
            static const auto k_ToolTipKey     = FName{TEXT("ToolTip")};
            static const auto k_ClampMinKey    = FName{TEXT("ClampMin")};

            if (Property->HasMetaData(k_DisplayNameKey))
            { Entry.Label = Property->GetMetaData(k_DisplayNameKey); }

            Entry.ToolTip = Property->GetMetaData(k_ToolTipKey);

            if (Property->HasMetaData(k_ClampMinKey))
            { Entry.MinValue = FCString::Atod(*Property->GetMetaData(k_ClampMinKey)); }
#endif

            Entries.Add(MoveTemp(Entry));
        }

        // Alphabetical, with the property name as the tie-break. `TFieldIterator`'s order follows the class's
        // property link list, which is an implementation detail nothing should render from — and a settings panel
        // whose rows reordered between two sessions is a panel the reader stops navigating by position.
        Entries.Sort([](const FThresholdEntry& InLhs, const FThresholdEntry& InRhs)
        {
            const auto LabelOrder = InLhs.Label.Compare(InRhs.Label, ESearchCase::IgnoreCase);

            if (LabelOrder != 0)
            { return LabelOrder < 0; }

            return InLhs.Property->GetName().Compare(InRhs.Property->GetName(), ESearchCase::CaseSensitive) < 0;
        });

        return Entries;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The glyph a cleanup category is named by, on its sub-tab and on its rows. One mapping, so a category cannot
     *  mean two pictures. Every identifier is a compile-checked ECk_Icon. */
    auto
        Get_CleanupCategoryIcon(
            ECkOptimizationDebugger_CleanupCategory InCategory)
        -> ECk_Icon
    {
        switch (InCategory)
        {
            // A package nothing opens.
            case ECkOptimizationDebugger_CleanupCategory::Unreferenced:  return ECk_Icon::Payload;
            // Identical crates in two folders.
            case ECkOptimizationDebugger_CleanupCategory::Duplicates:    return ECk_Icon::Duplicates;
            // A name is the key a short-name lookup turns; two assets holding one key is the whole finding.
            case ECkOptimizationDebugger_CleanupCategory::NameCollisions: return ECk_Icon::SaveKey;
            // Fixing one up cuts the hop out of every referencing package.
            case ECkOptimizationDebugger_CleanupCategory::Redirectors:   return ECk_Icon::Redirectors;
            case ECkOptimizationDebugger_CleanupCategory::DirtyPackages: return ECk_Icon::DirtyPackages;
            default:                                                     return ECk_Icon::Cleanup;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** One line of the clipboard payload a right-click copy produces for a cleanup row. Same shape as the finding and
     *  memory summaries: flat, stable, and something the reader searches or mails rather than re-parses. */
    auto
        Build_CleanupRowSummary(
            const FCkOptimizationDebugger_CleanupRow& InRow)
        -> FString
    {
        using namespace ck_optimization_debugger_model;

        // The SAME em-dash rule the rendered size cell applies. Zero bytes on disk and nothing on disk are different
        // statements — a never-written dirty package copying as "0 B" is exactly the claim that rule exists to
        // prevent, and a copied line that disagrees with the row it came from is worse than either.
        const auto SizeText = InRow.DiskSizeBytes > 0
            ? Format_ByteSize(InRow.DiskSizeBytes)
            : FString{TEXT("—")};

        return ck::Format_UE(TEXT("[{}] {} ({}) | {} | {} | {}"),
            Get_CleanupCategoryLabel(InRow.Category),
            InRow.DisplayName,
            InRow.AssetPath,
            InRow.ClassName,
            SizeText,
            InRow.Detail);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The navigation target a cleanup row names — the same asset target a finding and a memory row carry, so the
     *  Content Browser sync is the same one action rather than a third copy of it.
     *
     *  A dirty-package row names a PACKAGE rather than an object, and `Navigate_ToTarget` reports honestly that it
     *  cannot resolve one. That is the right answer: a package with no asset in it is nothing the Content Browser can
     *  be pointed at. */
    auto
        Build_CleanupTarget(
            const FCkOptimizationDebugger_CleanupRow& InRow)
        -> FCkOptimizationDebugger_Target
    {
        auto Target = FCkOptimizationDebugger_Target{};
        Target.Kind = ECkOptimizationDebugger_TargetKind::Asset;
        Target.Path = FSoftObjectPath{InRow.AssetPath};
        Target.DisplayName = InRow.DisplayName;

        return Target;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** One line of the clipboard payload a right-click copy produces for a finding. Deliberately flat and stable —
     *  a pasted finding is something a reader searches, diffs and mails, not something they re-parse. */
    auto
        Build_FindingSummary(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FString
    {
        return ck::Format_UE(TEXT("[{}] {} — {} ({}) | {} | {}"),
            ck_optimization_debugger_model::Get_SeverityLabel(InFinding.Severity),
            InFinding.Title,
            InFinding.Target.DisplayName,
            InFinding.Target.Path.ToString(),
            InFinding.CheckId,
            InFinding.Recommendation);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** One line of the clipboard payload a right-click copy produces for a memory row. Same shape as the finding
     *  summary: flat, stable, and something the reader searches or mails rather than re-parses. */
    auto
        Build_MemoryRowSummary(
            const FCkOptimizationDebugger_MemoryRow& InRow)
        -> FString
    {
        using namespace ck_optimization_debugger_model;

        return ck::Format_UE(TEXT("{} ({}) | {} | {} | {} | GPU {} | {}"),
            InRow.DisplayName,
            InRow.AssetPath,
            Get_MemoryTypeText(InRow),
            Get_MemoryDimensionText(InRow),
            Format_ByteSize(InRow.ResourceSizeBytes),
            InRow.HasSeparableGpuSize ? Format_ByteSize(InRow.GpuSizeBytes) : FString{TEXT("—")},
            Get_MemoryStreamingText(InRow));
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The navigation target a memory row names. A memory row IS an asset target — the same struct a finding
     *  carries — so the Content Browser sync it double-clicks into is the same one action, not a second copy of it. */
    auto
        Build_MemoryTarget(
            const FCkOptimizationDebugger_MemoryRow& InRow)
        -> FCkOptimizationDebugger_Target
    {
        auto Target = FCkOptimizationDebugger_Target{};
        Target.Kind = ECkOptimizationDebugger_TargetKind::Asset;
        Target.Path = FSoftObjectPath{InRow.AssetPath};
        Target.DisplayName = InRow.DisplayName;

        return Target;
    }
}

// --------------------------------------------------------------------------------------------------------------------

/** One line of a memory table.
 *
 *  A multi-column row rather than a hand-built `SHorizontalBox`, because the columns are sortable and resizable and
 *  `SHeaderRow` owns both — a row that laid its own cells out would drift from the header the moment somebody
 *  dragged a divider.
 *
 *  Row-safe throughout: `STextBlock`, `SBox` and `SCkDebug_MeterBar` (a leaf widget with no input handling), and no
 *  brush is allocated here — the meter paints with the shared heat ramp's colour and the row uses the style set's
 *  own `CkDebugger.TableView.Row`. */
class SCkOptimizationDebugger_MemoryTableRow
    : public SMultiColumnTableRow<TSharedPtr<FCkOptimizationDebugger_MemoryRow>>
{
public:
    SLATE_BEGIN_ARGS(SCkOptimizationDebugger_MemoryTableRow)
        : _LargestSizeBytes(0)
    {}
        SLATE_ARGUMENT(TSharedPtr<FCkOptimizationDebugger_MemoryRow>, Row)

        /** An ATTRIBUTE, not an argument. A filter pass reuses the row widgets whose key survived it, so a
         *  denominator captured at construction outlives the filter that changed it — filter out the biggest
         *  texture and every remaining bar would keep being drawn against it and read as near-empty. */
        SLATE_ATTRIBUTE(int64, LargestSizeBytes)
    SLATE_END_ARGS()

    auto
        Construct(
            const FArguments& InArgs,
            const TSharedRef<STableViewBase>& InOwnerTable)
        -> void
    {
        _Row = InArgs._Row;
        _LargestSizeBytes = InArgs._LargestSizeBytes;

        SMultiColumnTableRow<TSharedPtr<FCkOptimizationDebugger_MemoryRow>>::Construct(
            FSuperRowType::FArguments()
                .Style(&ck_optimization_debugger_window::Get_RowStyle())
                .Padding(FMargin{0.0f, 1.0f})
                .ShowSelection(true),
            InOwnerTable);

        // Set AFTER the super's Construct, not through its arguments: `SMultiColumnTableRow::Construct` forwards
        // only style, padding, selection and the drag-drop events to `STableRow` — a tooltip passed in would be
        // silently dropped. The full path is the one thing a truncated name column cannot show.
        SetToolTipText(FText::FromString(_Row.IsValid() ? _Row->AssetPath : FString{}));
    }

    virtual auto
        GenerateWidgetForColumn(
            const FName& InColumnName)
        -> TSharedRef<SWidget> override
    {
        using namespace ck_optimization_debugger_model;
        using namespace ck_optimization_debugger_window;

        if (NOT _Row.IsValid())
        { return SNullWidget::NullWidget; }

        if (InColumnName == Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::Name))
        {
            return Build_TextCell(_Row->DisplayName, CkStyle::Text(), false);
        }

        if (InColumnName == Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::Type))
        {
            return Build_TextCell(Get_MemoryTypeText(*_Row), CkStyle::TextDim(), false);
        }

        if (InColumnName == Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::Dimensions))
        {
            return Build_TextCell(Get_MemoryDimensionText(*_Row), CkStyle::TextDim(), false);
        }

        if (InColumnName == Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::ResourceSize))
        {
            return Build_SizeCell();
        }

        if (InColumnName == Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::GpuSize))
        {
            // An em dash, never a zero: a static mesh does not report a separate video-memory figure, and printing
            // "0 B" there would claim it costs nothing on the GPU.
            return Build_TextCell(_Row->HasSeparableGpuSize
                    ? Format_ByteSize(_Row->GpuSizeBytes)
                    : FString{TEXT("—")},
                _Row->HasSeparableGpuSize ? CkStyle::TextDim() : CkStyle::TextMute(),
                true);
        }

        if (InColumnName == Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::Streaming))
        {
            return Build_TextCell(Get_MemoryStreamingText(*_Row),
                _Row->HasStreamingMetrics ? CkStyle::TextDim() : CkStyle::TextMute(),
                false);
        }

        return SNullWidget::NullWidget;
    }

private:
    auto
        Build_TextCell(
            const FString& InText,
            const FLinearColor& InColor,
            bool InMonospace) const
        -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .VAlign(VAlign_Center)
            .HAlign(InMonospace ? HAlign_Right : HAlign_Left)
            .Padding(FMargin{CkStyle::SpaceS, 0.0f})
            [
                SNew(STextBlock)
                .Text(FText::FromString(InText))
                .Font(InMonospace
                    ? CkStyle::MonoFont(CkStyle::FontSizeSmall())
                    : CkStyle::RegularFont(CkStyle::FontSizeBody()))
                .ColorAndOpacity(FSlateColor{InColor})
            ];
    }

    /** The size cell carries a meter as well as a figure: a column of byte counts tells the reader which row is
     *  biggest only after they have read every row, and the bar answers it at a glance. It is a fraction of the
     *  LARGEST row in this table rather than of the total — a share of the total would leave every bar invisible on
     *  a table with a thousand rows in it.
     *
     *  The fraction is BOUND, not computed here: the denominator moves with the filter and this widget outlives it. */
    auto
        Build_SizeCell() const
        -> TSharedRef<SWidget>
    {
        using namespace ck_optimization_debugger_window;

        const auto Fraction = TAttribute<float>::CreateLambda(
            [WeakRow = TWeakPtr<FCkOptimizationDebugger_MemoryRow>{_Row}, Largest = _LargestSizeBytes]() -> float
        {
            const auto Row = WeakRow.Pin();

            if (NOT Row.IsValid())
            { return 0.0f; }

            const auto LargestBytes = Largest.Get(0);

            if (LargestBytes <= 0)
            { return 0.0f; }

            return FMath::Clamp(static_cast<float>(
                static_cast<double>(Row->ResourceSizeBytes) / static_cast<double>(LargestBytes)), 0.0f, 1.0f);
        });

        return SNew(SBox)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Right)
            .Padding(FMargin{CkStyle::SpaceS, 0.0f})
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(
                        ck_optimization_debugger_model::Format_ByteSize(_Row->ResourceSizeBytes)))
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor{CkStyle::Text()})
                ]

                + SVerticalBox::Slot()
                .AutoHeight() 
                .HAlign(HAlign_Right)
                .Padding(0.0f, 1.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_MeterBar)
                    .Fraction(Fraction)
                    // The shared cost ramp, never a hand-written hex — the heaviest row reads hot for the same
                    // reason an over-budget finding does. Bound off the same attribute so the colour cannot drift
                    // from the length.
                    .FillColor_Lambda([Fraction]() -> FLinearColor
                    {
                        return ck::debug_axes::Get_HeatColor(Fraction.Get(0.0f));
                    })
                    .DesiredSize(FVector2D{k_MemoryRowMeterWidth, k_MemoryRowMeterHeight})
                ]
            ];
    }

private:
    TSharedPtr<FCkOptimizationDebugger_MemoryRow> _Row;

    TAttribute<int64> _LargestSizeBytes;
};

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    // Per-window, created BEFORE the body: the dashboard's threshold editors bind it at construction.
    _ThresholdEditGuard = MakeShared<FCkInspectorEditGuard>();

    // The scope a previous session narrowed to is restored before anything is built, so the level toggles come up
    // showing what the next scan will actually do rather than a default the user has to re-narrow.
    _Model.Set_ExcludedLevelNames(UCkOptimizationDebuggerSettings::Get_ExcludedLevelNameSet());

    // Restored for the same reason: a finding the reader triaged away last week must not reappear on the first scan
    // of this session, or muting would be a per-session gesture nobody would bother making.
    _Model.Set_MutedStableKeys(UCkOptimizationDebuggerSettings::Load_MutedStableKeys());

    // And how they had the list folded. Unlike the two above this narrows nothing — it is purely how the reader
    // arranged twenty-eight checks' worth of groups, which is exactly the kind of arrangement nobody wants to redo
    // every time they open the tool.
    _Model.Set_CollapsedCheckIds(UCkOptimizationDebuggerSettings::Load_CollapsedCheckIds());

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkOptimizationDebugger"))
        .MenuActionsContent()
        [
            DoCreate_MenuActions()
        ]
        .ToolbarContent()
        [
            DoCreate_Toolbar()
        ]
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

    // An actor-targeted finding names an object path INSIDE a world, and entering or leaving PIE swaps the world
    // those paths resolve against. The findings are dropped at that boundary rather than left describing a level
    // that no longer exists — this window still holds no handle and no world pointer of its own.
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkOptimizationDebuggerWindow::DoOnSessionInvalidated);

    DoRebuild_All();
}

// --------------------------------------------------------------------------------------------------------------------

SCkOptimizationDebuggerWindow::~SCkOptimizationDebuggerWindow()
{
    // AddSP self-unbinds when the widget dies; the explicit RemoveAll is determinism, not repair.
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().RemoveAll(this);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // Status untouched: a palette change is not an event with anything to say, and letting the findings line
    // overwrite the strip meant a Style Lab revision after a cleanup or memory scan reset it to "No scan yet."
    DoRebuild_All(/*InRefreshStatus*/ false);
}

// --------------------------------------------------------------------------------------------------------------------
// Chrome construction
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_MenuActions()
    -> TSharedRef<SWidget>
{
    // The three highest-frequency booleans in this tool: "stop showing me the Minor ones" is the first thing a
    // reader reaches for on a real level, and the chrome's icon-action area is where every tool in the suite puts
    // that kind of switch.
    auto Actions = TArray<FCkDebug_IconToggleAction>{};

    for (const auto Severity : ck_optimization_debugger_model::Get_AllSeverities())
    {
        const auto Label = ck_optimization_debugger_model::Get_SeverityLabel(Severity);

        Actions.Add(FCkDebug_IconToggleAction{
            FName{*ck::Format_UE(TEXT("Severity.{}"), Label)},
            ck_optimization_debugger_window::Get_SeverityIcon(Severity),
            FText::FromString(Label),
            // Reads the toggle-scoped count, never `_VisibleSeverityCounts` — the severity this button controls is
            // exactly the axis that must be lifted, or a severity switched off would advertise zero.
            TAttribute<FText>::CreateLambda([this, Severity, Label]() -> FText
            {
                const auto Counts = _ToggleSeverityCounts;

                const auto Count = Severity == ECkOptimizationDebugger_Severity::Critical ? Counts.CriticalCount
                    : Severity == ECkOptimizationDebugger_Severity::Major ? Counts.MajorCount
                    : Counts.MinorCount;

                return FText::FromString(ck::Format_UE(
                    TEXT("Show {} findings in the list — {} match the other filters"),
                    Label.ToLower(), Count));
            }),
            TAttribute<bool>::CreateLambda([this, Severity]() -> bool
            {
                return _Model.Get_SeverityVisible(Severity);
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda([this, Severity](bool InNewState)
            {
                _Model.Set_SeverityVisible(Severity, InNewState);
                DoRebuild_Findings();
            })});
    }

    return SNew(SCkDebug_IconToolbar)
        .Actions(Actions);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_Toolbar()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            Build_CommandButton(ECk_Icon::Target,
                TEXT("Scan"),
                TEXT("Analyze the persistent level and every loaded sub-level"),
                FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnScanClicked),
                TAttribute<bool>::CreateSP(this, &SCkOptimizationDebuggerWindow::Get_CanScan))
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_PageTabs()
    -> TSharedRef<SWidget>
{
    auto Tabs = TArray<FCkDebug_UnderlineTabDesc>{};

    for (const auto Page : ck_optimization_debugger_model::Get_AllPages())
    {
        auto Tab = FCkDebug_UnderlineTabDesc{
            ck_optimization_debugger_model::Get_PageId(Page),
            FText::FromString(ck_optimization_debugger_model::Get_PageLabel(Page))};

        // EVERY count below reads a CACHED field, never a model projection.
        //
        // This tab bar lives outside the page switcher, so it is painted on every page on every frame — and
        // `SCkDebug_UnderlineTabs` evaluates `CountText` twice per tab (once for the badge's visibility, once for
        // its text). A projection here is therefore a full walk of that census, twice, per frame, forever. The
        // fields are refreshed by the same `DoRebuild_*` that refills the list they describe.
        if (Page == ECkOptimizationDebugger_Page::Findings)
        {
            Tab.CountText = TAttribute<FText>::CreateLambda([this]() -> FText
            {
                return _VisibleFindingCount > 0 ? FText::AsNumber(_VisibleFindingCount) : FText::GetEmpty();
            });

            Tab.ShowWarnDot = TAttribute<bool>::CreateLambda([this]() -> bool
            {
                return _VisibleSeverityCounts.CriticalCount > 0;
            });
        }
        else if (Page == ECkOptimizationDebugger_Page::Dashboard)
        {
            // Deliberately the UNFILTERED total, where the Findings tab shows the filtered one. The dashboard is the
            // headline of the whole scan; a filter the reader typed on another page must not make this number drop
            // and read as a level that got better.
            Tab.CountText = TAttribute<FText>::CreateLambda([this]() -> FText
            {
                const auto Count = _TotalSeverityCounts.Get_Total();
                return Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty();
            });

            Tab.ShowWarnDot = TAttribute<bool>::CreateLambda([this]() -> bool
            {
                return _TotalSeverityCounts.CriticalCount > 0;
            });
        }
        else if (Page == ECkOptimizationDebugger_Page::Memory)
        {
            // The ACTIVE table's filtered count, matching the sub-table selector under it. A grand total across
            // three tables would not agree with any number visible on the page.
            //
            // No warn dot, deliberately: this page has no severity concept. A texture being large is a fact, not a
            // finding, and a dot would be the tool asserting a judgement no threshold here backs up.
            Tab.CountText = TAttribute<FText>::CreateLambda([this]() -> FText
            {
                const auto Count = Get_CachedMemoryCount(_ActiveMemoryTable);
                return Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty();
            });
        }
        else if (Page == ECkOptimizationDebugger_Page::Profiling)
        {
            // How much of the viewport this page is currently changing — enabled stat overlays plus a non-Lit view
            // mode. Read off the LIVE viewport, not off a record this window keeps, so a stat somebody toggled from
            // the console moves this number too.
            //
            // No warn dot: leaving Quad Overdraw on is a thing worth noticing, but it is the reader's own doing and
            // not a defect, and this tool's dot means "something is wrong with your level".
            // The LIVE variant, which asks the viewport client directly instead of assembling a state struct — the
            // struct copies the enabled-stat array, and this attribute runs twice a frame on every page.
            Tab.CountText = TAttribute<FText>::CreateLambda([]() -> FText
            {
                using namespace ck_optimization_debugger_profile_commands;

                const auto Count = Get_ActiveCommandCountLive();
                return Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty();
            });
        }
        else if (Page == ECkOptimizationDebugger_Page::Cleanup)
        {
            // How many things are waiting to be REVIEWED, across all four categories and unfiltered. Unlike the
            // memory tab this is not the active sub-tab's count: the reader's question here is "is there anything to
            // look at", and a number that changed when they clicked a category would answer a different one.
            //
            // No warn dot, deliberately. An unreferenced asset is not a defect — deciding whether it should exist is
            // the reader's judgement, and this tool's dot means "something is wrong with your level".
            Tab.CountText = TAttribute<FText>::CreateLambda([this]() -> FText
            {
                const auto Count = _CleanupTotals.RowCount;
                return Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty();
            });
        }
        else if (Page == ECkOptimizationDebugger_Page::Snapshots)
        {
            // How many captures are stored. No warn dot: a snapshot is a picture the reader asked for, and there is
            // nothing about having taken one that is wrong.
            Tab.CountText = TAttribute<FText>::CreateLambda([this]() -> FText
            {
                return _SnapshotCount > 0 ? FText::AsNumber(_SnapshotCount) : FText::GetEmpty();
            });
        }

        Tabs.Add(MoveTemp(Tab));
    }

    return SNew(SCkDebug_UnderlineTabs)
        .Tabs(Tabs)
        .ActiveTabId_Lambda([this]() -> FName
        {
            return ck_optimization_debugger_model::Get_PageId(_Model.Get_ActivePage());
        })
        .OnTabSelected_Lambda([this](FName InPageId)
        {
            const auto Page = ck_optimization_debugger_model::TryGet_PageFromId(InPageId);

            if (NOT Page.IsSet())
            { return; }

            DoSelect_Page(Page.GetValue());
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_Body()
    -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, 0.0f)
        [
            DoCreate_PageTabs()
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            // Slot order IS ECkOptimizationDebugger_Page order — DoSelect_Page indexes with Get_PageIndex.
            SAssignNew(_PageSwitcher, SWidgetSwitcher)

            + SWidgetSwitcher::Slot()
            [
                DoCreate_DashboardPage()
            ]

            + SWidgetSwitcher::Slot()
            [
                DoCreate_FindingsPage()
            ]

            + SWidgetSwitcher::Slot()
            [
                DoCreate_MemoryPage()
            ]

            + SWidgetSwitcher::Slot()
            [
                DoCreate_ProfilingPage()
            ]

            + SWidgetSwitcher::Slot()
            [
                DoCreate_CleanupPage()
            ]

            + SWidgetSwitcher::Slot()
            [
                DoCreate_SnapshotsPage()
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_Status()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

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
            .Size(FVector2D{k_StatusDotSize, k_StatusDotSize})
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SAssignNew(_StatusText, STextBlock)
            .Text(FText::FromString(TEXT("No scan yet.")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
        ];
}

// --------------------------------------------------------------------------------------------------------------------
// Pages
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_DashboardPage()
    -> TSharedRef<SWidget>
{
    return SNew(SScrollBox)

        + SScrollBox::Slot()
        .Padding(CkStyle::SpaceM)
        [
            SAssignNew(_DashboardBox, SVerticalBox)
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_CategoryFilters()
    -> TSharedRef<SWidget>
{
    auto Actions = TArray<FCkDebug_IconToggleAction>{};

    for (const auto Category : ck_optimization_debugger_model::Get_AllCategories())
    {
        const auto Label = ck_optimization_debugger_model::Get_CategoryLabel(Category);

        Actions.Add(FCkDebug_IconToggleAction{
            FName{*ck::Format_UE(TEXT("Category.{}"), Label)},
            ck_optimization_debugger_window::Get_CategoryIcon(Category),
            FText::FromString(Label),
            // The count IGNORES the category mask while honouring every other axis, which is what makes it the
            // count this button would GIVE you rather than the one it is currently showing. A category toggled off
            // reporting zero would make the control that turns it back on the one claiming there is nothing there.
            TAttribute<FText>::CreateLambda([this, Category, Label]() -> FText
            {
                return FText::FromString(ck::Format_UE(
                    TEXT("Show {} findings in the list — {} match the other filters"),
                    Label.ToLower(), Get_CachedCategoryCount(Category)));
            }),
            TAttribute<bool>::CreateLambda([this, Category]() -> bool
            {
                return _Model.Get_CategoryVisible(Category);
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda([this, Category](bool InNewState)
            {
                _Model.Set_CategoryVisible(Category, InNewState);
                DoRebuild_Findings();
            })});
    }

    return SNew(SCkDebug_IconToolbar)
        .Actions(Actions);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_ScopeFilters()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_model;

    // The three axes a reader actually narrows by, kept OFF the free-text box on purpose. Typing `/Game/Characters`
    // into Filter also matched any finding whose wording contained the word, which is one control silently answering
    // two questions.
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(SCkDebug_SearchBar)
            .HintText(FText::FromString(TEXT("Scope to path, e.g. /Game/Characters")))
            .OnSearchTextChanged_Lambda([this](const FString& InText)
            {
                if (_Model.Get_PathScope() == InText)
                { return; }

                _Model.Set_PathScope(InText);
                DoRebuild_Findings();
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_IconToggle)
            .IconId(ECk_Icon::Fix)
            .Label(FText::FromString(TEXT("Has a suggested fix")))
            .ToolTip(FText::FromString(
                TEXT("Show only findings whose check offered a fix. Whether the button can RUN one also needs an ")
                TEXT("editor session, no play session, and a registered fix — the disabled tooltip says which.")))
            .IsOn_Lambda([this]() -> bool
            {
                return _Model.Get_ShowOnlyWithSuggestedFix();
            })
            .OnStateChanged_Lambda([this](bool InNewState)
            {
                _Model.Set_ShowOnlyWithSuggestedFix(InNewState);
                DoRebuild_Findings();
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_IconToggle)
            // `Ghost` for things that are present but not shown. There is no eye glyph in the set, and inventing one
            // for a single toggle is not worth a new icon.
            .IconId(ECk_Icon::Orphaned)
            .Label(FText::FromString(TEXT("Show muted")))
            .ToolTip(FText::FromString(
                TEXT("Show findings you muted, marked as muted. Muting hides a finding from the list until you ")
                TEXT("unmute it — it never deletes, fixes, or excludes anything from the scan.")))
            .IsOn_Lambda([this]() -> bool
            {
                return _Model.Get_ShowMuted();
            })
            .IsEnabled_Lambda([this]() -> bool
            {
                // Nothing muted means nothing to reveal. Disabled rather than hidden: a control that vanished would
                // teach nobody the feature exists, and its tooltip still explains what it is for.
                return _Model.Get_MutedFindingCount() > 0;
            })
            .OnStateChanged_Lambda([this](bool InNewState)
            {
                _Model.Set_ShowMuted(InNewState);
                DoRebuild_Findings();
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCkDebug_IconToggle)
            .IconId(ECk_Icon::HotPath)
            .Label(FText::FromString(TEXT("≥2× budget")))
            .ToolTip(FText::FromString(
                TEXT("Show only findings at least twice past their own budget — the ratio the severity grading ")
                TEXT("already computes. Checks that fire on a condition rather than a measurement have no budget ")
                TEXT("to be over, so this excludes them, exactly as a path scope excludes findings with no path.")))
            .IsOn_Lambda([this]() -> bool
            {
                return _Model.Get_MinBudgetRatio() > 0.0f;
            })
            .IsEnabled_Lambda([this]() -> bool
            {
                // Nothing measured against a budget means nothing this can narrow to. Disabled rather than hidden,
                // for the same reason the muted toggle is: a control that vanished would teach nobody it exists.
                return _Model.Get_FindingsWithBudgetCount() > 0;
            })
            .OnStateChanged_Lambda([this](bool InNewState)
            {
                // One fixed step rather than a spinner. The grading rule's own escalation point is 2×, so this is
                // the threshold the severities were already derived from — a free-typed number would invite the
                // reader to pick one the list's own colouring does not agree with.
                _Model.Set_MinBudgetRatio(InNewState
                    ? ck_optimization_debugger_window::k_OverBudgetRatioStep
                    : 0.0f);
                DoRebuild_Findings();
            })
        ]

        // Deliberately in THIS row rather than the toolbar: the controls to its left are exactly what decide which
        // findings it will touch, so sitting under them states the scope without a sentence.
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [
            SNew(SButton)
            .Text_Lambda([this]() -> FText
            {
                return FText::FromString(ck::Format_UE(TEXT("Fix All ({})"), _VisibleFixableCount));
            })
            .ToolTipText_Lambda([this]() -> FText
            {
                const auto Unavailable = ck_optimization_debugger_fixes::Get_FixesUnavailableReason();

                if (NOT Unavailable.IsEmpty())
                { return FText::FromString(Unavailable); }

                if (_VisibleFixableCount == 0)
                {
                    return FText::FromString(
                        TEXT("Nothing currently visible has an automatic fix. This acts on the list as filtered — ")
                        TEXT("narrow it first if you only want part of it."));
                }

                return FText::FromString(ck::Format_UE(
                    TEXT("Apply every automatic fix in the {} finding(s) the current filters admit, inside one ")
                    TEXT("transaction Undo can reverse. Muted and filtered-out findings are NOT touched. Anything ")
                    TEXT("destructive or behaviour-changing asks first."),
                    _VisibleFixableCount));
            })
            .IsEnabled_Lambda([this]() -> bool
            {
                return _VisibleFixableCount > 0 && ck_optimization_debugger_fixes::Get_CanApplyFixes();
            })
            .OnClicked_Lambda([this]() -> FReply
            {
                DoApply_FixAllVisible();
                return FReply::Handled();
            })
        ]

        // The count sits BESIDE the toggle rather than inside its tooltip, and that is the load-bearing half of this
        // control. A filter that can silently suppress findings makes the whole tool unreliable, so the reader has to
        // be able to see that something is hidden WITHOUT hovering to discover it. (`SCkDebug_IconToggle`'s label and
        // tooltip are both static `FText` arguments, so a live number could not have gone in either.)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(CkStyle::SpaceXS, 0.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() -> FText
            {
                return FText::FromString(ck::Format_UE(TEXT("{} muted"), _Model.Get_MutedFindingCount()));
            })
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .Visibility_Lambda([this]() -> EVisibility
            {
                return _Model.Get_MutedFindingCount() > 0
                    ? EVisibility::HitTestInvisible
                    : EVisibility::Collapsed;
            })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_FindingsPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

    // Built ONCE. Every later change — scan, filter, highlight, selection — mutates the list's item source or the
    // detail box, never this tree. Re-creating a page on a data change is the one-frame-scrunch defect.
    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            SNew(SCkDebug_DualSearchBar)
            .FilterHintText(FText::FromString(TEXT("Filter findings...")))
            .HighlightHintText(FText::FromString(TEXT("Highlight...")))
            .OnFilterTextChanged_Lambda([this](const FString& InText)
            {
                if (_Model.Get_Filter().FilterString == InText)
                { return; }

                _Model.Get_Filter().FilterString = InText;
                DoRebuild_Findings();
            })
            .OnHighlightTextChanged_Lambda([this](const FString& InText)
            {
                if (_Model.Get_Filter().HighlightString == InText)
                { return; }

                _Model.Get_Filter().HighlightString = InText;
                DoRebuild_Findings();
            })
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            DoCreate_ScopeFilters()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, CkStyle::SpaceXS)
        [
            DoCreate_CategoryFilters()
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)

            + SSplitter::Slot()
            .Value(0.62f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SCkDebug_SectionHeader)
                    .Label(FText::FromString(TEXT("Findings")))
                    .ToolTip(FText::FromString(TEXT("Grouped by the check that produced them, worst first")))
                    .Underline(true)
                    .RightContent()
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
                        [
                            SNew(SCkDebug_Chip)
                            .Text(FText::FromString(TEXT("Fold all")))
                            .Kind(ECkDebug_ChipKind::Neutral)
                            .ShowDot(false)
                            .OnClicked(FOnCkDebug_ChipClicked::CreateLambda([this]() -> void
                            {
                                DoSet_AllChecksCollapsed(true);
                            }))
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                        [
                            SNew(SCkDebug_Chip)
                            .Text(FText::FromString(TEXT("Unfold all")))
                            .Kind(ECkDebug_ChipKind::Neutral)
                            .ShowDot(false)
                            .OnClicked(FOnCkDebug_ChipClicked::CreateLambda([this]() -> void
                            {
                                DoSet_AllChecksCollapsed(false);
                            }))
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SCkDebug_CountBadge)
                            .ValueText_Lambda([this]() -> FText
                            {
                                // The cached count, not the projection — this badge paints whenever the Findings page
                                // is up, and re-deriving it here would walk every finding once a frame.
                                return FText::FromString(ck::Format_UE(TEXT("{}"), _VisibleFindingCount));
                            })
                            .ValueColor(CkStyle::Text())
                            .BackgroundColor(CkStyle::Bg2())
                            .BorderColor(CkStyle::Border())
                        ]
                    ]
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SAssignNew(_FindingList, SListView<FFindingItem>)
                    .ListItemsSource(&_FindingItems)
                    .OnGenerateRow(this, &SCkOptimizationDebuggerWindow::DoGenerate_FindingRow)
                    .OnSelectionChanged(this, &SCkOptimizationDebuggerWindow::DoOnFindingSelectionChanged)
                    .OnContextMenuOpening(this, &SCkOptimizationDebuggerWindow::DoOnFindingContextMenu)
                    .OnMouseButtonDoubleClick(this, &SCkOptimizationDebuggerWindow::DoOnFindingDoubleClicked)
                    .SelectionMode(ESelectionMode::Multi)
                ]
            ]

            + SSplitter::Slot()
            .Value(0.38f)
            [
                // Detail above, fix queue below. A splitter rather than a fixed split so a reader assembling a
                // large batch can give the tray the room, and one reading a long explanation can take it back.
                SNew(SSplitter)
                .Orientation(Orient_Vertical)

                + SSplitter::Slot()
                .Value(0.55f)
                [
                    SNew(SScrollBox)

                    + SScrollBox::Slot()
                    .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                    [
                        SAssignNew(_FindingDetailBox, SVerticalBox)
                    ]
                ]

                + SSplitter::Slot()
                .Value(0.45f)
                [
                    DoCreate_FixQueuePanel()
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_FixQueuePanel()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

    // Built ONCE. Everything inside that can move is either an attribute over a cached field or lives in
    // `_FixQueueBox`, which `DoRebuild_FixQueue` refills in place.
    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Fix queue")))
            .ToolTip(FText::FromString(TEXT("Findings you staged. Survives filtering, folding and re-scans; cleared when a play session invalidates the scan.")))
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
                        return FText::FromString(ck::Format_UE(TEXT("{}"), _QueuedCount));
                    })
                    .ValueColor(CkStyle::Text())
                    .BackgroundColor(CkStyle::Bg2())
                    .BorderColor(CkStyle::Border())
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_Chip)
                    .Text(FText::FromString(TEXT("Clear")))
                    .Kind(ECkDebug_ChipKind::Neutral)
                    .ShowDot(false)
                    .Visibility_Lambda([this]() -> EVisibility
                    {
                        return _QueuedCount > 0 ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    .OnClicked(FOnCkDebug_ChipClicked::CreateLambda([this]() -> void
                    {
                        DoClear_Queue();
                    }))
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SScrollBox)

            + SScrollBox::Slot()
            [
                SAssignNew(_FixQueueBox, SVerticalBox)
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                .Text_Lambda([this]() -> FText
                {
                    if (_QueuedCount == 0)
                    { return FText::GetEmpty(); }

                    // Both numbers, always. "9 staged" alone would let a reader believe nine fixes are about to run
                    // when only four of them have one, and the difference is the whole reason the button is a
                    // different number from the badge above.
                    return FText::FromString(ck::Format_UE(TEXT("{} staged · {} fixable"),
                        _QueuedCount, _QueuedFixableCount));
                })
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .IsEnabled_Lambda([this]() -> bool { return _QueueApplyEnabled; })
                .ToolTipText_Lambda([this]() -> FText { return FText::FromString(_QueueApplyTooltip); })
                .OnClicked_Lambda([this]() -> FReply
                {
                    DoApply_Queue();
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() -> FText { return FText::FromString(_QueueApplyLabel); })
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_MemoryPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

    // Built ONCE, like the findings page. A scan, a filter, a sort or a table switch mutates the list's item source
    // and the cached totals the header's attributes read — never this tree.
    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            DoCreate_MemoryHeader()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            DoCreate_MemoryTableTabs()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            // A single debounced query — the memory tables are searched, never highlighted. The dual bar's second
            // box would offer a dimming pass over a table whose rows are already one line each.
            SNew(SCkDebug_SearchBar)
            .HintText(FText::FromString(TEXT("Filter by name, path or format...")))
            .OnSearchTextChanged_Lambda([this](const FString& InText)
            {
                if (_Model.Get_MemoryFilterString() == InText)
                { return; }

                _Model.Set_MemoryFilterString(InText);
                DoRebuild_Memory();
            })
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceXS, CkStyle::SpaceM, 0.0f)
        [
            // The streaming note. Present only when there is a reason to print one, and only on the table it is
            // about — a footnote about texture streaming above a mesh table would be noise.
            SNew(STextBlock)
            .AutoWrapText(true)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .Text_Lambda([this]() -> FText
            {
                return FText::FromString(ck_optimization_debugger_model::Get_StreamingAvailabilityNote(
                    _Model.Get_StreamingAvailability()));
            })
            .Visibility_Lambda([this]() -> EVisibility
            {
                if (NOT _Model.Get_HasMemoryScan())
                { return EVisibility::Collapsed; }

                if (_ActiveMemoryTable == ECkOptimizationDebugger_MemoryTable::StaticMeshes)
                { return EVisibility::Collapsed; }

                return _Model.Get_StreamingAvailability() ==
                    ECkOptimizationDebugger_StreamingAvailability::Available
                        ? EVisibility::Collapsed
                        : EVisibility::Visible;
            })
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, 0.0f)
        [
            // Pre-allocated and toggled by visibility rather than slotted in on demand: swapping a page's children
            // on a data change is the one-frame-scrunch defect.
            SNew(SCkDebug_Card)
            .Visibility_Lambda([this]() -> EVisibility
            {
                return _Model.Get_HasMemoryScan() ? EVisibility::Collapsed : EVisibility::Visible;
            })
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(Get_IconBrush(ECk_Icon::Hardware))
                    .Meaning(FText::FromString(TEXT("Nothing has been measured yet")))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    .Size(FVector2D{k_EmptyStateIconSize, k_EmptyStateIconSize})
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Nothing measured yet. Refresh to list every texture, render ")
                            TEXT("target and static mesh that is resident right now — including the ones no level ")
                            TEXT("places, which is exactly what a level scan cannot tell you.")))
                    .AutoWrapText(true)
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SAssignNew(_MemoryList, SListView<FMemoryItem>)
            .ListItemsSource(&_MemoryItems)
            .OnGenerateRow(this, &SCkOptimizationDebuggerWindow::DoGenerate_MemoryRow)
            .OnContextMenuOpening(this, &SCkOptimizationDebuggerWindow::DoOnMemoryContextMenu)
            .OnMouseButtonDoubleClick(this, &SCkOptimizationDebuggerWindow::DoOnMemoryDoubleClicked)
            .SelectionMode(ESelectionMode::Multi)
            .HeaderRow(DoCreate_MemoryHeaderRow())
            .Visibility_Lambda([this]() -> EVisibility
            {
                return _Model.Get_HasMemoryScan() ? EVisibility::Visible : EVisibility::Collapsed;
            })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_MemoryHeader()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_model;

    // Every tile reads the CACHED totals struct, refreshed by DoRebuild_Memory. An attribute that re-summed the rows
    // would do it on the paint path, every frame, over every resident object in the session.
    const auto Tile = [](const FString& InLabel, TAttribute<FText> InValue, const FString& InToolTip)
        -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .ToolTipText(FText::FromString(InToolTip))
            .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceXL, 0.0f})
            [
                SNew(SCkDebug_StatPair)
                .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                .Value(InValue)
                .Label(FText::FromString(InLabel))
            ];
    };

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Resident memory")))
            .SubText(FText::FromString(TEXT("what is loaded right now, not what this level places")))
            .Underline(true)
            .RightContent()
            [
                Build_CommandButton(ECk_Icon::Probe,
                    TEXT("Refresh"),
                    TEXT("Measure every resident texture, render target and static mesh. Loads nothing and builds ")
                    TEXT("nothing — it reads what is already in memory"),
                    FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnMemoryRefreshClicked),
                    TAttribute<bool>{true})
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                Tile(TEXT("Assets"),
                    TAttribute<FText>::CreateLambda([this]() -> FText
                    {
                        return FText::FromString(Format_AbbreviatedCount(_MemoryTotals.RowCount));
                    }),
                    TEXT("Resident textures, render targets and static meshes, across all three tables"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                Tile(TEXT("Resource"),
                    TAttribute<FText>::CreateLambda([this]() -> FText
                    {
                        return FText::FromString(Format_ByteSize(_MemoryTotals.ResourceSizeBytes));
                    }),
                    TEXT("Summed exclusive resource size — what these objects are costing right now, not what they ")
                    TEXT("would cost fully loaded"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                Tile(TEXT("GPU"),
                    TAttribute<FText>::CreateLambda([this]() -> FText
                    {
                        // An em dash while nothing reports a separable figure: a "0 B" GPU total on a session full
                        // of textures would read as a measurement rather than as its absence.
                        return FText::FromString(_MemoryTotals.SeparableGpuRowCount > 0
                            ? Format_ByteSize(_MemoryTotals.GpuSizeBytes)
                            : FString{TEXT("—")});
                    }),
                    TEXT("Summed video memory, over the rows that report one separately. Textures do; static meshes ")
                    TEXT("and render targets fold their whole cost into one untagged figure"))
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_MemoryTableTabs()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_model;

    auto Tabs = TArray<FCkDebug_UnderlineTabDesc>{};

    for (const auto Table : Get_AllMemoryTables())
    {
        auto Tab = FCkDebug_UnderlineTabDesc{Get_MemoryTableId(Table),
            FText::FromString(Get_MemoryTableLabel(Table))};

        // The FILTERED count, matching what the page tab does for findings — a selector count that ignored the
        // search box would send the reader to a table the search has emptied. Cached, for the reason the page tabs'
        // counts are.
        Tab.CountText = TAttribute<FText>::CreateLambda([this, Table]() -> FText
        {
            const auto Count = Get_CachedMemoryCount(Table);
            return Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty();
        });

        Tabs.Add(MoveTemp(Tab));
    }

    // The same underline bar the page switcher uses, one level in. `SSegmentedControl` was the alternative and
    // carries no per-segment count — and a selector that could not say how many render targets there are would make
    // the reader click each one to find out.
    return SNew(SCkDebug_UnderlineTabs)
        .Tabs(Tabs)
        .FontSize(CkStyle::FontSizeSmall())
        .ActiveTabId_Lambda([this]() -> FName
        {
            return Get_MemoryTableId(_ActiveMemoryTable);
        })
        .OnTabSelected_Lambda([this](FName InTableId)
        {
            const auto Table = TryGet_MemoryTableFromId(InTableId);

            if (NOT Table.IsSet())
            { return; }

            DoSelect_MemoryTable(Table.GetValue());
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_MemoryHeaderRow()
    -> TSharedRef<SHeaderRow>
{
    using namespace ck_optimization_debugger_model;
    using namespace ck_optimization_debugger_window;

    const auto SortMode = [this](ECkOptimizationDebugger_MemoryColumn InColumn) -> TAttribute<EColumnSortMode::Type>
    {
        return TAttribute<EColumnSortMode::Type>::CreateLambda([this, InColumn]() -> EColumnSortMode::Type
        {
            return Get_MemorySortMode(InColumn);
        });
    };

    const auto OnSort = FOnSortModeChanged::CreateSP(
        this, &SCkOptimizationDebuggerWindow::DoOnMemorySortChanged);

    return SNew(SHeaderRow)

        + SHeaderRow::Column(Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::Name))
          .DefaultLabel(FText::FromString(Get_MemoryColumnLabel(ECkOptimizationDebugger_MemoryColumn::Name)))
          .FillWidth(1.0f)
          .SortMode(SortMode(ECkOptimizationDebugger_MemoryColumn::Name))
          .OnSort(OnSort)

        + SHeaderRow::Column(Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::Type))
          .DefaultLabel(FText::FromString(Get_MemoryColumnLabel(ECkOptimizationDebugger_MemoryColumn::Type)))
          .FixedWidth(k_MemoryTypeColumnWidth)
          .SortMode(SortMode(ECkOptimizationDebugger_MemoryColumn::Type))
          .OnSort(OnSort)

        + SHeaderRow::Column(Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::Dimensions))
          .DefaultLabel(FText::FromString(Get_MemoryColumnLabel(ECkOptimizationDebugger_MemoryColumn::Dimensions)))
          .FixedWidth(k_MemoryDimensionColumnWidth)
          .SortMode(SortMode(ECkOptimizationDebugger_MemoryColumn::Dimensions))
          .OnSort(OnSort)

        + SHeaderRow::Column(Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::ResourceSize))
          .DefaultLabel(FText::FromString(Get_MemoryColumnLabel(ECkOptimizationDebugger_MemoryColumn::ResourceSize)))
          .FixedWidth(k_MemorySizeColumnWidth)
          .HAlignHeader(HAlign_Right)
          .SortMode(SortMode(ECkOptimizationDebugger_MemoryColumn::ResourceSize))
          .OnSort(OnSort)

        + SHeaderRow::Column(Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::GpuSize))
          .DefaultLabel(FText::FromString(Get_MemoryColumnLabel(ECkOptimizationDebugger_MemoryColumn::GpuSize)))
          .FixedWidth(k_MemoryGpuColumnWidth)
          .HAlignHeader(HAlign_Right)
          .SortMode(SortMode(ECkOptimizationDebugger_MemoryColumn::GpuSize))
          .OnSort(OnSort)

        + SHeaderRow::Column(Get_MemoryColumnId(ECkOptimizationDebugger_MemoryColumn::Streaming))
          .DefaultLabel(FText::FromString(Get_MemoryColumnLabel(ECkOptimizationDebugger_MemoryColumn::Streaming)))
          .FixedWidth(k_MemoryStreamingColumnWidth)
          .SortMode(SortMode(ECkOptimizationDebugger_MemoryColumn::Streaming))
          .OnSort(OnSort);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_ProfilingPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_profile_commands;

    // Built ONCE, like every other page here. Nothing on it is re-created afterwards: each entry's on/off is an
    // attribute reading the live viewport, so a stat somebody toggled from the console moves these controls without
    // this window being told. The one exception is the recent-command rail, which DoRebuild_Profiling refills.
    auto Sections = SNew(SVerticalBox);

    for (const auto Group : Get_AllGroups())
    {
        Sections->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
        [
            DoBuild_ProfileGroupSection(Group)
        ];
    }

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Profiling")))
            .SubText(FText::FromString(TEXT("applies to the level editor viewport you last clicked in")))
            .Underline(true)
            .RightContent()
            [
                SNew(SCkDebug_StatusPill)
                .ShowDot(true)
                .Text_Lambda([]() -> FText
                {
                    return FText::FromString(Get_ActiveViewportModeText());
                })
                .Tone_Lambda([]() -> ECk_Tone
                {
                    if (NOT Get_CanExecute())
                    { return ECk_Tone::Neutral; }

                    // Info while a visualizer is up, neutral on Lit. A viewport left in Quad Overdraw is a viewport
                    // whose framerate means nothing, and the pill is what stops that being a surprise.
                    const auto ActiveId = TryGet_ActiveViewportCommandIdLive();

                    return ActiveId.IsNone() || ActiveId == FName{TEXT("ViewMode.Lit")}
                        ? ECk_Tone::Neutral
                        : ECk_Tone::Info;
                })
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            // Present only when there is nothing to drive. Outside an editor session every control below is
            // disabled, and a row of dead buttons with no explanation is the state this line exists to prevent.
            SNew(STextBlock)
            .AutoWrapText(true)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .Text(FText::FromString(TEXT("No level editor viewport. These actions drive one directly — a stat ")
                    TEXT("overlay and a view mode both belong to a viewport, not to the engine — so there is ")
                    TEXT("nothing here to press.")))
            .Visibility_Lambda([]() -> EVisibility
            {
                return Get_CanExecute() ? EVisibility::Collapsed : EVisibility::Visible;
            })
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SScrollBox)

            + SScrollBox::Slot()
            .Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, 0.0f)
            [
                Sections
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, CkStyle::SpaceM)
        [
            DoBuild_ProfileCustomCommandPanel()
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoBuild_ProfileGroupSection(
        ECkOptimizationDebugger_ProfileGroup InGroup)
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_profile_commands;

    const auto Commands = Get_CommandsInGroup(InGroup);
    const auto IconId = Get_ProfileGroupIcon(InGroup);

    // Wraps rather than clips: the page lives in a dockable tab a reader will narrow, and a shelf that hid its last
    // two entries at 400px would be a shelf whose contents depend on the window size.
    auto Entries = SNew(SWrapBox)
        .UseAllottedSize(true)
        .InnerSlotPadding(FVector2D{CkStyle::SpaceS, CkStyle::SpaceS});

    for (const auto& Command : Commands)
    {
        const auto ToolTip = ck::Format_UE(TEXT("{}\n\n{}"), Command.Description, Get_CommandSummary(Command));
        const auto CommandId = Command.Id;

        if (Command.Kind == ECkOptimizationDebugger_ProfileCommandKind::OneShot)
        {
            Entries->AddSlot()
            [
                Build_CommandButton(IconId,
                    Command.DisplayName,
                    ToolTip,
                    FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnProfileCommandClicked, CommandId),
                    TAttribute<bool>::CreateLambda([]() -> bool { return Get_CanExecute(); }))
            ];

            continue;
        }

        // Toggle and ViewMode share a widget on purpose: both answer "is this what I am looking at". The difference
        // is exclusivity, and that is the viewport's own property rather than something this bar enforces — setting
        // a view mode turns the previous one off because a viewport has exactly one.
        Entries->AddSlot()
        [
            SNew(SCkDebug_IconToggle)
            .IconId(IconId)
            .Label(FText::FromString(Command.DisplayName))
            .ToolTip(FText::FromString(ToolTip))
            .ShowLabel(true)
            .IsEnabled_Lambda([]() -> bool { return Get_CanExecute(); })
            .IsOn_Lambda([CommandId]() -> bool
            {
                const auto* Found = TryGet_Command(CommandId);

                return Found != nullptr && Get_IsCommandActive(*Found);
            })
            .OnStateChanged_Lambda([this, CommandId](bool InNewState)
            {
                (void)InNewState;

                // The requested state is deliberately ignored. A stat command TOGGLES and a view mode SETS; asking
                // the engine for "off" is not something either lever offers, so the honest action for a press is
                // the same action in both directions, and the next paint reads back what actually happened.
                const auto* Found = TryGet_Command(CommandId);

                if (Found == nullptr)
                { return; }

                DoExecute_ProfileCommand(*Found);
            })
        ];
    }

    const auto IsVisualizerGroup = InGroup == ECkOptimizationDebugger_ProfileGroup::Nanite ||
                                   InGroup == ECkOptimizationDebugger_ProfileGroup::Lumen ||
                                   InGroup == ECkOptimizationDebugger_ProfileGroup::VirtualShadowMaps;

    // The way out of a visualizer is the single `ViewMode.Lit` entry, which lives on another shelf and may well be
    // scrolled off. Rather than duplicate it into three more catalog entries that would all do the same thing, each
    // visualizer shelf reuses that ONE entry from its own header. Non-visualizer shelves get the null widget, which
    // is what the named slot defaults to anyway.
    auto ResetContent = IsVisualizerGroup
        ? Build_CommandButton(ECk_Icon::DayTime,
            TEXT("Lit"),
            TEXT("Put the viewport back to the normal Lit view"),
            FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnProfileCommandClicked,
                FName{TEXT("ViewMode.Lit")}),
            TAttribute<bool>::CreateLambda([]() -> bool { return Get_CanExecute(); }))
        : SNullWidget::NullWidget;

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(Get_GroupLabel(InGroup)))
            .SubText(FText::FromString(Get_GroupSubText(InGroup)))
            .Underline(true)
            .RightContent()
            [
                ResetContent
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            Entries
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoBuild_ProfileCustomCommandPanel()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_profile_commands;

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Custom command")))
            .SubText(FText::FromString(TEXT("anything not on the shelves — exec'd against the editor world")))
            .Underline(true)
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                // A plain editable box, not `SCkDebug_SearchBar`: that widget debounces and reports every
                // keystroke, which is the right shape for narrowing a list and exactly the wrong one for a command
                // — half a console command must never run. This one fires on Enter and on the Run button only.
                SAssignNew(_CustomCommandBox, SEditableTextBox)
                .HintText(FText::FromString(TEXT("e.g. r.ScreenPercentage 50")))
                .IsEnabled_Lambda([]() -> bool { return Get_CanExecute(); })
                .OnTextCommitted(this, &SCkOptimizationDebuggerWindow::DoOnCustomCommandCommitted)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                Build_CommandButton(ECk_Icon::Power,
                    TEXT("Run"),
                    TEXT("Exec this command against the editor world. Whatever the console accepts, this accepts — ")
                    TEXT("including the ones that cost a frame"),
                    FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnRunCustomCommandClicked),
                    TAttribute<bool>::CreateLambda([]() -> bool { return Get_CanExecute(); }))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SAssignNew(_RecentCommandsBox, SVerticalBox)
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_CleanupPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_model;

    // Built ONCE, like the findings and memory pages. A scan, a filter or a category switch mutates the list's item
    // source and the cached totals the header's attributes read — never this tree.
    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            DoCreate_CleanupHeader()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            DoCreate_CleanupCategoryTabs()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceXS, CkStyle::SpaceM, 0.0f)
        [
            // What the active category IS, said where the reader is standing. For duplicates this is the sentence
            // that says "possible", and it is an attribute so switching categories rewrites it without a rebuild.
            SNew(STextBlock)
            .AutoWrapText(true)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .Text_Lambda([this]() -> FText
            {
                return FText::FromString(Get_CleanupCategoryHint(_ActiveCleanupCategory));
            })
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                // Its own query — a filter typed on the findings or memory page must not narrow this list. The
                // matching semantics are shared; the state is not.
                SNew(SCkDebug_SearchBar)
                .HintText(FText::FromString(TEXT("Filter by name, path, class or reason...")))
                .OnSearchTextChanged_Lambda([this](const FString& InText)
                {
                    if (_Model.Get_CleanupFilterString() == InText)
                    { return; }

                    _Model.Set_CleanupFilterString(InText);
                    DoRebuild_Cleanup();
                })
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                // ONE action button, whose verb, count and enabled state all follow the active category. Three
                // buttons of which two are always disabled would teach the reader to stop reading them.
                Build_ActionButton(ECk_Icon::Cleanup,
                    TAttribute<FText>::CreateLambda([this]() -> FText
                    {
                        return FText::FromString(_CleanupActionLabel);
                    }),
                    TAttribute<FText>::CreateLambda([this]() -> FText
                    {
                        return FText::FromString(_CleanupActionTooltip);
                    }),
                    FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnCleanupActionClicked),
                    TAttribute<bool>::CreateLambda([this]() -> bool { return _CleanupActionEnabled; }))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, 0.0f)
        [
            // Pre-allocated and toggled by visibility rather than slotted in on demand: swapping a page's children on
            // a data change is the one-frame-scrunch defect.
            SNew(SCkDebug_Card)
            .Visibility_Lambda([this]() -> EVisibility
            {
                return _Model.Get_HasCleanupScan() ? EVisibility::Collapsed : EVisibility::Visible;
            })
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .HAlign(HAlign_Left)
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                    [
                        SNew(SCkDebug_Icon)
                        .Brush(Get_IconBrush(ECk_Icon::Cleanup))
                        .Meaning(FText::FromString(TEXT("Nothing has been reviewed yet")))
                        .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                        .Size(FVector2D{k_EmptyStateIconSize, k_EmptyStateIconSize})
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Nothing reviewed yet. Scan /Game for assets nothing ")
                                TEXT("references, possible duplicates, redirectors and unsaved packages. Everything ")
                                TEXT("found is presented for review — nothing is deleted by a scan, and deletion ")
                                TEXT("always goes through the editor's own confirmation dialog.")))
                        .AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
                [
                    // The SAME command the header's button runs — one cleanup scan path, two places to reach it.
                    Build_CommandButton(ECk_Icon::Cleanup,
                        TEXT("Scan Project"),
                        TEXT("Review /Game for unreferenced assets, possible duplicates, redirectors and dirty ")
                        TEXT("packages"),
                        FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnCleanupScanClicked),
                        TAttribute<bool>::CreateSP(this, &SCkOptimizationDebuggerWindow::Get_CanScan))
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SAssignNew(_CleanupList, SListView<FCleanupItem>)
            .ListItemsSource(&_CleanupItems)
            .OnGenerateRow(this, &SCkOptimizationDebuggerWindow::DoGenerate_CleanupRow)
            .OnSelectionChanged(this, &SCkOptimizationDebuggerWindow::DoOnCleanupSelectionChanged)
            .OnContextMenuOpening(this, &SCkOptimizationDebuggerWindow::DoOnCleanupContextMenu)
            .OnMouseButtonDoubleClick(this, &SCkOptimizationDebuggerWindow::DoOnCleanupDoubleClicked)
            .SelectionMode(ESelectionMode::Multi)
            .Visibility_Lambda([this]() -> EVisibility
            {
                return _Model.Get_HasCleanupScan() ? EVisibility::Visible : EVisibility::Collapsed;
            })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_SnapshotsPage()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

    // Built ONCE, like every other page. Capture, cycling and deletion move the viewer's pointer, the strip's
    // children and the facts box's rows — never this tree. Swapping a page's children on a data change is the
    // one-frame-scrunch defect.
    const auto HasSnapshots = [this]() -> bool { return _SnapshotCount > 0; };

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                Build_ActionButton(ECk_Icon::Capture,
                    FText::FromString(TEXT("Capture Snapshot")),
                    FText::FromString(TEXT("Capture what the camera sees — the game camera in a play session or a ")
                        TEXT("build, the level viewport camera otherwise")),
                    FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnCaptureSnapshotClicked),
                    TAttribute<bool>{true})
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
            [
                Build_ActionButton(ECk_Icon::SkipBackward,
                    FText::FromString(TEXT("Previous")),
                    FText::FromString(TEXT("Show the previous snapshot, wrapping round to the newest")),
                    FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnCycleSnapshotClicked, -1),
                    TAttribute<bool>::CreateLambda(HasSnapshots))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                Build_ActionButton(ECk_Icon::SkipForward,
                    FText::FromString(TEXT("Next")),
                    FText::FromString(TEXT("Show the next snapshot, wrapping round to the oldest")),
                    FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnCycleSnapshotClicked, 1),
                    TAttribute<bool>::CreateLambda(HasSnapshots))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            [
                Build_ActionButton(ECk_Icon::Delete,
                    FText::FromString(TEXT("Delete")),
                    FText::FromString(TEXT("Discard the snapshot on screen")),
                    FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnDeleteSnapshotClicked),
                    TAttribute<bool>::CreateLambda(HasSnapshots))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, 0.0f)
        [
            SNew(SCkDebug_Card)
            .Visibility_Lambda([this]() -> EVisibility
            {
                return _SnapshotCount > 0 ? EVisibility::Collapsed : EVisibility::Visible;
            })
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(Get_IconBrush(ECk_Icon::Camera))
                    .Meaning(FText::FromString(TEXT("No snapshots yet")))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    .Size(FVector2D{k_EmptyStateIconSize, k_EmptyStateIconSize})
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("No snapshots yet. Capture takes a picture of what the camera ")
                            TEXT("sees; selection mode then identifies every mesh in it — in the editor or a ")
                            TEXT("Development build.")))
                    .AutoWrapText(true)
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            SAssignNew(_SnapshotStrip, SHorizontalBox)
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, CkStyle::SpaceM)
        [
            SNew(SHorizontalBox)
            .Visibility_Lambda([this]() -> EVisibility
            {
                return _SnapshotCount > 0 ? EVisibility::Visible : EVisibility::Collapsed;
            })

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SAssignNew(_SnapshotViewer, SCkOptimizationSnapshotViewer)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(k_SnapshotFactsWidth)
                [
                    SNew(SCkDebug_Card)
                    [
                        SAssignNew(_SnapshotFactsBox, SVerticalBox)
                    ]
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRebuild_Snapshots()
    -> void
{
    using namespace ck_optimization_debugger_window;

    _SnapshotCount = _Model.Get_Snapshots().Num();

    if (ck::IsValid(_SnapshotStrip))
    {
        _SnapshotStrip->ClearChildren();

        for (auto Index = 0; Index < _Model.Get_Snapshots().Num(); ++Index)
        {
            const auto& Snapshot = _Model.Get_Snapshots()[Index];
            const auto IsActive = Index == _Model.Get_ActiveSnapshotIndex();

            _SnapshotStrip->AddSlot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
            [
                SNew(SButton)
                .ToolTipText(FText::FromString(ck::Format_UE(TEXT("{} — {}x{}, {} mesh(es)"),
                    Snapshot.Label, Snapshot.Width, Snapshot.Height, Snapshot.Prims.Num())))
                .ContentPadding(FMargin{CkStyle::SpaceS, CkStyle::SpaceXS})
                .OnClicked_Lambda([this, Index]() -> FReply
                {
                    _Model.Set_ActiveSnapshotIndex(Index);
                    DoRebuild_Snapshots();

                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Snapshot.Label))
                    .ColorAndOpacity(FSlateColor{IsActive ? CkStyle::Accent() : CkStyle::TextDim()})
                ]
            ];
        }
    }

    const auto* Active = _Model.TryGet_ActiveSnapshot();

    if (ck::IsValid(_SnapshotViewer))
    { _SnapshotViewer->Set_Snapshot(Active); }

    if (NOT ck::IsValid(_SnapshotFactsBox))
    { return; }

    _SnapshotFactsBox->ClearChildren();

    if (Active == nullptr)
    { return; }

    const auto AddFact = [this](const FString& InLabel, const FString& InValue) -> void
    {
        _SnapshotFactsBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [
            SNew(STextBlock)
            .Text(FText::FromString(ck::Format_UE(TEXT("{}: {}"), InLabel, InValue)))
            .AutoWrapText(true)
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
        ];
    };

    AddFact(TEXT("World"), Active->WorldName);
    AddFact(TEXT("Size"), ck::Format_UE(TEXT("{}x{}"), Active->Width, Active->Height));
    AddFact(TEXT("Meshes"), FString::FromInt(Active->Prims.Num()));

    if (NOT Active->CaptureNotes.IsEmpty())
    { AddFact(TEXT("Notes"), Active->CaptureNotes); }

    if (NOT Active->HasIdMap)
    {
        // Said plainly rather than left to be discovered by clicking and having nothing happen.
        AddFact(TEXT("Selection"), TEXT("this snapshot carries no mesh identification, so it cannot be clicked into"));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRun_SnapshotCapture()
    -> void
{
    using namespace ck_optimization_debugger_snapshot_capture;

    auto* World = TryGet_CaptureWorld();

    if (ck::Is_NOT_Valid(World))
    {
        DoSet_Status(TEXT("No world to capture."), ECk_Tone::Warn);
        return;
    }

    const auto* Settings = UCkOptimizationDebuggerSettings::Get();
    const auto CapturedAt = FDateTime::Now();

    auto Params = FCkOptimizationDebugger_SnapshotCaptureParams{};
    Params.CaptureWidth = Settings != nullptr ? Settings->SnapshotCaptureWidth : 1280;
    Params.CapturedAt = CapturedAt;
    Params.Label = ck::Format_UE(TEXT("Snapshot {} — {}"),
        _Model.Get_Snapshots().Num() + 1, CapturedAt.ToString(TEXT("%H:%M:%S")));

    auto FailureReason = FString{};
    auto Captured = Run_Capture(World, Params, FailureReason);

    if (NOT Captured.IsSet())
    {
        DoSet_Status(FailureReason.IsEmpty() ? FString{TEXT("The capture failed.")} : FailureReason, ECk_Tone::Warn);
        return;
    }

    const auto PrimCount = Captured->Prims.Num();
    const auto Notes = Captured->CaptureNotes;

    // The viewer holds a pointer INTO the array this is about to grow, and growing it can reallocate.
    if (ck::IsValid(_SnapshotViewer))
    { _SnapshotViewer->Set_Snapshot(nullptr); }

    _Model.Add_Snapshot(MoveTemp(Captured.GetValue()), Settings != nullptr ? Settings->MaxStoredSnapshots : 8);

    DoRebuild_Snapshots();

    DoSet_Status(Notes.IsEmpty()
        ? ck::Format_UE(TEXT("Captured {} mesh(es)."), PrimCount)
        : ck::Format_UE(TEXT("Captured {} mesh(es). {}."), PrimCount, Notes), ECk_Tone::Ok);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnCaptureSnapshotClicked()
    -> FReply
{
    DoRun_SnapshotCapture();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnCycleSnapshotClicked(
        int32 InDelta)
    -> FReply
{
    _Model.Cycle_ActiveSnapshot(InDelta);
    DoRebuild_Snapshots();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnDeleteSnapshotClicked()
    -> FReply
{
    // Same reallocation hazard as capture: removing shifts every later element.
    if (ck::IsValid(_SnapshotViewer))
    { _SnapshotViewer->Set_Snapshot(nullptr); }

    _Model.Remove_ActiveSnapshot();
    DoRebuild_Snapshots();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_CleanupHeader()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_model;

    // Every tile reads the CACHED totals struct, refreshed by DoRebuild_Cleanup. An attribute that re-counted the
    // rows would do it on the paint path, every frame, over every package in the project.
    const auto Tile = [](const FString& InLabel, TAttribute<FText> InValue, const FString& InToolTip)
        -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .ToolTipText(FText::FromString(InToolTip))
            .Padding(FMargin{0.0f, 0.0f, CkStyle::SpaceXL, 0.0f})
            [
                SNew(SCkDebug_StatPair)
                .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                .Value(InValue)
                .Label(FText::FromString(InLabel))
            ];
    };

    auto Tiles = SNew(SHorizontalBox);

    for (const auto Category : Get_AllCleanupCategories())
    {
        const auto CategoryIndex = static_cast<int32>(Category);

        Tiles->AddSlot()
        .AutoWidth()
        [
            Tile(Get_CleanupCategoryLabel(Category).ToUpper(),
                TAttribute<FText>::CreateLambda([this, CategoryIndex]() -> FText
                {
                    return FText::FromString(_CleanupTotals.Categories.IsValidIndex(CategoryIndex)
                        ? Format_AbbreviatedCount(_CleanupTotals.Categories[CategoryIndex].RowCount)
                        : FString{TEXT("0")});
                }),
                Get_CleanupCategoryHint(Category))
        ];
    }

    Tiles->AddSlot()
    .AutoWidth()
    [
        Tile(TEXT("Reclaimable"),
            TAttribute<FText>::CreateLambda([this]() -> FText
            {
                return FText::FromString(Format_ByteSize(_CleanupTotals.ReclaimableBytes));
            }),
            TEXT("What the unreferenced assets weigh on disk, and only those. A duplicate's bytes are reclaimable ")
            TEXT("only once somebody decides which copy is redundant, and a dirty package is not on disk yet"))
    ];

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Project cleanup")))
            .SubText(FText::FromString(TEXT("presented for review — nothing here is deleted by a scan")))
            .Underline(true)
            .RightContent()
            [
                Build_CommandButton(ECk_Icon::Cleanup,
                    TEXT("Scan Project"),
                    TEXT("Review /Game for unreferenced assets, possible duplicates, redirectors and dirty ")
                    TEXT("packages. Loads nothing except what an action you press asks for"),
                    FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnCleanupScanClicked),
                    TAttribute<bool>::CreateSP(this, &SCkOptimizationDebuggerWindow::Get_CanScan))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            Tiles
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoCreate_CleanupCategoryTabs()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_model;

    auto Tabs = TArray<FCkDebug_UnderlineTabDesc>{};

    for (const auto Category : Get_AllCleanupCategories())
    {
        auto Tab = FCkDebug_UnderlineTabDesc{Get_CleanupCategoryId(Category),
            FText::FromString(Get_CleanupCategoryLabel(Category))};

        // The FILTERED count, matching what the memory sub-table selector does — a selector count that ignored the
        // search box would send the reader to a category the search has emptied. Cached, for the same reason.
        Tab.CountText = TAttribute<FText>::CreateLambda([this, Category]() -> FText
        {
            const auto Count = Get_CachedCleanupCount(Category);
            return Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty();
        });

        Tabs.Add(MoveTemp(Tab));
    }

    return SNew(SCkDebug_UnderlineTabs)
        .Tabs(Tabs)
        .FontSize(CkStyle::FontSizeSmall())
        .ActiveTabId_Lambda([this]() -> FName
        {
            return Get_CleanupCategoryId(_ActiveCleanupCategory);
        })
        .OnTabSelected_Lambda([this](FName InCategoryId)
        {
            const auto Category = TryGet_CleanupCategoryFromId(InCategoryId);

            if (NOT Category.IsSet())
            { return; }

            DoSelect_CleanupCategory(Category.GetValue());
        });
}

// --------------------------------------------------------------------------------------------------------------------
// Commands
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    Get_CanScan() const
    -> bool
{
#if WITH_EDITOR
    return true;
#else
    // The module ships in packaged Development/DebugGame builds, where there is no editor world to walk. A disabled
    // button plus the status line below is the honest presentation of "not here", rather than a live button that
    // silently reports a clean level.
    return false;
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnScanClicked()
    -> FReply
{
    DoRun_Scan();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRun_Scan()
    -> bool
{
    const auto Thresholds = ck_optimization_debugger_thresholds::Build_FromSettings();

    auto* EditorWorld = ck_optimization_debugger_scan::TryGet_EditorWorld();

    // The exclusion set is handed IN, so a level the user switched off is skipped whole — gather and checks — rather
    // than walked and then filtered out of the answer.
    const auto Result = ck_optimization_debugger_scan::Run_Scan(EditorWorld, Thresholds,
        _Model.Get_ExcludedLevelNames());

    if (Result.RequiresEditor)
    {
        DoSet_Status(TEXT("Scanning needs an open editor level — nothing to analyze here."), ECk_Tone::Neutral);
        return false;
    }

    _SelectedFindingKey.Empty();

    // Row identity is reused BETWEEN filter passes, not across scans: a re-scan can reproduce a key with a
    // different severity or explanation behind it, and the row widgets built for the old values are only rebuilt
    // when the set changes. Dropping the map makes every line new, which is what a fresh answer is.
    _FindingItems.Reset();
    _FindingItemsByKey.Reset();

    // BEFORE the findings go in, and through the one setter that rotates the previous census into place. This is the
    // only place a delta is captured, which is what makes the post-fix refresh produce one too — a fix-triggered
    // re-scan takes exactly this path.
    _Model.Set_Summary(Result.Summary);

    _Model.Set_Findings(Result.Findings);
    _Model.Set_ScanInfo(Result.ScannedLevelNames, FDateTime::Now());

    DoRebuild_All();

    // `DoRebuild_All` already wrote the normal status line. These two override it, because both are things the
    // reader has to know BEFORE they read the count: a partial scan and a partial scope both make "0 findings" a
    // different sentence.
    if (Result.WasCancelled)
    {
        DoSet_Status(ck::Format_UE(TEXT("Scan cancelled — {} finding(s) from the part that ran."),
            _Model.Get_Findings().Num()), ECk_Tone::Warn);
    }
    else if (NOT Result.SkippedUnloadedLevelNames.IsEmpty())
    {
        DoSet_Status(ck::Format_UE(TEXT("{} finding(s) — {} sub-level(s) NOT scanned because they are not loaded: {}"),
            _Model.Get_Findings().Num(),
            Result.SkippedUnloadedLevelNames.Num(),
            FString::Join(Result.SkippedUnloadedLevelNames, TEXT(", "))), ECk_Tone::Warn);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoSolo_Severity(
        ECkOptimizationDebugger_Severity InSeverity)
    -> void
{
    // The state change is the model's, so a spec pins the behaviour without a widget. The window's job is the two
    // consequences: re-render the list against the new mask, and put the reader on the page that shows it — a badge
    // that silently narrowed a page nobody was looking at would read as doing nothing.
    _Model.Set_SeveritySolo(InSeverity);

    DoSelect_Page(ECkOptimizationDebugger_Page::Findings);
    DoRebuild_Findings();

    DoSet_Status(ck::Format_UE(TEXT("Showing {} findings only — the severity toggles in the title bar widen it again."),
        ck_optimization_debugger_model::Get_SeverityLabel(InSeverity).ToLower()), ECk_Tone::Neutral);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoSet_LevelExcluded(
        FName InLevelName,
        bool InExcluded)
    -> void
{
    _Model.Set_LevelExcluded(InLevelName, InExcluded);

    // Persisted immediately rather than at window close: a scope narrowed once should survive an editor that
    // crashes, and there is no "apply" step in this panel to hang the write off.
    UCkOptimizationDebuggerSettings::Save_ExcludedLevelNames(_Model.Get_ExcludedLevelNames());

    // Nothing re-scans here. The toggle is a statement about the NEXT scan, and silently re-walking a large world
    // because somebody flicked a switch is not what the switch says it does.
    DoSet_Status(InExcluded
        ? ck::Format_UE(TEXT("{} will be skipped by the next scan."), InLevelName)
        : ck::Format_UE(TEXT("{} is back in scope for the next scan."), InLevelName),
        ECk_Tone::Neutral);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnGoToClicked()
    -> FReply
{
    DoNavigate_ToSelected();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnOpenAssetClicked()
    -> FReply
{
    DoOpenAsset_Selected();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnApplyFixClicked()
    -> FReply
{
    DoApply_FixToSelected();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    Get_SelectedFindings() const
    -> TArray<FCkOptimizationDebugger_FindingRow>
{
    auto Findings = TArray<FCkOptimizationDebugger_FindingRow>{};

    if (ck::IsValid(_FindingList))
    {
        for (const auto& Item : _FindingList->GetSelectedItems())
        {
            if (NOT Item.IsValid() || Item->IsGroupHeader)
            { continue; }

            Findings.Add(Item->Finding);
        }
    }

    // The list's own selection is the truth whenever it has one. The remembered key is the fallback for the state a
    // rebuild leaves behind, where the detail panel still names a finding the widget no longer reports as selected.
    if (Findings.IsEmpty())
    {
        if (const auto* Finding = TryGet_SelectedFinding())
        { Findings.Add(*Finding); }
    }

    return Findings;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoNavigate_ToSelected()
    -> void
{
    const auto Selected = Get_SelectedFindings();

    if (Selected.IsEmpty())
    {
        DoSet_Status(TEXT("Select a finding first."), ECk_Tone::Neutral);
        return;
    }

    // One target even from a multi-selection: "go to" names a place, and there is one viewport and one Content
    // Browser to put the reader in. The detailed finding wins, because that is the one they are reading about.
    const auto* Detailed = TryGet_SelectedFinding();

    DoNavigate_To(Detailed != nullptr ? *Detailed : Selected[0]);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoNavigate_To(
        const FCkOptimizationDebugger_FindingRow& InFinding)
    -> void
{
    const auto Result = ck_optimization_debugger_navigation::Navigate_ToFinding(InFinding);

    // A failed navigation is Warn, never Err: an actor whose sub-level has been unloaded since the scan is the
    // ordinary case, and the message already says which one it was.
    DoSet_Status(Result.Message, Result.Succeeded ? ECk_Tone::Ok : ECk_Tone::Warn);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOpenAsset_Selected()
    -> void
{
    const auto Selected = Get_SelectedFindings();

    if (Selected.IsEmpty())
    {
        DoSet_Status(TEXT("Select a finding first."), ECk_Tone::Neutral);
        return;
    }

    const auto* Detailed = TryGet_SelectedFinding();

    DoOpenAsset_ForTarget(Detailed != nullptr ? Detailed->Target : Selected[0].Target);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOpenAsset_ForTarget(
        FCkOptimizationDebugger_Target InTarget)
    -> void
{
    const auto Result = ck_optimization_debugger_navigation::Open_TargetAsset(InTarget);

    DoSet_Status(Result.Message, Result.Succeeded ? ECk_Tone::Ok : ECk_Tone::Warn);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoApply_FixToSelected()
    -> void
{
    using namespace ck_optimization_debugger_fixes;

    DoApply_Fixes(Get_FixableFindings(Get_SelectedFindings()), TEXT("the selection"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoApply_FixAllVisible()
    -> void
{
    using namespace ck_optimization_debugger_fixes;

    // The VISIBLE findings, never `_Model.Get_Findings()`. The reader has just narrowed the list with a path scope,
    // a suggested-fix toggle and a mute set; a "Fix All" that ignored those would apply fixes to rows they
    // deliberately excluded — including MUTED ones, which they explicitly told this tool to leave alone. The button
    // says the count for the same reason: this must never be a blind action.
    DoApply_Fixes(Get_FixableFindings(_Model.Get_VisibleFindings()), TEXT("the visible list"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoApply_Fixes(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFixable,
        const FString& InScopeLabel)
    -> void
{
    using namespace ck_optimization_debugger_fixes;

    const auto& Fixable = InFixable;

    if (Fixable.IsEmpty())
    {
        DoSet_Status(ck::Format_UE(TEXT("Nothing in {} has an automatic fix."), InScopeLabel), ECk_Tone::Neutral);
        return;
    }

    if (NOT Get_CanApplyFixes())
    {
        DoSet_Status(Get_FixesUnavailableReason(), ECk_Tone::Neutral);
        return;
    }

    // The confirmation gate. Only a batch that DESTROYS actors or writes a project config file asks — a property
    // edit inside a transaction is one Ctrl+Z away and needs no ceremony. Without this, "Fix 12 Findings" could
    // delete hundreds of actors and rewrite DefaultEngine.ini on a single click, and the registry has carried an
    // `IsDestructive` flag for exactly this since P2 with nothing reading it.
    const auto Confirmation = Build_BatchConfirmation(Fixable);

    if (Confirmation.IsRequired)
    {
        const auto Prompt = FText::FromString(ck::Format_UE(TEXT("{}\n\n{}\n\nApply {} fix(es)?"),
            Confirmation.Title, Confirmation.Body, Fixable.Num()));

        if (FMessageDialog::Open(EAppMsgType::YesNo, Prompt) != EAppReturnType::Yes)
        {
            DoSet_Status(TEXT("Nothing was applied — you declined the confirmation."), ECk_Tone::Neutral);
            return;
        }
    }

    auto* EditorWorld = ck_optimization_debugger_scan::TryGet_EditorWorld();

    // ONE fix goes through the single-fix entry point, which labels the undo record with that fix's own verb —
    // "Enable Nanite (SM_Foo)" rather than "Apply 1 optimization fix(es)". Routing everything through the batch made
    // that label unreachable and the per-fix path dead code.
    auto Batch = FCkOptimizationDebugger_BatchFixResult{};

    if (Fixable.Num() == 1)
    {
        const auto Result = Apply_Fix(Fixable[0], EditorWorld);

        Batch.SucceededCount = Result.Succeeded ? 1 : 0;
        Batch.FailedCount = Result.Succeeded ? 0 : 1;
        Batch.ChangedState = Result.ChangedState;
        Batch.Messages.Add(Result.Message);
    }
    else
    {
        Batch = Apply_Fixes(Fixable, EditorWorld);
    }

    const auto Tone = Batch.FailedCount == 0
        ? ECk_Tone::Ok
        : (Batch.SucceededCount > 0 ? ECk_Tone::Warn : ECk_Tone::Err);

    // Findings are refreshed after fixes, because a fix that landed changed the answer the list is showing. A
    // review-style fix changes nothing, so it does not pay for a whole re-scan to reprint the same rows.
    const auto Rescanned = Batch.ChangedState && DoRun_Scan();

    // One fix says what it did; a batch says how it went. Concatenating a dozen messages onto a one-line status
    // strip would push the counts off the end of it.
    const auto Detail = Batch.Messages.Num() == 1 ? Batch.Messages[0] : FString{};

    DoSet_Status(Rescanned
        ? ck::Format_UE(TEXT("{} fixed, {} failed — rescanned: {} finding(s). {}"),
            Batch.SucceededCount, Batch.FailedCount, _Model.Get_Findings().Num(), Detail)
        : ck::Format_UE(TEXT("{} fixed, {} failed. {}"),
            Batch.SucceededCount, Batch.FailedCount, Detail),
        Tone);
}

// --------------------------------------------------------------------------------------------------------------------
// Rebuild entry points
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRebuild_All(
        bool InRefreshStatus)
    -> void
{
    DoRebuild_Dashboard();
    DoRebuild_Findings(InRefreshStatus);

    // The memory page is not touched by a level scan — but a style revision and a session invalidation both route
    // here, and both have to reach it. Over an empty census this is three cheap loops and no allocation.
    DoRebuild_Memory();

    // The profiling page's ONLY derived state: five chips at most, and usually none. Everything else on that page is
    // an attribute over the live viewport, which is why there is nothing bigger to rebuild — but a style revision
    // still has to re-create the chips, because a chip reads its brushes when it is built.
    DoRebuild_Profiling();

    // Not touched by a level scan either — but a style revision routes here and has to reach it, and over an empty
    // census this is one loop and no allocation. A session invalidation reaches it too, and finds the cleanup rows
    // deliberately still there: an asset nothing references does not stop being unreferenced because somebody
    // pressed Play.
    DoRebuild_Cleanup();

    // And again for the snapshots, which survive a session invalidation for a stronger version of the same reason:
    // every one is a picture of a moment that has already passed.
    DoRebuild_Snapshots();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRebuild_Dashboard()
    -> void
{
    using namespace ck_optimization_debugger_window;

    if (ck::Is_NOT_Valid(_DashboardBox))
    { return; }

    // A threshold field the user is halfway through typing into is INSIDE this tree. Tearing it down here would eat
    // the keystrokes; the guard turns that into a deferral, and the editor's own end-of-edit consumes it.
    if (_ThresholdEditGuard.IsValid() && _ThresholdEditGuard->Get_HasActiveEdit())
    {
        _ThresholdEditGuard->Request_Rebuild();
        return;
    }

    if (_ThresholdEditGuard.IsValid())
    { _ThresholdEditGuard->Clear_AllEdits(); }

    _DashboardBox->ClearChildren();

    const auto AddSection = [this](TSharedRef<SWidget> InSection, float InBottomPadding) -> void
    {
        _DashboardBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, InBottomPadding)
        [
            InSection
        ];
    };

    // Before the first scan the dashboard has nothing honest to say about a level, so it says what to do instead of
    // printing a page of zeroes. The threshold panel stays: those are worth setting BEFORE the first scan, not
    // after it.
    if (NOT _Model.Get_HasSummary())
    {
        AddSection(DoBuild_DashboardEmptyState(), CkStyle::SpaceL);
        AddSection(DoBuild_DashboardThresholdsSection(), 0.0f);
        return;
    }

    AddSection(DoBuild_DashboardTiles(), CkStyle::SpaceL);
    AddSection(DoBuild_DashboardSeverityStrip(), CkStyle::SpaceL);
    AddSection(DoBuild_DashboardDiskSection(), CkStyle::SpaceL);
    AddSection(DoBuild_DashboardLevelsSection(), CkStyle::SpaceL);
    AddSection(DoBuild_DashboardThresholdsSection(), 0.0f);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoBuild_DashboardEmptyState()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

    return SNew(SCkDebug_Card)
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(Get_IconBrush(ECk_Icon::ProfileTiming))
                .Meaning(FText::FromString(TEXT("Nothing has been analyzed yet")))
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                .Size(FVector2D{k_EmptyStateIconSize, k_EmptyStateIconSize})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("No scan yet. Analyze the persistent level and every loaded sub-level ")
                        TEXT("to see actor, mesh, triangle, material and light counts, the findings ")
                        TEXT("they produced, and what the project costs on disk.")))
                .AutoWrapText(true)
                .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Left)
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            // The SAME command the toolbar's Scan button runs — one scan path, two places to reach it.
            Build_CommandButton(ECk_Icon::Target,
                TEXT("Scan the open levels"),
                TEXT("Analyze the persistent level and every loaded sub-level"),
                FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnScanClicked),
                TAttribute<bool>::CreateSP(this, &SCkOptimizationDebuggerWindow::Get_CanScan))
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoBuild_DashboardTiles()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_model;

    auto Tiles = SNew(SHorizontalBox);

    // One tile description: what to read out of the summary, what to read out of the delta, and whether the change
    // has a better direction. Everything is a lambda over the model, so the tiles track the next scan without this
    // function running again.
    const auto AddTile = [this, &Tiles](
        const FString& InLabel,
        const FString& InToolTip,
        TFunction<int64(const FCkOptimizationDebugger_ScanSummary&)> InReadValue,
        TFunction<int64(const FCkOptimizationDebugger_SummaryDelta&)> InReadDelta,
        bool InFewerIsBetter) -> void
    {
        Tiles->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Top)
        .Padding(0.0f, 0.0f, CkStyle::SpaceXL, 0.0f)
        [
            Build_StatTile(InLabel,
                TAttribute<FText>::CreateLambda([this, InReadValue]() -> FText
                {
                    return FText::FromString(Format_AbbreviatedCount(InReadValue(_Model.Get_Summary())));
                }),
                TAttribute<FText>::CreateLambda([this, InReadDelta]() -> FText
                {
                    const auto Delta = _Model.Get_SummaryDelta();

                    // No previous scan is not "no change" — an em dash says the comparison does not exist, where a
                    // "+0" would claim it does and came out even.
                    if (NOT Delta.HasPrevious)
                    { return FText::FromString(TEXT("—")); }

                    return FText::FromString(Format_Delta(InReadDelta(Delta)));
                }),
                TAttribute<FSlateColor>::CreateLambda([this, InReadDelta, InFewerIsBetter]() -> FSlateColor
                {
                    const auto Delta = _Model.Get_SummaryDelta();

                    if (NOT Delta.HasPrevious)
                    { return FSlateColor{CkStyle::TextMute()}; }

                    const auto Tone = Get_DeltaTone(InReadDelta(Delta), InFewerIsBetter);

                    return FSlateColor{Tone == ECk_Tone::Neutral ? CkStyle::TextMute() : CkStyle::GetToneColor(Tone)};
                }),
                InToolTip)
        ];
    };

    AddTile(TEXT("Actors"),
        TEXT("Every actor in every scanned level, including sub-levels"),
        [](const FCkOptimizationDebugger_ScanSummary& InSummary) { return static_cast<int64>(InSummary.ActorCount); },
        [](const FCkOptimizationDebugger_SummaryDelta& InDelta) { return InDelta.ActorCountDelta; },
        false);

    AddTile(TEXT("Static meshes"),
        TEXT("Distinct static-mesh ASSETS the scanned levels place. One mesh placed a thousand times is one row here"),
        [](const FCkOptimizationDebugger_ScanSummary& InSummary)
        { return static_cast<int64>(InSummary.UniqueStaticMeshCount); },
        [](const FCkOptimizationDebugger_SummaryDelta& InDelta) { return InDelta.UniqueStaticMeshCountDelta; },
        false);

    AddTile(TEXT("LOD0 tris"),
        TEXT("LOD0 triangles summed over the DISTINCT meshes above — the content budget, not the frame cost"),
        [](const FCkOptimizationDebugger_ScanSummary& InSummary) { return InSummary.Lod0TriangleTotal; },
        [](const FCkOptimizationDebugger_SummaryDelta& InDelta) { return InDelta.Lod0TriangleTotalDelta; },
        false);

    AddTile(TEXT("Materials"),
        TEXT("Distinct materials and material instances assigned across the scanned levels"),
        [](const FCkOptimizationDebugger_ScanSummary& InSummary)
        { return static_cast<int64>(InSummary.UniqueMaterialCount); },
        [](const FCkOptimizationDebugger_SummaryDelta& InDelta) { return InDelta.UniqueMaterialCountDelta; },
        false);

    AddTile(TEXT("Lights"),
        TEXT("Every light component in the scanned levels — the mobility split is on the level rows below"),
        [](const FCkOptimizationDebugger_ScanSummary& InSummary) { return static_cast<int64>(InSummary.LightCount); },
        [](const FCkOptimizationDebugger_SummaryDelta& InDelta) { return InDelta.LightCountDelta; },
        false);

    AddTile(TEXT("Findings"),
        TEXT("Everything the checks reported, before any filtering. Fewer is better, so this one is coloured"),
        [](const FCkOptimizationDebugger_ScanSummary& InSummary)
        { return static_cast<int64>(InSummary.FindingCounts.Get_Total()); },
        [](const FCkOptimizationDebugger_SummaryDelta& InDelta) { return InDelta.FindingTotalDelta; },
        true);

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Level stats")))
            .SubText(FText::FromString(TEXT("whole scan, with the change since the previous one")))
            .Underline(true)
            .RightContent()
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    return FText::FromString(_Model.Get_HasScanned()
                        ? _Model.Get_LastScanTime().ToString()
                        : FString{TEXT("—")});
                })
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            Tiles
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoBuild_DashboardSeverityStrip()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

    auto Strip = SNew(SHorizontalBox);

    for (const auto Severity : ck_optimization_debugger_model::Get_AllSeverities())
    {
        const auto Label = ck_optimization_debugger_model::Get_SeverityLabel(Severity);
        const auto Tone = ck_optimization_debugger_model::Get_SeverityTone(Severity);

        Strip->AddSlot()
        .AutoWidth()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            // A BUTTON, not a row child: this is toolbar-level chrome, so consuming the click is exactly what it is
            // for. The same widget inside an SListView row would be the first click-trap.
            //
            // "HoverHintOnly" is the borderless button treatment already proven in this suite
            // (`SCkDebug_HistoryRow`), not a style name recalled from a tutorial — an engine style that does not
            // exist here resolves to a default-constructed one SILENTLY, and the pill would render as a grey box.
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
            .ContentPadding(FMargin{CkStyle::SpaceXS})
            .ToolTipText(FText::FromString(ck::Format_UE(
                TEXT("Show only {} findings — switches to Level analysis with the other severities hidden"),
                Label.ToLower())))
            .OnClicked_Lambda([this, Severity]() -> FReply
            {
                DoSolo_Severity(Severity);
                return FReply::Handled();
            })
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(Get_IconBrush(Get_SeverityIcon(Severity)))
                    .Meaning(FText::FromString(Label))
                    .ColorAndOpacity(FSlateColor{CkStyle::GetToneColor(Tone)})
                    .Size(FVector2D{k_PanelIconSize, k_PanelIconSize})
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_CountBadge)
                    .ValueText_Lambda([this, Severity]() -> FText
                    {
                        // The cached UNFILTERED census — three badges each re-deriving it would be three full walks
                        // of the findings list per frame while the dashboard is up.
                        const auto& Counts = _TotalSeverityCounts;

                        switch (Severity)
                        {
                            case ECkOptimizationDebugger_Severity::Critical: return FText::AsNumber(Counts.CriticalCount);
                            case ECkOptimizationDebugger_Severity::Major:    return FText::AsNumber(Counts.MajorCount);
                            case ECkOptimizationDebugger_Severity::Minor:    return FText::AsNumber(Counts.MinorCount);
                            default:                                         return FText::AsNumber(0);
                        }
                    })
                    .SuffixText(FText::FromString(Label.ToLower()))
                    .ValueColor(CkStyle::GetToneColor(Tone))
                    .SuffixColor(CkStyle::TextMute())
                    .BackgroundColor(CkStyle::Bg2())
                    .BorderColor(CkStyle::Border())
                ]
            ]
        ];
    }

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Findings by severity")))
            .SubText(FText::FromString(TEXT("click one to see only those")))
            .Underline(true)
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            Strip
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoBuild_DashboardDiskSection()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_model;

    const auto& Disk = _Model.Get_Summary().Disk;

    auto Rows = SNew(SVerticalBox);

    if (NOT Disk.IsAvailable)
    {
        Rows->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("The asset registry could not be reached, so nothing can be said about ")
                    TEXT("what the project costs on disk.")))
            .AutoWrapText(true)
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
        ];
    }
    else if (Disk.Categories.IsEmpty())
    {
        Rows->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("No saved content under /Game.")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
        ];
    }

    // Already sorted largest-first with a total tie-break by the scan — rendered in the order it arrived in, never
    // re-sorted here, so the row order is the scan's contract rather than this function's.
    for (const auto& Category : Disk.Categories)
    {
        const auto Fraction = Get_DiskCategoryFraction(Category.TotalBytes, Disk.TotalBytes);
        const auto Color = ck::debug_axes::Get_CategoricalColor(static_cast<int32>(Category.Category));

        Rows->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SCkDebug_KeyValueRow)
                .KeyText(FText::FromString(Get_DiskCategoryLabel(Category.Category)))
                .ValueText(FText::FromString(Format_ByteSize(Category.TotalBytes)))
                .Tone(ECkDebug_KeyValueTone::Custom)
                .CustomValueColor(CkStyle::Text())
                .ShowMarker(true)
                .MarkerColor(Color)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .ToolTipText(FText::FromString(ck::Format_UE(TEXT("{} of {} across {} package(s)"),
                    Format_ByteSize(Category.TotalBytes),
                    Format_ByteSize(Disk.TotalBytes),
                    Category.PackageCount)))
                [
                    // Fraction is a fixed number for the life of this sub-tree: the breakdown only changes when a
                    // scan re-runs, and a scan rebuilds this section outright.
                    SNew(SCkDebug_MeterBar)
                    .Fraction(Fraction)
                    .FillColor(Color)
                    .DesiredSize(FVector2D{k_MeterWidth, k_MeterHeight})
                ]
            ]
        ];
    }

    const auto SubText = Disk.WasStillIndexing
        ? FString{TEXT("/Game — the asset registry was still indexing, so this is a floor")}
        : FString{TEXT("/Game, by asset family")};

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Project disk size")))
            .SubText(FText::FromString(SubText))
            .Underline(true)
            .RightContent()
            [
                SNew(STextBlock)
                .Text(FText::FromString(Disk.IsAvailable
                    ? ck::Format_UE(TEXT("{} · {} packages"), Format_ByteSize(Disk.TotalBytes), Disk.PackageCount)
                    : FString{TEXT("unavailable")}))
                .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor{Disk.IsAvailable ? CkStyle::Text() : CkStyle::TextMute()})
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            Rows
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoBuild_DashboardLevelsSection()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

    const auto& Levels = _Model.Get_Summary().Levels;

    // A plain SVerticalBox, NOT an SListView. The level set is small and fixed between scans, so there is no
    // virtualization to buy — and a plain box is what makes an interactive switch legal in a row at all: inside a
    // table row the switch would consume the click that selects it.
    auto Rows = SNew(SVerticalBox);

    if (Levels.IsEmpty())
    {
        Rows->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("The last scan reached no level.")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
        ];
    }

    for (const auto& Level : Levels)
    {
        const auto LevelName = FName{*Level.LevelName};
        const auto IsUnloaded = NOT Level.IsLoaded;
        const auto IsPersistent = Level.IsPersistentLevel;

        // An ATTRIBUTE, not a captured string: the toggle changes what this row says, and nothing rebuilds the
        // section when it does. A frozen hint would leave a greyed-out level still describing itself as in scope.
        const auto Hint = TAttribute<FText>::CreateLambda([this, LevelName, IsUnloaded, IsPersistent]() -> FText
        {
            if (IsUnloaded)
            { return FText::FromString(TEXT("not loaded — nothing can be said about it")); }

            if (_Model.Get_LevelExcluded(LevelName))
            { return FText::FromString(TEXT("excluded from the next scan")); }

            return FText::FromString(IsPersistent ? TEXT("persistent level") : TEXT("sub-level"));
        });

        Rows->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(Get_IconBrush(ECk_Icon::World))
                .Meaning(Hint)
                .ColorAndOpacity_Lambda([this, LevelName]() -> FSlateColor
                {
                    return FSlateColor{_Model.Get_LevelExcluded(LevelName) ? CkStyle::TextMute() : CkStyle::TextDim()};
                })
                .Size(FVector2D{k_RowIconSize, k_RowIconSize})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Level.LevelName))
                .Font(Level.IsPersistentLevel
                    ? CkStyle::BoldFont(CkStyle::FontSizeBody())
                    : CkStyle::RegularFont(CkStyle::FontSizeBody()))
                .ColorAndOpacity_Lambda([this, LevelName]() -> FSlateColor
                {
                    // Greyed rather than removed: a level that vanished when it was switched off would make a
                    // narrowed scan look like a smaller project, and there would be no row left to switch back on.
                    return FSlateColor{_Model.Get_LevelExcluded(LevelName) ? CkStyle::TextMute() : CkStyle::Text()};
                })
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(STextBlock)
                .Text(Hint)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(SBox)
                .MinDesiredWidth(k_LevelActorColumnWidth)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Level.IsLoaded
                        ? ck::Format_UE(TEXT("{} actors"), Level.ActorCount)
                        : FString{TEXT("—")}))
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .ToolTipText(FText::FromString(IsUnloaded
                    ? TEXT("This sub-level is not loaded, so there is nothing for a scan to include")
                    : TEXT("Include this level in the next scan. Switching it off skips it entirely — it is never ")
                      TEXT("walked and never checked")))
                .IsEnabled(NOT IsUnloaded)
                [
                    SNew(SCkDebug_Switch)
                    .IsOn_Lambda([this, LevelName]() -> bool
                    {
                        return NOT _Model.Get_LevelExcluded(LevelName);
                    })
                    .OnStateChanged_Lambda([this, LevelName](bool InIsIncluded)
                    {
                        DoSet_LevelExcluded(LevelName, NOT InIsIncluded);
                    })
                ]
            ]
        ];
    }

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Levels")))
            .SubText(FText::FromString(TEXT("switching one off takes effect on the NEXT scan")))
            .Underline(true)
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            Rows
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoBuild_DashboardThresholdsSection()
    -> TSharedRef<SWidget>
{
    using namespace ck_optimization_debugger_window;

    const auto Entries = Get_ThresholdEntries();

    auto Rows = SNew(SVerticalBox);

    for (const auto& Entry : Entries)
    {
        const auto* Property = Entry.Property;

        if (Property == nullptr)
        { continue; }

        // One scope per FIELD, held by the delegates as a TSharedPtr by value: a control destroyed mid-type (panel
        // rebuild, style revision, window close) releases its edit instead of wedging the guard forever.
        const auto Scope = MakeShared<FCkInspectorEditScope>(_ThresholdEditGuard);

        Rows->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(Entry.Label))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::Text())
            .ValueWidget()
            [
                SNew(SBox)
                .ToolTipText(FText::FromString(Entry.ToolTip))
                .HAlign(HAlign_Right)
                [
                    SNew(SCkDebug_NumericEditor)
                    .Kind(ECkDebug_NumericKind::Integer)
                    .MinValue(Entry.MinValue)
                    .Value_Lambda([Property]() -> double
                    {
                        const auto* Settings = UCkOptimizationDebuggerSettings::Get();

                        if (Settings == nullptr)
                        { return 0.0; }

                        return static_cast<double>(Property->GetPropertyValue_InContainer(Settings));
                    })
                    .OnValueCommitted_Lambda([Property](double InValue)
                    {
                        // Per-user store: `config=GameUserSettings` with an "Editor" container, i.e. the same place
                        // Editor Preferences → Ck writes. Tightening a threshold must never dirty a committed
                        // project config for the whole team.
                        auto* Settings = GetMutableDefault<UCkOptimizationDebuggerSettings>();

                        if (Settings == nullptr)
                        { return; }

                        Property->SetPropertyValue_InContainer(Settings, static_cast<int32>(InValue));
                        Settings->SaveConfig();
                    })
                    .OnEditStateChanged_Lambda([this, Scope](bool InIsEditing)
                    {
                        Scope->Set_Active(InIsEditing);

                        if (InIsEditing || NOT _ThresholdEditGuard.IsValid())
                        { return; }

                        if (NOT _ThresholdEditGuard->Get_HasPendingRebuild())
                        { return; }

                        // The edit is over and something asked for a rebuild while it was in flight — but this
                        // callback is running INSIDE the text box's own commit handler, and rebuilding here would
                        // destroy the widget that is still on the stack. A one-shot active timer runs the rebuild on
                        // the next frame instead, outside the event that triggered it. This is not a Tick: it fires
                        // once, from a user action, and unregisters itself.
                        RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(
                            this, &SCkOptimizationDebuggerWindow::DoOnDeferredDashboardRebuild));
                    })
                ]
            ]
        ];
    }

    if (Entries.IsEmpty())
    {
        Rows->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("No configurable thresholds were found on the settings object.")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
        ];
    }

    return SNew(SCkDebug_InspectorPanel)
        .Title(FText::FromString(TEXT("Analysis thresholds")))
        .CountText(FText::AsNumber(Entries.Num()))
        .StatusPillText(FText::FromString(TEXT("PER-USER")))
        .StatusPillTone(ECk_Tone::Info)
        // Collapsed by default: twelve editable numbers above the level list would bury the answer the reader came
        // for behind the settings they only tune once.
        .StartExpanded(false)
        .IconBrush(Get_IconBrush(ECk_Icon::Size))
        .IconColor(CkStyle::TextDim())
        .Body()
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("What the checks compare against. Committed on Enter or focus loss and ")
                        TEXT("saved per-user immediately; the NEXT scan reads them.")))
                .AutoWrapText(true)
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                Rows
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRebuild_Findings(
        bool InRefreshStatus)
    -> void
{
    using namespace ck_optimization_debugger_window;

    const auto Groups = _Model.Get_VisibleFindingsGroupedByCheck();
    const auto& Filter = _Model.Get_Filter();

    // Cached HERE — a filter change, a severity toggle and a scan all land in this function, and those are exactly
    // the three things that can move these numbers. The tab bar's attributes then read a field.
    _VisibleFindingCount = _Model.Get_VisibleFindingCount();
    _VisibleSeverityCounts = _Model.Get_VisibleCountsBySeverity();
    _TotalSeverityCounts = _Model.Get_CountsBySeverity();
    _ToggleSeverityCounts = _Model.Get_VisibleCountsBySeverity_IgnoringSeverityMask();
    _VisibleCategoryCounts = _Model.Get_VisibleCountsByCategory();

    // Every one of these counts is over what the FILTER admits, and none of them consults the collapse set. A folded
    // group is still counted everywhere it appears — that is the difference between folding a section and filtering
    // it out, and it is the whole reason collapse state does not live on the filter.

    // Stable row identity: reuse the TSharedPtr whose key matches, allocate only for genuinely new lines, and ask
    // the list to refresh only when the SET changed. Resetting and re-allocating per rebuild would destroy the
    // user's selection every time they typed a character into the filter box.
    auto Existing = MoveTemp(_FindingItemsByKey);
    _FindingItemsByKey.Reset();
    _FindingItems.Reset();

    auto SetChanged = false;

    // Every finding the FILTER admits, whether or not its group is folded. The selection prune below tests this
    // rather than the item map: a collapsed finding has no row, but it has not been filtered away, and clearing the
    // detail panel when the reader folds the group they were reading would make folding destructive.
    auto VisibleFindingKeys = TSet<FString>{};

    const auto TakeItem = [&Existing, &SetChanged](const FString& InKey) -> FFindingItem
    {
        if (auto* Found = Existing.Find(InKey))
        {
            auto Item = *Found;
            Existing.Remove(InKey);
            return Item;
        }

        SetChanged = true;
        return MakeShared<FCkOptimizationDebugger_FindingListItem>();
    };

    for (const auto& Group : Groups)
    {
        const auto GroupKey = k_GroupKeyPrefix + Group.CheckId.ToString();
        const auto IsCollapsed = _Model.Get_IsCheckCollapsed(Group.CheckId);

        auto GroupStableKeys = TArray<FString>{};
        GroupStableKeys.Reserve(Group.Findings.Num());

        auto GroupQueuedCount = 0;

        // Walked for EVERY group, folded or not: the header's staged count is the one thing that has to keep being
        // true while its rows are out of sight, or folding a group with staged work in it would make that work
        // vanish from the screen entirely.
        for (const auto& Finding : Group.Findings)
        {
            GroupStableKeys.Add(Finding.StableKey);
            VisibleFindingKeys.Add(Finding.StableKey);

            if (_Model.Get_IsQueued(Finding.StableKey))
            { ++GroupQueuedCount; }
        }

        auto HeaderItem = TakeItem(GroupKey);

        HeaderItem->Key = GroupKey;
        HeaderItem->IsGroupHeader = true;
        HeaderItem->CheckId = Group.CheckId;
        HeaderItem->Severity = Group.WorstSeverity;
        HeaderItem->Category = Group.Category;
        HeaderItem->GroupCount = Group.Findings.Num();
        HeaderItem->Title = Group.Title;
        HeaderItem->Finding = FCkOptimizationDebugger_FindingRow{};
        HeaderItem->IsHighlightMatch = true;
        HeaderItem->IsCollapsed = IsCollapsed;
        HeaderItem->GroupStableKeys = MoveTemp(GroupStableKeys);
        HeaderItem->GroupQueuedCount = GroupQueuedCount;

        _FindingItemsByKey.Add(GroupKey, HeaderItem);
        _FindingItems.Add(MoveTemp(HeaderItem));

        for (const auto& Finding : Group.Findings)
        {
            auto Item = TakeItem(Finding.StableKey);

            Item->Key = Finding.StableKey;
            Item->IsGroupHeader = false;
            Item->CheckId = Finding.CheckId;
            Item->Severity = Finding.Severity;
            Item->Category = Finding.Category;
            Item->GroupCount = 0;
            Item->Title = Finding.Title;
            Item->Finding = Finding;
            Item->IsHighlightMatch = ck_optimization_debugger_model::Matches_Highlight(Finding, Filter);
            Item->IsCollapsed = false;
            Item->GroupQueuedCount = 0;
            Item->IsQueued = _Model.Get_IsQueued(Finding.StableKey);

            // Every visible finding joins the identity MAP, folded or not, but only an unfolded one joins the item
            // SOURCE. The two do different jobs: the map is what a row widget and the detail-panel selection are
            // reused by, so keeping a folded finding in it means expanding the group again costs no re-allocation
            // and never moves the reader's selection. The source is only what `SListView` renders.
            _FindingItemsByKey.Add(Finding.StableKey, Item);

            if (NOT IsCollapsed)
            { _FindingItems.Add(MoveTemp(Item)); }
        }
    }

    // Lines that vanished are a set change just as much as lines that appeared.
    if (Existing.Num() > 0)
    { SetChanged = true; }

    // The RENDERED sequence, compared separately from the identity map. Folding a group changes what the list draws
    // without adding or removing anything the map holds, so `SetChanged` alone cannot see it — the same reason the
    // memory list compares its row order rather than its membership, where a sort reorders without changing the set.
    auto RowOrder = TArray<FString>{};
    RowOrder.Reserve(_FindingItems.Num());

    for (const auto& Item : _FindingItems)
    {
        RowOrder.Add(Item->Key);
    }

    if (RowOrder != _FindingRowOrder)
    {
        _FindingRowOrder = MoveTemp(RowOrder);
        SetChanged = true;
    }

    if (ck::IsValid(_FindingList) && SetChanged)
    { _FindingList->RequestListRefresh(); }

    // The selected finding may have been filtered out from under the user; the detail panel says so rather than
    // keeping a stale row on screen. Tested against what the FILTER admits, not against the rows that exist — a
    // finding inside a folded group keeps its detail panel, because folding is not a statement about relevance.
    if (NOT _SelectedFindingKey.IsEmpty() && NOT VisibleFindingKeys.Contains(_SelectedFindingKey))
    { _SelectedFindingKey.Empty(); }

    DoRefresh_SelectionCommands();
    DoRebuild_FindingDetail();

    // The tray is refilled by the same rebuild that refills the list, because every one of the three things that can
    // move it — a scan pruning it, a stage, an unstage — already lands here.
    DoRefresh_QueueCommands();
    DoRebuild_FixQueue();

    if (InRefreshStatus)
    { DoRefresh_Status(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRefresh_SelectionCommands()
    -> void
{
    using namespace ck_optimization_debugger_fixes;

    const auto Selected = Get_SelectedFindings();
    const auto Fixable = Get_FixableFindings(Selected);

    _HasSelectedFinding = NOT Selected.IsEmpty();
    _SelectedFixableCount = Fixable.Num();

    // Cached here rather than derived in the button's attribute, for the reason every flag in this function is:
    // an attribute runs on the paint path, and this one would re-walk the selection to find the detailed finding.
    _OpenAssetButtonEnabled = false;

    if (_HasSelectedFinding)
    {
        const auto* DetailedForOpen = TryGet_SelectedFinding();

        _OpenAssetButtonEnabled = ck_optimization_debugger_navigation::Can_OpenAsset(
            DetailedForOpen != nullptr ? DetailedForOpen->Target : Selected[0].Target);
    }

    // Cached for the same reason every tab count on this window is: the Fix All button's label, tooltip and enabled
    // state all read it, and it paints every frame. Re-deriving it there would walk the whole findings list from a
    // paint-path attribute three times over.
    _VisibleFixableCount = Get_FixableFindings(_Model.Get_VisibleFindings()).Num();
    _FixButtonLabel = Build_FixButtonLabel(Fixable);

    // Session availability is folded in HERE rather than inside the pure projection, exactly as the cleanup page
    // does it: whether a play session is running is a fact about the editor, not a property of the selection, and a
    // spec must be able to assert the selection rule without one.
    const auto Unavailable = Get_FixesUnavailableReason();

    if (NOT Unavailable.IsEmpty())
    {
        _FixButtonEnabled = false;
        _FixButtonTooltip = Unavailable;
        return;
    }

    _FixButtonEnabled = _SelectedFixableCount > 0;

    if (_SelectedFixableCount == 0)
    {
        _FixButtonTooltip = FString{TEXT("This finding has no automatic fix — the recommendation below is the whole answer.")};
        return;
    }

    const auto Confirmation = Build_BatchConfirmation(Fixable);

    // The disabled tooltip answers "why not"; the ENABLED one has to answer "what will this cost me" before the
    // click, which is what the confirmation text says.
    _FixButtonTooltip = Confirmation.IsRequired
        ? ck::Format_UE(TEXT("{}\n\nYou will be asked to confirm."), Confirmation.Body)
        : Fixable[0].FixDescription;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRefresh_QueueCommands()
    -> void
{
    using namespace ck_optimization_debugger_fixes;

    const auto Queued = _Model.Get_QueuedFindings();
    const auto Fixable = Get_FixableFindings(Queued);

    _QueuedCount = Queued.Num();
    _QueuedFixableCount = Fixable.Num();

    _QueueApplyLabel = _QueuedFixableCount > 0
        ? ck::Format_UE(TEXT("Review && Fix {}"), _QueuedFixableCount)
        : FString{TEXT("Review && Fix")};

    if (_QueuedCount == 0)
    {
        _QueueApplyEnabled = false;
        _QueueApplyTooltip = FString{TEXT("Tick a finding to stage it here. The queue survives filtering and folding, so a batch can be assembled across several checks.")};
        return;
    }

    // Same split as the selection commands: session availability is asked here, never inside the pure projection.
    const auto Unavailable = Get_FixesUnavailableReason();

    if (NOT Unavailable.IsEmpty())
    {
        _QueueApplyEnabled = false;
        _QueueApplyTooltip = Unavailable;
        return;
    }

    _QueueApplyEnabled = _QueuedFixableCount > 0;

    if (_QueuedFixableCount == 0)
    {
        // Staging a finding with no automatic fix is legitimate — it is a shortlist of things to go and do by hand.
        // The button says why it cannot act on them rather than the tray refusing to hold them.
        _QueueApplyTooltip = FString{TEXT("Nothing in the queue has an automatic fix. These are staged for you to act on by hand.")};
        return;
    }

    const auto Confirmation = Build_BatchConfirmation(Fixable);

    _QueueApplyTooltip = Confirmation.IsRequired
        ? ck::Format_UE(TEXT("{}\n\nYou will be asked to confirm."), Confirmation.Body)
        : ck::Format_UE(TEXT("Apply {} staged fix(es) in one undoable step."), _QueuedFixableCount);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRebuild_FixQueue()
    -> void
{
    using namespace ck_optimization_debugger_fixes;

    if (ck::Is_NOT_Valid(_FixQueueBox))
    { return; }

    _FixQueueBox->ClearChildren();

    const auto Groups = _Model.Get_QueuedFindingsGroupedByCheck();

    if (Groups.IsEmpty())
    {
        _FixQueueBox->AddSlot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Nothing staged. Tick findings to build a batch.")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .AutoWrapText(true)
        ];

        return;
    }

    for (const auto& Group : Groups)
    {
        // The check's own fix VERB names the section, not the finding title: the tray answers "what is about to
        // happen", and the verb is the sentence the undo record will carry. A check with no registered fix falls
        // back to its title, because a heading reading "no fix" over rows the reader deliberately staged would be
        // the tray arguing with them.
        const auto* FixInfo = TryGet_FixInfo(Group.CheckId);

        const auto Heading = FixInfo != nullptr
            ? FixInfo->DisplayVerb
            : Group.Title;

        _FixQueueBox->AddSlot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, CkStyle::SpaceXS)
        [
            SNew(STextBlock)
            .Text(FText::FromString(ck::Format_UE(TEXT("{} · {}"), Heading, Group.Findings.Num())))
            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
        ];

        for (const auto& Finding : Group.Findings)
        {
            const auto StableKey = Finding.StableKey;

            _FixQueueBox->AddSlot()
            .AutoHeight()
            .Padding(CkStyle::SpaceL, 0.0f, CkStyle::SpaceS, CkStyle::SpaceXS)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Finding.Target.DisplayName))
                    .ToolTipText(FText::FromString(Finding.Target.Path.ToString()))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    // A CLICKABLE chip, which is legal here and would not be in a list row: the tray is a fixed
                    // panel, so there is no STableRow whose selection click this could eat — the same argument that
                    // lets the profiling page's recent rail carry clickable chips.
                    SNew(SCkDebug_Chip)
                    .Text(FText::FromString(TEXT("Remove")))
                    .Kind(ECkDebug_ChipKind::Neutral)
                    .ShowDot(false)
                    .OnClicked(FOnCkDebug_ChipClicked::CreateLambda([this, StableKey]() -> void
                    {
                        DoSet_Queued({StableKey}, false);
                    }))
                ]
            ];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRebuild_FindingDetail()
    -> void
{
    using namespace ck_optimization_debugger_window;

    if (ck::Is_NOT_Valid(_FindingDetailBox))
    { return; }

    _FindingDetailBox->ClearChildren();

    const auto* Finding = TryGet_SelectedFinding();

    if (Finding == nullptr)
    {
        _FindingDetailBox->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(_Model.Get_HasScanned()
                ? TEXT("Select a finding to see why it matters.")
                : TEXT("Run a scan to see what the open levels cost.")))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .AutoWrapText(true)
        ];

        return;
    }

    const auto AddRow = [this](const FString& InKey, const FString& InValue) -> void
    {
        _FindingDetailBox->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(InKey))
            .ValueText(FText::FromString(InValue))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(CkStyle::Text())
        ];
    };

    const auto AddProse = [this](const FString& InHeading, const FString& InBody) -> void
    {
        if (InBody.IsEmpty())
        { return; }

        _FindingDetailBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, CkStyle::SpaceXS)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(InHeading))
            .Underline(true)
        ];

        _FindingDetailBox->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(InBody))
            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
            .AutoWrapText(true)
        ];
    };

    _FindingDetailBox->AddSlot()
    .AutoHeight()
    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
    [
        SNew(SCkDebug_SectionHeader)
        .Label(FText::FromString(Finding->Title))
        .SubText(FText::FromString(ck_optimization_debugger_model::Get_CategoryLabel(Finding->Category)))
        .Underline(true)
        .RightContent()
        [
            SNew(SCkDebug_StatusPill)
            .Text(FText::FromString(ck_optimization_debugger_model::Get_SeverityLabel(Finding->Severity).ToUpper()))
            .Tone(ck_optimization_debugger_model::Get_SeverityTone(Finding->Severity))
        ]
    ];

    // The action row. Every button stays VISIBLE on every finding and merely disables when it does not apply — a
    // button that vanishes teaches nobody that the feature exists, and "why is this one not fixable" is a question
    // the disabled tooltip answers in place.
    const auto GoToTooltip = ck_optimization_debugger_navigation::Get_NavigationDescription(Finding->Target);
    const auto OpenAssetTooltip = ck_optimization_debugger_navigation::Get_OpenAssetDescription(Finding->Target);

    _FindingDetailBox->AddSlot()
    .AutoHeight()
    .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
    [
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            Build_ActionButton(ECk_Icon::Aim,
                FText::FromString(TEXT("Go To")),
                FText::FromString(GoToTooltip),
                FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnGoToClicked),
                TAttribute<bool>::CreateLambda([this]() -> bool { return _HasSelectedFinding; }))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            // A construction-time tooltip, unlike the fix button's: this one says what opening THIS finding's target
            // would do, and the panel is rebuilt whenever that target changes.
            Build_ActionButton(ECk_Icon::Edit,
                FText::FromString(TEXT("Open Asset")),
                FText::FromString(OpenAssetTooltip),
                FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnOpenAssetClicked),
                TAttribute<bool>::CreateLambda([this]() -> bool { return _OpenAssetButtonEnabled; }))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Center)
        [
            // Label, tooltip AND enabled state all read fields `DoRefresh_SelectionCommands` maintains. The tooltip
            // is an attribute rather than a construction-time string because it now carries the session gate ("not
            // while a play session is running") and the confirmation warning, neither of which is a property of the
            // finding this panel was built for.
            Build_ActionButton(ECk_Icon::Fix,
                TAttribute<FText>::CreateLambda([this]() -> FText
                {
                    return FText::FromString(_FixButtonLabel);
                }),
                TAttribute<FText>::CreateLambda([this]() -> FText
                {
                    return FText::FromString(_FixButtonTooltip);
                }),
                FOnClicked::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOnApplyFixClicked),
                TAttribute<bool>::CreateLambda([this]() -> bool { return _FixButtonEnabled; }))
        ]
    ];

    AddRow(TEXT("Check"), Finding->CheckId.ToString());
    AddRow(TEXT("Target"), Finding->Target.DisplayName);

    if (Finding->Target.Kind == ECkOptimizationDebugger_TargetKind::ProjectSettings)
    { AddRow(TEXT("Settings section"), Finding->Target.SettingsSectionName); }
    else
    { AddRow(TEXT("Path"), Finding->Target.Path.ToString()); }

    if (NOT Finding->Target.LevelName.IsEmpty())
    { AddRow(TEXT("Level"), Finding->Target.LevelName); }

    AddProse(TEXT("Why it matters"), Finding->Explanation);
    AddProse(TEXT("What to do"), Finding->Recommendation);

    if (Finding->HasAutoFix)
    { AddProse(TEXT("Automatic fix"), Finding->FixDescription); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRefresh_Status()
    -> void
{
    if (NOT _Model.Get_HasScanned())
    {
        DoSet_Status(TEXT("No scan yet."), ECk_Tone::Neutral);
        return;
    }

    const auto Counts = _Model.Get_VisibleCountsBySeverity();
    const auto Worst = _Model.TryGet_WorstVisibleSeverity();

    const auto Where = _Model.Get_ScannedLevelNames().IsEmpty()
        ? FString{TEXT("no level")}
        : FString::Join(_Model.Get_ScannedLevelNames(), TEXT(", "));

    const auto Text = ck::Format_UE(TEXT("{} finding(s) ({} critical / {} major / {} minor) — scanned {} at {}"),
        Counts.Get_Total(),
        Counts.CriticalCount,
        Counts.MajorCount,
        Counts.MinorCount,
        Where,
        _Model.Get_LastScanTime().ToString());

    // Tone follows the worst thing still on screen: a level with nothing left after filtering reads Ok, because
    // "nothing matches what you asked for" and "nothing is wrong" are the same sentence to the reader looking at
    // an empty list they themselves narrowed.
    DoSet_Status(Text, Worst.IsSet()
        ? ck_optimization_debugger_model::Get_SeverityTone(Worst.GetValue())
        : ECk_Tone::Ok);
}

// --------------------------------------------------------------------------------------------------------------------
// Findings list
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoGenerate_FindingRow(
        FFindingItem InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_optimization_debugger_window;

    const auto WeakRow = TWeakPtr<FCkOptimizationDebugger_FindingListItem>{InItem};

    if (NOT InItem.IsValid())
    {
        return SNew(STableRow<FFindingItem>, InOwnerTable)
            .Style(&Get_RowStyle());
    }

    // Row-safe widgets only — STextBlock / SBox / SCkDebug_Icon / SCkDebug_CategoryDot / SCkDebug_StatusPill and an
    // INERT chip. Anything that returns Handled on LMB-down here would make the row render but never select.
    if (InItem->IsGroupHeader)
    {
        return SNew(STableRow<FFindingItem>, InOwnerTable)
            .Style(&Get_RowStyle())
            .Padding(FMargin{0.0f, CkStyle::SpaceS, 0.0f, CkStyle::SpaceXS})
            // A header names a check, not a thing to act on — nothing about it belongs in the detail panel, so it
            // does not paint as selected either.
            .ShowSelection(false)
            .ToolTipText_Lambda([WeakRow]() -> FText
            {
                const auto Row = WeakRow.Pin();

                if (NOT Row.IsValid())
                { return FText::GetEmpty(); }

                return FText::FromString(ck::Format_UE(TEXT("{} — {} finding(s)"), Row->CheckId, Row->GroupCount));
            })
            .Content()
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceXS, 0.0f, 0.0f, 0.0f)
                [
                    // A live button INSIDE a list row, which the row-safety rule otherwise forbids — legal here for
                    // one specific reason: this header already sets `ShowSelection(false)`, so there is no selection
                    // click for it to eat. It handles LMB only, leaving the right-click that opens the list's
                    // context menu to pass through untouched.
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
                    .ContentPadding(FMargin{CkStyle::SpaceXS, 0.0f})
                    .ToolTipText(FText::FromString(TEXT("Fold this check. Folding hides rows — it changes no count and filters nothing.")))
                    .OnClicked_Lambda([this, WeakRow]() -> FReply
                    {
                        const auto Row = WeakRow.Pin();

                        if (NOT Row.IsValid())
                        { return FReply::Handled(); }

                        DoSet_CheckCollapsed(Row->CheckId, NOT Row->IsCollapsed);
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(InItem->IsCollapsed ? TEXT("▸") : TEXT("▾")))
                        .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    // Tri-state: ticked when the whole group is staged, undetermined when part of it is. Clicking
                    // stages or unstages the WHOLE group, which is what the two-state reading of a partial group
                    // has to resolve to — and unstaging is the safer of the two when the reader has already
                    // gathered some of it.
                    SNew(SCheckBox)
                    .ToolTipText(FText::FromString(TEXT("Stage every finding in this check for the fix queue")))
                    .IsChecked_Lambda([WeakRow]() -> ECheckBoxState
                    {
                        const auto Row = WeakRow.Pin();

                        if (NOT Row.IsValid() || Row->GroupCount == 0)
                        { return ECheckBoxState::Unchecked; }

                        if (Row->GroupQueuedCount == 0)
                        { return ECheckBoxState::Unchecked; }

                        return Row->GroupQueuedCount == Row->GroupCount
                            ? ECheckBoxState::Checked
                            : ECheckBoxState::Undetermined;
                    })
                    .OnCheckStateChanged_Lambda([this, WeakRow](ECheckBoxState InState) -> void
                    {
                        const auto Row = WeakRow.Pin();

                        if (NOT Row.IsValid())
                        { return; }

                        // Driven off what the group ALREADY is rather than the state Slate hands in: a tri-state box
                        // reports `Checked` when leaving Undetermined, so trusting it would make a half-staged group
                        // stage the rest when the reader's gesture on a partial group reads as "clear this".
                        const auto ShouldQueue = Row->GroupQueuedCount < Row->GroupCount;

                        DoSet_Queued(Row->GroupStableKeys, ShouldQueue);
                    })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_CategoryDot)
                    .Color(Get_CategoryColor(InItem->Category))
                    .Diameter(k_CategoryDotSize)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(Get_IconBrush(Get_CategoryIcon(InItem->Category)))
                    .Meaning(FText::FromString(
                        ck_optimization_debugger_model::Get_CategoryLabel(InItem->Category)))
                    .ColorAndOpacity(FSlateColor{Get_CategoryColor(InItem->Category)})
                    .Size(FVector2D{k_RowIconSize, k_RowIconSize})
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(InItem->Title))
                    .Font(CkStyle::BoldFont(CkStyle::FontSizeBody()))
                    .ColorAndOpacity(FSlateColor{CkStyle::GetToneColor(
                        ck_optimization_debugger_model::Get_SeverityTone(InItem->Severity))})
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    // What the group is holding staged, shown on the HEADER so folding a group never hides the fact
                    // that there is work gathered inside it. Inert — the checkbox to its left is the control.
                    SNew(SCkDebug_Chip)
                    .Text(FText::FromString(ck::Format_UE(TEXT("{} staged"), InItem->GroupQueuedCount)))
                    .Kind(ECkDebug_ChipKind::Effect)
                    .ShowDot(false)
                    .Visibility(InItem->GroupQueuedCount > 0 ? EVisibility::Visible : EVisibility::Collapsed)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    // The group's worst severity, drawn on the header for the same reason: a folded group must
                    // still say how bad what it hides is, or folding would be a way to make a Critical disappear.
                    SNew(SCkDebug_Icon)
                    .Brush(Get_IconBrush(Get_SeverityIcon(InItem->Severity)))
                    .Meaning(FText::FromString(
                        ck_optimization_debugger_model::Get_SeverityLabel(InItem->Severity)))
                    .ColorAndOpacity(FSlateColor{CkStyle::GetToneColor(
                        ck_optimization_debugger_model::Get_SeverityTone(InItem->Severity))})
                    .Size(FVector2D{k_RowIconSize, k_RowIconSize})
                    .Visibility(InItem->IsCollapsed ? EVisibility::Visible : EVisibility::Collapsed)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(ck::Format_UE(TEXT("{}"), InItem->GroupCount)))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ];
    }

    const auto HighlightText = TAttribute<FText>::CreateLambda([this]() -> FText
    {
        return FText::FromString(_Model.Get_Filter().HighlightString);
    });

    return SNew(STableRow<FFindingItem>, InOwnerTable)
        .Style(&Get_RowStyle())
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        .ToolTipText_Lambda([WeakRow]() -> FText
        {
            const auto Row = WeakRow.Pin();

            if (NOT Row.IsValid())
            { return FText::GetEmpty(); }

            return FText::FromString(Row->Finding.Explanation);
        })
        .Content()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(k_RowIndent, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                // The ONE interactive widget the row-safety rule admits into a finding row, and the exception is
                // narrow on purpose: a checkbox consumes only the click that lands inside its own bounds, so the
                // rest of the row still selects and still opens the context menu. It is what makes staging
                // INDEPENDENT of selection — the reader can gather a batch across several groups and several filter
                // changes without the list's own selection, which any click replaces, being the thing that holds it.
                SNew(SCheckBox)
                .ToolTipText(FText::FromString(TEXT("Stage this finding for the fix queue")))
                .IsChecked_Lambda([WeakRow]() -> ECheckBoxState
                {
                    const auto Row = WeakRow.Pin();

                    return Row.IsValid() && Row->IsQueued
                        ? ECheckBoxState::Checked
                        : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([this, WeakRow](ECheckBoxState InState) -> void
                {
                    const auto Row = WeakRow.Pin();

                    if (NOT Row.IsValid())
                    { return; }

                    DoSet_Queued({Row->Finding.StableKey}, InState == ECheckBoxState::Checked);
                })
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_StatusPill)
                .Text(FText::FromString(
                    ck_optimization_debugger_model::Get_SeverityLabel(InItem->Severity).ToUpper()))
                .Tone(ck_optimization_debugger_model::Get_SeverityTone(InItem->Severity))
                .ShowDot(false)
            ]

            // How far past its budget, when the finding has one. Inert, and hidden entirely rather than printing a
            // placeholder: most checks fire on a condition rather than a measurement, and a badge on those would be
            // claiming a number they never computed.
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Chip)
                .Text(FText::FromString(
                    ck_optimization_debugger_model::Format_BudgetRatio(InItem->Finding.BudgetRatio)))
                .Kind(ECkDebug_ChipKind::Neutral)
                .ShowDot(false)
                .Visibility(InItem->Finding.BudgetRatio > 0.0f ? EVisibility::Visible : EVisibility::Collapsed)
            ]

            // A muted row only ever reaches the list while "Show muted" is on, and it MUST say so on the row itself.
            // Without the mark, the reader is looking at a list that silently mixes "this is a live finding" with
            // "you told me to ignore this", which is the one confusion muting must never create. An inert chip —
            // an OnClicked here would eat the click that selects the row.
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Chip)
                .Text(FText::FromString(TEXT("MUTED")))
                .Kind(ECkDebug_ChipKind::Neutral)
                .ShowDot(false)
                .Visibility_Lambda([this, WeakRow]() -> EVisibility
                {
                    const auto Row = WeakRow.Pin();

                    if (NOT Row.IsValid())
                    { return EVisibility::Collapsed; }

                    return _Model.Get_IsMuted(Row->Finding.StableKey)
                        ? EVisibility::HitTestInvisible
                        : EVisibility::Collapsed;
                })
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(InItem->Finding.Target.DisplayName))
                .HighlightText(HighlightText)
                .ColorAndOpacity_Lambda([WeakRow]() -> FSlateColor
                {
                    const auto Row = WeakRow.Pin();
                    return Get_RowTextColor(Row.IsValid() ? Row->IsHighlightMatch : true);
                })
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(InItem->Finding.Target.LevelName.IsEmpty()
                    ? InItem->Finding.Target.Path.ToString()
                    : InItem->Finding.Target.LevelName))
                .HighlightText(HighlightText)
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(SBox)
                .Visibility(InItem->Finding.HasAutoFix ? EVisibility::Visible : EVisibility::Collapsed)
                [
                    // Fully inert on purpose. A bound chip consumes left-click and the row would stop selecting;
                    // a chip with CopyText consumes RIGHT-click and would punch a hole in the list's own copy
                    // menu. The action itself lands in the fixes phase, on a command, not inside the row.
                    SNew(SCkDebug_Chip)
                    .Text(FText::FromString(TEXT("FIX")))
                    .Kind(ECkDebug_ChipKind::Effect)
                    .ShowDot(false)
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnFindingSelectionChanged(
        FFindingItem InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    // `ESelectInfo::Direct` is the whole guard. There was a `_SuppressSelectionEcho` flag beside it that nothing
    // ever set — the window restores selection by KEY through the row-identity map rather than by re-selecting
    // through the widget, so there is no programmatic selection for an echo flag to suppress.
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    // Refreshed BEFORE the header early-out: a multi-selection that ends on a group header still changed how many
    // fixable findings are selected, and the Apply button has to say so.
    DoRefresh_SelectionCommands();

    if (NOT InItem.IsValid() || InItem->IsGroupHeader)
    { return; }

    _SelectedFindingKey = InItem->Key;
    DoRebuild_FindingDetail();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnFindingDoubleClicked(
        FFindingItem InItem)
    -> void
{
    if (NOT InItem.IsValid())
    { return; }

    // On a HEADER the same gesture folds the group — the affordance every tree in the editor already carries, so
    // the chevron is the discoverable control rather than the only one.
    if (InItem->IsGroupHeader)
    {
        DoSet_CheckCollapsed(InItem->CheckId, NOT InItem->IsCollapsed);
        return;
    }

    // Double-click is the discoverable twin of the Go To button — the gesture the reader already uses in every
    // other list in the editor to open the thing a row names.
    DoNavigate_To(InItem->Finding);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoToggle_MuteOnSelected(
        bool InMute)
    -> void
{
    if (ck::Is_NOT_Valid(_FindingList))
    { return; }

    auto ChangedCount = 0;

    for (const auto& Item : _FindingList->GetSelectedItems())
    {
        // A group header names a check, not a finding, and has no stable key to mute by. Muting a whole check is a
        // different feature with a different persistence shape; silently muting its rows here would make one click
        // do something the label never said.
        if (NOT Item.IsValid() || Item->IsGroupHeader)
        { continue; }

        if (_Model.Get_IsMuted(Item->Finding.StableKey) == InMute)
        { continue; }

        _Model.Set_Muted(Item->Finding.StableKey, InMute);
        ++ChangedCount;
    }

    if (ChangedCount == 0)
    { return; }

    // Persisted immediately, exactly as a level-exclusion toggle is: a triage decision lost to a crash is one the
    // reader has to make again, and they have no way to know they lost it.
    UCkOptimizationDebuggerSettings::Save_MutedStableKeys(_Model.Get_MutedStableKeys());

    DoRebuild_Findings();

    DoSet_Status(ck::Format_UE(
        TEXT("{} finding(s) {} — {} muted in this scan"),
        ChangedCount,
        InMute ? TEXT("muted") : TEXT("unmuted"),
        _Model.Get_MutedFindingCount()),
        ECk_Tone::Info);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoSet_CheckCollapsed(
        FName InCheckId,
        bool InCollapsed)
    -> void
{
    if (_Model.Get_IsCheckCollapsed(InCheckId) == InCollapsed)
    { return; }

    _Model.Set_CheckCollapsed(InCheckId, InCollapsed);

    UCkOptimizationDebuggerSettings::Save_CollapsedCheckIds(_Model.Get_CollapsedCheckIds());

    // No status line. Folding a section is not news — it changes no count and no answer, and a strip that narrated
    // it would overwrite whatever the last scan or fix had to say for a gesture the reader can see the result of.
    DoRebuild_Findings(false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoSet_AllChecksCollapsed(
        bool InCollapsed)
    -> void
{
    _Model.Set_AllChecksCollapsed(InCollapsed);

    UCkOptimizationDebuggerSettings::Save_CollapsedCheckIds(_Model.Get_CollapsedCheckIds());

    DoRebuild_Findings(false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoSet_Queued(
        const TArray<FString>& InStableKeys,
        bool InQueued)
    -> void
{
    if (InStableKeys.IsEmpty())
    { return; }

    _Model.Set_QueuedForKeys(InStableKeys, InQueued);

    // Deliberately NOT persisted, unlike muting and collapsing. A queue is work in progress — a batch restored
    // across an editor restart would be a set of fixes the reader had forgotten they staged, waiting behind one
    // button press.
    DoRebuild_Findings(false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoToggle_QueueOnSelected(
        bool InQueued)
    -> void
{
    if (ck::Is_NOT_Valid(_FindingList))
    { return; }

    auto Keys = TArray<FString>{};

    for (const auto& Item : _FindingList->GetSelectedItems())
    {
        if (NOT Item.IsValid())
        { continue; }

        // A selected group HEADER stages its whole group. Unlike muting — which refuses a header because muting a
        // check is a different feature with a different persistence shape — staging every finding under a header is
        // exactly what the header's own checkbox does, so the two gestures agree rather than one silently doing
        // nothing.
        if (Item->IsGroupHeader)
        {
            Keys.Append(Item->GroupStableKeys);
            continue;
        }

        Keys.Add(Item->Finding.StableKey);
    }

    DoSet_Queued(Keys, InQueued);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoClear_Queue()
    -> void
{
    if (_Model.Get_QueuedFindingCount() == 0)
    { return; }

    _Model.Clear_Queue();

    DoRebuild_Findings(false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoApply_Queue()
    -> void
{
    // Routed through the SAME pipeline the selection apply uses, so the session gate, the confirmation dialog, the
    // single-vs-batch split and the post-fix re-scan are shared. A second apply path is a second place for the
    // confirmation rules to drift, and the confirmation is the whole safety story for a destructive fix.
    DoApply_Fixes(_Model.Get_QueuedFindings(), TEXT("the fix queue"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnFindingContextMenu()
    -> TSharedPtr<SWidget>
{
    using namespace ck_optimization_debugger_window;

    if (ck::Is_NOT_Valid(_FindingList))
    { return nullptr; }

    const auto Selected = _FindingList->GetSelectedItems();

    auto Titles = TArray<FString>{};
    auto Paths = TArray<FString>{};
    auto Summaries = TArray<FString>{};
    auto Findings = TArray<FCkOptimizationDebugger_FindingRow>{};
    auto Headers = TArray<FFindingItem>{};

    for (const auto& Item : Selected)
    {
        if (NOT Item.IsValid())
        { continue; }

        if (Item->IsGroupHeader)
        {
            Headers.Add(Item);
            continue;
        }

        Findings.Add(Item->Finding);
        Titles.Add(Item->Finding.Title);
        Paths.Add(Item->Finding.Target.Kind == ECkOptimizationDebugger_TargetKind::ProjectSettings
            ? Item->Finding.Target.SettingsSectionName
            : Item->Finding.Target.Path.ToString());
        Summaries.Add(Build_FindingSummary(Item->Finding));
    }

    // A header-only right-click gets the GROUP menu. This is where the batch verbs live rather than as buttons in
    // the header itself: the row-safety rule admits the chevron and the checkbox because each consumes only its own
    // bounds, and a row of action buttons would start eating the clicks the list needs.
    if (Summaries.IsEmpty() && NOT Headers.IsEmpty())
    { return DoBuild_GroupContextMenu(Headers); }

    if (Summaries.IsEmpty())
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};

    // The actions come first and the copy entries after: the reader who right-clicked a finding is far more often
    // going to it than quoting it.
    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Go To")),
        FText::FromString(TEXT("Show what this finding is about — the asset in the Content Browser, the actor in the level viewport, or its Project Settings page")),
        FSlateIcon{},
        FUIAction{FExecuteAction::CreateSP(this, &SCkOptimizationDebuggerWindow::DoNavigate_ToSelected)});

    // Single selection only, and present only when the target has an editor to open — the menu's own rule. One click
    // opening N asset editors is not what the reader asked for, and it is not undoable by closing one window.
    if (Findings.Num() == 1 && ck_optimization_debugger_navigation::Can_OpenAsset(Findings[0].Target))
    {
        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Open Asset")),
            FText::FromString(ck_optimization_debugger_navigation::Get_OpenAssetDescription(Findings[0].Target)),
            FSlateIcon{},
            FUIAction{FExecuteAction::CreateSP(this, &SCkOptimizationDebuggerWindow::DoOpenAsset_Selected)});
    }

    const auto Fixable = ck_optimization_debugger_fixes::Get_FixableFindings(Findings);

    // Present only when it would actually do something — including the session gate, the same way the cleanup
    // context menu hides its action rather than offering a greyed line the reader reads once and never again.
    if (NOT Fixable.IsEmpty() && ck_optimization_debugger_fixes::Get_CanApplyFixes())
    {
        const auto Confirmation = ck_optimization_debugger_fixes::Build_BatchConfirmation(Fixable);

        MenuBuilder.AddMenuEntry(
            FText::FromString(ck::Format_UE(TEXT("Apply Fix ({})"), Fixable.Num())),
            FText::FromString(Confirmation.IsRequired
                ? ck::Format_UE(TEXT("{}\n\nYou will be asked to confirm."), Confirmation.Body)
                : FString{TEXT("Apply every automatic fix in the selection, inside one transaction Undo can reverse")}),
            FSlateIcon{},
            FUIAction{FExecuteAction::CreateSP(this, &SCkOptimizationDebuggerWindow::DoApply_FixToSelected)});
    }

    // Staging, offered on the same "the verb follows the whole selection" rule as muting below. It is here as well
    // as on the row checkbox because a rubber-banded selection is the fastest way to gather a batch, and reaching
    // for twenty checkboxes to do what one menu entry does would make the tray feel like a tax.
    {
        auto QueuedCount = 0;

        for (const auto& Finding : Findings)
        {
            if (_Model.Get_IsQueued(Finding.StableKey))
            { ++QueuedCount; }
        }

        const auto UnstageInstead = QueuedCount == Findings.Num();

        MenuBuilder.AddMenuEntry(
            FText::FromString(UnstageInstead
                ? ck::Format_UE(TEXT("Unstage ({})"), Findings.Num())
                : ck::Format_UE(TEXT("Stage ({})"), Findings.Num())),
            FText::FromString(UnstageInstead
                ? FString{TEXT("Remove these findings from the fix queue.")}
                : FString{TEXT("Add these findings to the fix queue. Staging survives filtering, folding and re-scans, so a batch can be gathered across several checks before anything is applied.")}),
            FSlateIcon{},
            FUIAction{FExecuteAction::CreateSP(this,
                &SCkOptimizationDebuggerWindow::DoToggle_QueueOnSelected, NOT UnstageInstead)});
    }

    // Mute is offered as ONE entry over the whole selection, and its verb is decided by what the selection already
    // is: all-muted unmutes, anything else mutes. A per-row toggle in a multi-select menu would leave a mixed
    // selection in a state nobody could predict from the label they clicked.
    {
        auto MutedCount = 0;

        for (const auto& Finding : Findings)
        {
            if (_Model.Get_IsMuted(Finding.StableKey))
            { ++MutedCount; }
        }

        const auto UnmuteInstead = MutedCount == Findings.Num();

        MenuBuilder.AddMenuEntry(
            FText::FromString(UnmuteInstead
                ? ck::Format_UE(TEXT("Unmute ({})"), Findings.Num())
                : ck::Format_UE(TEXT("Mute ({})"), Findings.Num())),
            FText::FromString(UnmuteInstead
                ? FString{TEXT("Show these findings in the list again.")}
                : FString{TEXT("Hide these findings from the list until you unmute them. They stay muted across ")
                    TEXT("re-scans and editor restarts, and the 'Show muted' toggle above the list always says how ")
                    TEXT("many this scan is hiding — nothing is deleted and nothing is fixed.")}),
            FSlateIcon{},
            FUIAction{FExecuteAction::CreateSP(this,
                &SCkOptimizationDebuggerWindow::DoToggle_MuteOnSelected, NOT UnmuteInstead)});
    }

    MenuBuilder.AddMenuSeparator();

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Title")),
        FText::FromString(TEXT("Copy the selected findings' titles, one per line")),
        FString::Join(Titles, TEXT("\n")));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Target Path")),
        FText::FromString(TEXT("Copy the path of what each selected finding is about, one per line")),
        FString::Join(Paths, TEXT("\n")));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Summary")),
        FText::FromString(TEXT("Copy severity, title, target and recommendation for each selected finding")),
        FString::Join(Summaries, TEXT("\n")));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoBuild_GroupContextMenu(
        const TArray<FFindingItem>& InHeaders)
    -> TSharedPtr<SWidget>
{
    using namespace ck_optimization_debugger_window;

    if (InHeaders.IsEmpty())
    { return nullptr; }

    auto Keys = TArray<FString>{};
    auto Findings = TArray<FCkOptimizationDebugger_FindingRow>{};
    auto QueuedCount = 0;

    for (const auto& Header : InHeaders)
    {
        if (NOT Header.IsValid())
        { continue; }

        for (const auto& Key : Header->GroupStableKeys)
        {
            Keys.Add(Key);

            if (_Model.Get_IsQueued(Key))
            { ++QueuedCount; }

            if (const auto* Item = _FindingItemsByKey.Find(Key); Item != nullptr && Item->IsValid())
            { Findings.Add((*Item)->Finding); }
        }
    }

    if (Keys.IsEmpty())
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};

    // The verb follows what the whole selection ALREADY is, exactly as the finding menu's mute entry does: fully
    // staged offers to clear, anything else offers to stage.
    const auto UnstageInstead = QueuedCount == Keys.Num();

    MenuBuilder.AddMenuEntry(
        FText::FromString(UnstageInstead
            ? ck::Format_UE(TEXT("Unstage ({})"), Keys.Num())
            : ck::Format_UE(TEXT("Stage ({})"), Keys.Num())),
        FText::FromString(UnstageInstead
            ? FString{TEXT("Remove every finding in this check from the fix queue.")}
            : FString{TEXT("Add every finding in this check to the fix queue. Staging survives filtering, folding and re-scans.")}),
        FSlateIcon{},
        FUIAction{FExecuteAction::CreateLambda([this, Keys, UnstageInstead]() -> void
        {
            DoSet_Queued(Keys, NOT UnstageInstead);
        })});

    const auto Fixable = ck_optimization_debugger_fixes::Get_FixableFindings(Findings);

    // Same gate the finding menu uses: present only when it would do something, session included, rather than a
    // greyed line the reader learns to skip.
    if (NOT Fixable.IsEmpty() && ck_optimization_debugger_fixes::Get_CanApplyFixes())
    {
        const auto Confirmation = ck_optimization_debugger_fixes::Build_BatchConfirmation(Fixable);

        MenuBuilder.AddMenuEntry(
            FText::FromString(ck::Format_UE(TEXT("Fix All In Group ({})"), Fixable.Num())),
            FText::FromString(Confirmation.IsRequired
                ? ck::Format_UE(TEXT("{}\n\nYou will be asked to confirm."), Confirmation.Body)
                : FString{TEXT("Apply this check's fix to every finding under it, inside one transaction Undo can reverse")}),
            FSlateIcon{},
            FUIAction{FExecuteAction::CreateLambda([this, Fixable]() -> void
            {
                DoApply_Fixes(Fixable, TEXT("this check"));
            })});
    }

    MenuBuilder.AddMenuSeparator();

    // Solo ASSIGNS rather than toggles, the same rule `Set_SeveritySolo` holds to: a second press on the control
    // that means "show me only this" must leave that answer standing rather than emptying the list.
    if (InHeaders.Num() == 1 && InHeaders[0].IsValid())
    {
        const auto CheckId = InHeaders[0]->CheckId;
        const auto Category = InHeaders[0]->Category;

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Fold Every Other Check")),
            FText::FromString(TEXT("Collapse every group except this one. Folding changes no count — it is a way to read the list, not to narrow it.")),
            FSlateIcon{},
            FUIAction{FExecuteAction::CreateLambda([this, CheckId]() -> void
            {
                _Model.Set_AllChecksCollapsed(true);
                _Model.Set_CheckCollapsed(CheckId, false);

                UCkOptimizationDebuggerSettings::Save_CollapsedCheckIds(_Model.Get_CollapsedCheckIds());

                DoRebuild_Findings(false);
            })});

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Show Only This Category")),
            FText::FromString(TEXT("Narrow the list to this check's category. This IS a filter — the counts move with it.")),
            FSlateIcon{},
            FUIAction{FExecuteAction::CreateLambda([this, Category]() -> void
            {
                for (const auto Each : ck_optimization_debugger_model::Get_AllCategories())
                {
                    _Model.Set_CategoryVisible(Each, Each == Category);
                }

                DoRebuild_Findings();
            })});
    }

    MenuBuilder.AddMenuSeparator();

    {
        auto MutedCount = 0;

        for (const auto& Key : Keys)
        {
            if (_Model.Get_IsMuted(Key))
            { ++MutedCount; }
        }

        const auto UnmuteInstead = MutedCount == Keys.Num();

        MenuBuilder.AddMenuEntry(
            FText::FromString(UnmuteInstead
                ? ck::Format_UE(TEXT("Unmute Group ({})"), Keys.Num())
                : ck::Format_UE(TEXT("Mute Group ({})"), Keys.Num())),
            FText::FromString(UnmuteInstead
                ? FString{TEXT("Show this check's findings in the list again.")}
                : FString{TEXT("Hide this check's findings until you unmute them. They stay muted across re-scans and editor restarts, and the 'Show muted' toggle always says how many this scan is hiding.")}),
            FSlateIcon{},
            FUIAction{FExecuteAction::CreateLambda([this, Keys, UnmuteInstead]() -> void
            {
                for (const auto& Key : Keys)
                {
                    _Model.Set_Muted(Key, NOT UnmuteInstead);
                }

                UCkOptimizationDebuggerSettings::Save_MutedStableKeys(_Model.Get_MutedStableKeys());

                DoRebuild_Findings();
            })});
    }

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    TryGet_SelectedFinding() const
    -> const FCkOptimizationDebugger_FindingRow*
{
    if (_SelectedFindingKey.IsEmpty())
    { return nullptr; }

    const auto* Item = _FindingItemsByKey.Find(_SelectedFindingKey);

    if (Item == nullptr || NOT Item->IsValid() || (*Item)->IsGroupHeader)
    { return nullptr; }

    return &(*Item)->Finding;
}

// --------------------------------------------------------------------------------------------------------------------
// Memory analyzer
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnMemoryRefreshClicked()
    -> FReply
{
    DoRun_MemoryScan();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRun_MemoryScan()
    -> void
{
    using namespace ck_optimization_debugger_model;

    auto Result = ck_optimization_debugger_memory::Run_MemoryScan();

    const auto Availability = Result.StreamingAvailability;

    // Row identity is reused between FILTER and SORT passes, dropped between SCANS — the same rule the findings list
    // holds to. A re-scan can reproduce a path with different numbers behind it, and the row widgets built for the
    // old numbers are only rebuilt when the order changes; clearing the order too makes the next rebuild refresh
    // unconditionally, which is what a fresh answer is.
    _MemoryItems.Reset();
    _MemoryItemsByPath.Reset();
    _MemoryRowOrder.Reset();

    _Model.Set_MemoryRows(MoveTemp(Result.Rows), Availability);

    DoRebuild_Memory();

    const auto Note = Get_StreamingAvailabilityNote(Availability);

    DoSet_Status(ck::Format_UE(TEXT("{} resident asset(s) — {} resource, {} GPU. {}"),
        _MemoryTotals.RowCount,
        Format_ByteSize(_MemoryTotals.ResourceSizeBytes),
        _MemoryTotals.SeparableGpuRowCount > 0 ? Format_ByteSize(_MemoryTotals.GpuSizeBytes) : FString{TEXT("—")},
        Note),
        // A census taken without streaming metrics is a partial answer, and the reader has to know that BEFORE they
        // read the mip column rather than after wondering why it is empty.
        Note.IsEmpty() ? ECk_Tone::Ok : ECk_Tone::Warn);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRebuild_Memory()
    -> void
{
    using namespace ck_optimization_debugger_model;

    const auto Rows = _Model.Get_SortedMemoryRows(_ActiveMemoryTable, _MemorySortColumn, _MemorySortAscending);

    // Cached so the header tiles read a field instead of re-summing every resident object on the paint path.
    _MemoryTotals = _Model.Get_MemoryTotals();

    // ALL THREE tables, not just the active one: the sub-table selector shows a count per tab, and the page tab
    // shows the active table's. Three walks per rebuild replaces three walks per FRAME.
    const auto AllTables = Get_AllMemoryTables();

    // Sized from the largest enum VALUE rather than from the count, because the array is indexed by the value. The
    // two agree today; sizing by count would turn a future gap in the enum into a silently dropped tab count.
    auto MaxTableIndex = -1;

    for (const auto Table : AllTables)
    { MaxTableIndex = FMath::Max(MaxTableIndex, static_cast<int32>(Table)); }

    _VisibleMemoryCountByTable.Reset();
    _VisibleMemoryCountByTable.AddZeroed(MaxTableIndex + 1);

    for (const auto Table : AllTables)
    {
        const auto Index = static_cast<int32>(Table);

        if (_VisibleMemoryCountByTable.IsValidIndex(Index))
        { _VisibleMemoryCountByTable[Index] = _Model.Get_VisibleMemoryRowCount(Table); }
    }

    _MemoryLargestRowBytes = 0;

    for (const auto& Row : Rows)
    { _MemoryLargestRowBytes = FMath::Max(_MemoryLargestRowBytes, Row.ResourceSizeBytes); }

    // Stable row identity, keyed by asset path: reuse the TSharedPtr whose key matches, allocate only for genuinely
    // new rows. Resetting and re-allocating per rebuild would destroy the selection every keystroke in the filter.
    auto Existing = MoveTemp(_MemoryItemsByPath);
    _MemoryItemsByPath.Reset();
    _MemoryItems.Reset();

    auto NewOrder = TArray<FString>{};
    NewOrder.Reserve(Rows.Num());

    for (const auto& Row : Rows)
    {
        auto Item = FMemoryItem{};

        if (auto* Found = Existing.Find(Row.AssetPath))
        {
            Item = *Found;
            Existing.Remove(Row.AssetPath);
        }
        else
        {
            Item = MakeShared<FCkOptimizationDebugger_MemoryRow>();
        }

        *Item = Row;

        _MemoryItemsByPath.Add(Row.AssetPath, Item);
        NewOrder.Add(Row.AssetPath);
        _MemoryItems.Add(MoveTemp(Item));
    }

    // The ORDER, not just the membership: a sort reverses the list without adding or removing a single row, and
    // `SListView` renders in item-source order — comparing sets alone would leave a sorted table looking unsorted.
    const auto OrderChanged = NewOrder != _MemoryRowOrder;

    _MemoryRowOrder = MoveTemp(NewOrder);

    if (ck::IsValid(_MemoryList) && OrderChanged)
    { _MemoryList->RequestListRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoSelect_MemoryTable(
        ECkOptimizationDebugger_MemoryTable InTable)
    -> void
{
    if (_ActiveMemoryTable == InTable)
    { return; }

    // The sort column and direction deliberately SURVIVE a table switch: "show me the biggest first" is a question
    // about all three tables, and re-asking it per table would be the tool forgetting what the reader wanted.
    _ActiveMemoryTable = InTable;

    DoRebuild_Memory();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnMemorySortChanged(
        EColumnSortPriority::Type InPriority,
        const FName& InColumnId,
        EColumnSortMode::Type InSortMode)
    -> void
{
    // One sort key, so the priority Slate offers for multi-column sorting is not a state this window keeps.
    (void)InPriority;

    const auto Column = ck_optimization_debugger_model::TryGet_MemoryColumnFromId(InColumnId);

    if (NOT Column.IsSet())
    { return; }

    _MemorySortColumn = Column.GetValue();

    // Slate hands in the mode the header just moved to, including `None` on a column that was not sorted — treating
    // anything that is not explicitly Descending as ascending keeps the two states the header can actually paint.
    _MemorySortAscending = InSortMode != EColumnSortMode::Descending;

    DoRebuild_Memory();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    Get_MemorySortMode(
        ECkOptimizationDebugger_MemoryColumn InColumn) const
    -> EColumnSortMode::Type
{
    if (InColumn != _MemorySortColumn)
    { return EColumnSortMode::None; }

    return _MemorySortAscending ? EColumnSortMode::Ascending : EColumnSortMode::Descending;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoGenerate_MemoryRow(
        FMemoryItem InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_optimization_debugger_window;

    if (NOT InItem.IsValid())
    {
        return SNew(STableRow<FMemoryItem>, InOwnerTable)
            .Style(&Get_RowStyle());
    }

    // BOUND, not read: a filter pass reuses the row widgets whose asset path survived it, so a denominator captured
    // here would outlive the filter that moved it.
    return SNew(SCkOptimizationDebugger_MemoryTableRow, InOwnerTable)
        .Row(InItem)
        .LargestSizeBytes(TAttribute<int64>::CreateLambda([this]() -> int64
        {
            return _MemoryLargestRowBytes;
        }));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnMemoryContextMenu()
    -> TSharedPtr<SWidget>
{
    using namespace ck_optimization_debugger_window;

    if (ck::Is_NOT_Valid(_MemoryList))
    { return nullptr; }

    const auto Selected = _MemoryList->GetSelectedItems();

    auto Names = TArray<FString>{};
    auto Paths = TArray<FString>{};
    auto Summaries = TArray<FString>{};

    for (const auto& Item : Selected)
    {
        if (NOT Item.IsValid())
        { continue; }

        Names.Add(Item->DisplayName);
        Paths.Add(Item->AssetPath);
        Summaries.Add(Build_MemoryRowSummary(*Item));
    }

    if (Summaries.IsEmpty())
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};

    // The action first, the quotes after — the same ordering the findings menu uses, for the same reason.
    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Show in Content Browser")),
        FText::FromString(TEXT("Sync the Content Browser to this asset — the same thing double-clicking the row does")),
        FSlateIcon{},
        FUIAction{FExecuteAction::CreateSP(this,
            &SCkOptimizationDebuggerWindow::DoOnMemoryDoubleClicked, Selected[0])});

    // Same target the row already navigates by, same single-selection rule as the findings menu.
    if (Selected.Num() == 1 && Selected[0].IsValid())
    {
        const auto MemoryTarget = Build_MemoryTarget(*Selected[0]);

        if (ck_optimization_debugger_navigation::Can_OpenAsset(MemoryTarget))
        {
            MenuBuilder.AddMenuEntry(
                FText::FromString(TEXT("Open Asset")),
                FText::FromString(ck_optimization_debugger_navigation::Get_OpenAssetDescription(MemoryTarget)),
                FSlateIcon{},
                FUIAction{FExecuteAction::CreateSP(this,
                    &SCkOptimizationDebuggerWindow::DoOpenAsset_ForTarget, MemoryTarget)});
        }
    }

    MenuBuilder.AddMenuSeparator();

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Name")),
        FText::FromString(TEXT("Copy the selected assets' names, one per line")),
        FString::Join(Names, TEXT("\n")));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Path")),
        FText::FromString(TEXT("Copy the selected assets' object paths, one per line")),
        FString::Join(Paths, TEXT("\n")));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Summary")),
        FText::FromString(TEXT("Copy name, path, format, dimensions and sizes for each selected asset")),
        FString::Join(Summaries, TEXT("\n")));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnMemoryDoubleClicked(
        FMemoryItem InItem)
    -> void
{
    using namespace ck_optimization_debugger_window;

    if (NOT InItem.IsValid())
    { return; }

    // The SAME navigation an asset-targeted finding takes. A memory row names an asset; there is one way to show the
    // reader an asset, and it is not a second copy of this logic.
    const auto Result = ck_optimization_debugger_navigation::Navigate_ToTarget(Build_MemoryTarget(*InItem));

    DoSet_Status(Result.Message, Result.Succeeded ? ECk_Tone::Ok : ECk_Tone::Warn);
}

// --------------------------------------------------------------------------------------------------------------------
// Project cleanup
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnCleanupScanClicked()
    -> FReply
{
    DoRun_CleanupScan();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRun_CleanupScan()
    -> void
{
    using namespace ck_optimization_debugger_model;

    auto Result = ck_optimization_debugger_cleanup::Run_CleanupScan();

    if (Result.RequiresEditor)
    {
        DoSet_Status(TEXT("Reviewing project content needs an editor session — nothing to scan here."),
            ECk_Tone::Neutral);
        return;
    }

    // Row identity is reused between FILTER and CATEGORY passes, dropped between SCANS — the same rule the findings
    // and memory lists hold to. A re-scan can reproduce a path with a different reason behind it.
    _CleanupItems.Reset();
    _CleanupItemsByKey.Reset();
    _CleanupRowOrder.Reset();

    _Model.Set_CleanupRows(MoveTemp(Result.Rows), FDateTime::Now());

    DoRebuild_Cleanup();

    const auto& Totals = _CleanupTotals;

    const auto Counted = [&Totals](ECkOptimizationDebugger_CleanupCategory InCategory) -> int32
    {
        const auto Index = static_cast<int32>(InCategory);
        return Totals.Categories.IsValidIndex(Index) ? Totals.Categories[Index].RowCount : 0;
    };

    // Three different statements about script/config references, and the reader has to get the right one. "Nobody was
    // there to ask" is NOT "asked and found none": with no provider registered, the unreferenced count silently
    // excludes nothing, and reporting it bare would present a project this pass could not fully consider as one it
    // did — the same defect `RequiresEditor` exists to prevent one level up.
    const auto ExternalNote = [&Result]() -> FString
    {
        if (NOT Result.HasExternalReferenceProvider)
        {
            return FString{TEXT(" — no external-reference provider was registered, so references made only from ")
                TEXT("script or config were NOT considered")};
        }

        if (Result.ExternallyReferencedCount == 0)
        { return FString{}; }

        auto SourceNames = TArray<FString>{};
        SourceNames.Reserve(Result.ExternalReferenceSourceIds.Num());

        for (const auto SourceId : Result.ExternalReferenceSourceIds)
        { SourceNames.Add(SourceId.ToString()); }

        return ck::Format_UE(TEXT(" — {} asset(s) kept off the unreferenced list because {} references them"),
            Result.ExternallyReferencedCount,
            FString::Join(SourceNames, TEXT(", ")));
    }();

    const auto Text = ck::Format_UE(
        TEXT("{} unreferenced / {} possible duplicate(s) / {} name collision(s) / {} redirector(s) / {} dirty — ")
        TEXT("{} reclaimable, scanned at {}{}{}"),
        Counted(ECkOptimizationDebugger_CleanupCategory::Unreferenced),
        Counted(ECkOptimizationDebugger_CleanupCategory::Duplicates),
        Counted(ECkOptimizationDebugger_CleanupCategory::NameCollisions),
        Counted(ECkOptimizationDebugger_CleanupCategory::Redirectors),
        Counted(ECkOptimizationDebugger_CleanupCategory::DirtyPackages),
        Format_ByteSize(Totals.ReclaimableBytes),
        _Model.Get_LastCleanupScanTime().ToString(),
        Result.WasStillIndexing
            ? FString{TEXT(" — the asset registry was still indexing, so these are a floor")}
            : FString{},
        ExternalNote);

    // Cancelled and still-indexing are both partial answers, and the reader has to know that BEFORE they read the
    // counts: "0 unreferenced" is a different sentence when half the project was not looked at.
    // A missing provider is Warn-toned for the same reason a cancelled or still-indexing scan is: all three are
    // partial answers, and the tone is what tells the reader that BEFORE they read the counts.
    DoSet_Status(Result.WasCancelled
        ? ck::Format_UE(TEXT("Scan cancelled — {}"), Text)
        : Text,
        Result.WasCancelled || Result.WasStillIndexing || NOT Result.HasExternalReferenceProvider
            ? ECk_Tone::Warn
            : ECk_Tone::Ok);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRebuild_Cleanup()
    -> void
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_model;

    // Cached so the header tiles read a field instead of re-counting every row on the paint path.
    _CleanupTotals = _Model.Get_CleanupTotals();

    // All four categories, for the same reason the memory rebuild caches all three tables: the sub-tabs each show a
    // filtered count and re-deriving them per paint walks the whole `/Game` census once per tab, per frame.
    const auto AllCategories = Get_AllCleanupCategories();

    // Sized from the largest enum VALUE, for the reason the memory rebuild is.
    auto MaxCategoryIndex = -1;

    for (const auto Category : AllCategories)
    { MaxCategoryIndex = FMath::Max(MaxCategoryIndex, static_cast<int32>(Category)); }

    _VisibleCleanupCountByCategory.Reset();
    _VisibleCleanupCountByCategory.AddZeroed(MaxCategoryIndex + 1);

    for (const auto Category : AllCategories)
    {
        const auto Index = static_cast<int32>(Category);

        if (_VisibleCleanupCountByCategory.IsValidIndex(Index))
        { _VisibleCleanupCountByCategory[Index] = _Model.Get_VisibleCleanupRowCount(Category); }
    }

    // Stable row identity: reuse the TSharedPtr whose key matches, allocate only for genuinely new lines. Resetting
    // and re-allocating per rebuild would destroy the selection every keystroke in the filter box.
    auto Existing = MoveTemp(_CleanupItemsByKey);
    _CleanupItemsByKey.Reset();
    _CleanupItems.Reset();

    auto NewOrder = TArray<FString>{};

    const auto TakeItem = [&Existing](const FString& InKey) -> FCleanupItem
    {
        if (auto* Found = Existing.Find(InKey))
        {
            auto Item = *Found;
            Existing.Remove(InKey);
            return Item;
        }

        return MakeShared<FCkOptimizationDebugger_CleanupListItem>();
    };

    const auto AddRow = [this, &TakeItem, &NewOrder](const FCkOptimizationDebugger_CleanupRow& InRow) -> void
    {
        // `<category id>|<asset path>` — the category is part of the key because one asset can legitimately appear
        // under two categories, and a key that dropped it would make the second appearance replace the first.
        const auto Key = ck::Format_UE(TEXT("{}|{}"),
            Get_CleanupCategoryId(InRow.Category), InRow.AssetPath);

        auto Item = TakeItem(Key);

        Item->Key = Key;
        Item->IsGroupHeader = false;
        Item->Title = InRow.DisplayName;
        Item->GroupCount = 0;
        Item->GroupReclaimableBytes = 0;
        Item->Row = InRow;

        _CleanupItemsByKey.Add(Key, Item);
        NewOrder.Add(Key);
        _CleanupItems.Add(MoveTemp(Item));
    };

    if (Get_IsGroupedCleanupCategory(_ActiveCleanupCategory))
    {
        // Grouped, because a duplicate — and a name collision — only means anything next to the assets it matches.
        // Each group gets one non-selectable header line, exactly as a finding group does.
        for (const auto& Group : _Model.Get_CleanupGroups(_ActiveCleanupCategory))
        {
            const auto GroupKey = k_GroupKeyPrefix + Group.GroupKey;

            auto HeaderItem = TakeItem(GroupKey);

            HeaderItem->Key = GroupKey;
            HeaderItem->IsGroupHeader = true;
            HeaderItem->Title = Group.DisplayName;
            HeaderItem->GroupCount = Group.Rows.Num();

            // What keeping ONE copy would free — the number the reader is actually weighing. Zero for a name
            // collision, whose group carries no size because resolving one frees nothing.
            HeaderItem->GroupReclaimableBytes = Group.DiskSizeBytes * static_cast<int64>(Group.Rows.Num() - 1);

            // The header carries the active category on its otherwise-empty row so the row generator can word itself
            // without reaching back for window state. A header that said "copies" over a set of name collisions would
            // be telling the reader to go delete one of them.
            HeaderItem->Row = FCkOptimizationDebugger_CleanupRow{};
            HeaderItem->Row.Category = _ActiveCleanupCategory;

            _CleanupItemsByKey.Add(GroupKey, HeaderItem);
            NewOrder.Add(GroupKey);
            _CleanupItems.Add(MoveTemp(HeaderItem));

            for (const auto& Row : Group.Rows)
            { AddRow(Row); }
        }
    }
    else
    {
        for (const auto& Row : _Model.Get_SortedCleanupRows(_ActiveCleanupCategory))
        { AddRow(Row); }
    }

    // The ORDER, not just the membership: switching categories replaces the whole sequence, and `SListView` renders
    // in item-source order.
    const auto OrderChanged = NewOrder != _CleanupRowOrder;

    _CleanupRowOrder = MoveTemp(NewOrder);

    if (ck::IsValid(_CleanupList) && OrderChanged)
    { _CleanupList->RequestListRefresh(); }

    DoRefresh_CleanupCommands();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoSelect_CleanupCategory(
        ECkOptimizationDebugger_CleanupCategory InCategory)
    -> void
{
    if (_ActiveCleanupCategory == InCategory)
    { return; }

    _ActiveCleanupCategory = InCategory;

    DoRebuild_Cleanup();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    TryGet_ActiveCleanupAction() const
    -> const FCkOptimizationDebugger_CleanupActionInfo*
{
    return ck_optimization_debugger_cleanup_commands::TryGet_ActionForCategory(_ActiveCleanupCategory);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    Get_SelectedCleanupRows() const
    -> TArray<FCkOptimizationDebugger_CleanupRow>
{
    auto Rows = TArray<FCkOptimizationDebugger_CleanupRow>{};

    if (ck::Is_NOT_Valid(_CleanupList))
    { return Rows; }

    for (const auto& Item : _CleanupList->GetSelectedItems())
    {
        if (NOT Item.IsValid() || Item->IsGroupHeader)
        { continue; }

        Rows.Add(Item->Row);
    }

    return Rows;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRefresh_CleanupCommands()
    -> void
{
    using namespace ck_optimization_debugger_cleanup_commands;

    const auto* Action = TryGet_ActiveCleanupAction();

    if (Action == nullptr)
    {
        _CleanupActionEnabled = false;
        _CleanupActionLabel = FString{TEXT("No action")};

        // Named rather than shrugged at. "This category has nothing to act on" reads as a tool that has not finished
        // being written; the reader of the collisions tab needs to know the button is absent BECAUSE the fix is a
        // rename, which is a content decision no batch action should be making for them.
        _CleanupActionTooltip =
            _ActiveCleanupCategory == ECkOptimizationDebugger_CleanupCategory::NameCollisions
                ? FString{TEXT("Nothing here can be fixed by a button. Resolving a name collision means renaming one ")
                    TEXT("of the assets, which is a content decision — rename it in the Content Browser and re-scan.")}
                : FString{TEXT("This category has nothing to act on.")};
        return;
    }

    const auto Selected = Get_SelectedCleanupRows();
    const auto Applicable = Get_ApplicableRows(*Action, Selected);

    _CleanupActionLabel = Build_ActionButtonLabel(*Action, Applicable);

    // Session availability is checked HERE rather than inside the pure rule: it is a session fact, not a property of
    // the selection, and folding it in would make the rule need an editor to be asserted. The reason is asked for by
    // name rather than assumed — "not here" and "not while you are playing" are different answers and the disabled
    // tooltip has to give the right one.
    const auto Unavailable = Get_ActionsUnavailableReason();

    if (NOT Unavailable.IsEmpty())
    {
        _CleanupActionEnabled = false;
        _CleanupActionTooltip = Unavailable;
        return;
    }

    const auto Reason = Get_ActionDisabledReason(*Action, Selected);

    _CleanupActionEnabled = Reason.IsEmpty();

    // The disabled tooltip answers "why is this one not available" in place. The enabled one is the action's own
    // description, which is where the engine dialog is promised.
    _CleanupActionTooltip = Reason.IsEmpty() ? Action->Description : Reason;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnCleanupActionClicked()
    -> FReply
{
    DoRun_CleanupAction();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRun_CleanupAction()
    -> void
{
    const auto* Action = TryGet_ActiveCleanupAction();

    if (Action == nullptr)
    {
        DoSet_Status(TEXT("This category has nothing to act on."), ECk_Tone::Neutral);
        return;
    }

    const auto Result = ck_optimization_debugger_cleanup_commands::Run_Action(*Action, Get_SelectedCleanupRows());

    // A refusal is Warn, never Err: a reader who cancelled the engine's own delete dialog has not failed at anything,
    // and that dialog existing to be cancelled is the whole safety contract.
    DoSet_Status(Result.Message, Result.DidRun ? ECk_Tone::Ok : ECk_Tone::Warn);

    if (NOT Result.DidRun)
    { return; }

    // `ShouldRefresh` is false when the engine took the work but has not done it yet — `FixupReferencers` defers to
    // a completion callback while the asset registry is still indexing. Re-scanning on top of that would produce a
    // census describing a state nothing has reached, and the reader would read it as the outcome.
    if (NOT Result.ShouldRefresh)
    { return; }

    // Straight back down the ONE scan path. An action that changed what is on disk has changed the answer this list
    // is showing, and refreshing by any second route would be a route that forgets to drop the row-identity map.
    DoRun_CleanupScan();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoNavigate_ToCleanupRow(
        const FCkOptimizationDebugger_CleanupRow& InRow)
    -> void
{
    using namespace ck_optimization_debugger_window;

    // The SAME navigation an asset-targeted finding and a memory row take. There is one way to show the reader an
    // asset, and it is not a third copy of this logic.
    const auto Result = ck_optimization_debugger_navigation::Navigate_ToTarget(Build_CleanupTarget(InRow));

    DoSet_Status(Result.Message, Result.Succeeded ? ECk_Tone::Ok : ECk_Tone::Warn);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoNavigate_ToSelectedCleanupRow()
    -> void
{
    const auto Selected = Get_SelectedCleanupRows();

    if (Selected.IsEmpty())
    {
        DoSet_Status(TEXT("Select a row first."), ECk_Tone::Neutral);
        return;
    }

    // One target even from a multi-selection: "go to" names a place, and there is one Content Browser to put the
    // reader in.
    DoNavigate_ToCleanupRow(Selected[0]);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoGenerate_CleanupRow(
        FCleanupItem InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_model;

    if (NOT InItem.IsValid())
    {
        return SNew(STableRow<FCleanupItem>, InOwnerTable)
            .Style(&Get_RowStyle());
    }

    const auto WeakRow = TWeakPtr<FCkOptimizationDebugger_CleanupListItem>{InItem};

    // Row-safe widgets only — STextBlock / SBox / SCkDebug_Icon and nothing that handles a click. The action lives on
    // a command button and in the context menu, never inside a row.
    if (InItem->IsGroupHeader)
    {
        return SNew(STableRow<FCleanupItem>, InOwnerTable)
            .Style(&Get_RowStyle())
            .Padding(FMargin{0.0f, CkStyle::SpaceS, 0.0f, CkStyle::SpaceXS})
            // A header names a MATCH, not a thing to delete. Selecting it would put a row in the action's selection
            // that names no asset.
            .ShowSelection(false)
            .ToolTipText_Lambda([WeakRow]() -> FText
            {
                const auto Row = WeakRow.Pin();

                if (NOT Row.IsValid())
                { return FText::GetEmpty(); }

                if (Row->Row.Category == ECkOptimizationDebugger_CleanupCategory::NameCollisions)
                {
                    return FText::FromString(ck::Format_UE(
                        TEXT("{} assets answer to this name. Not duplicates — the fix is a rename, and nothing here ")
                        TEXT("is reclaimable"),
                        Row->GroupCount));
                }

                return FText::FromString(ck::Format_UE(
                    TEXT("{} assets share this name, class and size — keeping one would free {}"),
                    Row->GroupCount,
                    Format_ByteSize(Row->GroupReclaimableBytes)));
            })
            .Content()
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(Get_IconBrush(Get_CleanupCategoryIcon(InItem->Row.Category)))
                    .Meaning(FText::FromString(
                        InItem->Row.Category == ECkOptimizationDebugger_CleanupCategory::NameCollisions
                            ? TEXT("A set of assets sharing one name")
                            : TEXT("A set of possible duplicates")))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                    .Size(FVector2D{k_RowIconSize, k_RowIconSize})
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(InItem->Title))
                    .Font(CkStyle::BoldFont(CkStyle::FontSizeBody()))
                    .ColorAndOpacity(FSlateColor{CkStyle::Text()})
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, CkStyle::SpaceM, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(
                        InItem->Row.Category == ECkOptimizationDebugger_CleanupCategory::NameCollisions
                            // "assets", never "copies": these are unrelated assets that merely share a name, and no
                            // reclaimable figure, because resolving a collision frees nothing.
                            ? ck::Format_UE(TEXT("{} assets share this name"), InItem->GroupCount)
                            : ck::Format_UE(TEXT("{} copies · {} reclaimable"),
                                InItem->GroupCount, Format_ByteSize(InItem->GroupReclaimableBytes))))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ];
    }

    // A GROUPED category's rows are indented under their header; a flat category has no header to indent under, and
    // indenting it would leave a blank column nothing explains.
    const auto Indent = Get_IsGroupedCleanupCategory(InItem->Row.Category)
        ? k_RowIndent
        : CkStyle::SpaceS;

    return SNew(STableRow<FCleanupItem>, InOwnerTable)
        .Style(&Get_RowStyle())
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        .ToolTipText_Lambda([WeakRow]() -> FText
        {
            const auto Row = WeakRow.Pin();

            if (NOT Row.IsValid())
            { return FText::GetEmpty(); }

            return FText::FromString(ck::Format_UE(TEXT("{}\n{}"), Row->Row.AssetPath, Row->Row.Detail));
        })
        .Content()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            .Padding(Indent, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(Get_IconBrush(Get_CleanupCategoryIcon(InItem->Row.Category)))
                .Meaning(FText::FromString(Get_CleanupCategoryLabel(InItem->Row.Category)))
                .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                .Size(FVector2D{k_RowIconSize, k_RowIconSize})
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(InItem->Row.DisplayName))
                .ColorAndOpacity(FSlateColor{CkStyle::Text()})
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(k_CleanupClassColumnWidth)
                .HAlign(HAlign_Left)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(InItem->Row.ClassName))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(k_CleanupSizeColumnWidth)
                .HAlign(HAlign_Right)
                [
                    SNew(STextBlock)
                    // An em dash rather than "0 B" for a package that has never been written: zero bytes on disk and
                    // nothing on disk are different statements.
                    .Text(FText::FromString(InItem->Row.DiskSizeBytes > 0
                        ? Format_ByteSize(InItem->Row.DiskSizeBytes)
                        : FString{TEXT("—")}))
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor{InItem->Row.DiskSizeBytes > 0
                        ? CkStyle::Text()
                        : CkStyle::TextMute()})
                ]
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.4f)
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f, CkStyle::SpaceM, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(InItem->Row.Detail))
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnCleanupSelectionChanged(
        FCleanupItem InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    (void)InItem;

    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    // The action button's label and enabled state are the only things a cleanup selection drives — there is no detail
    // panel on this page, because a row's whole detail is already the line it prints.
    DoRefresh_CleanupCommands();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnCleanupDoubleClicked(
        FCleanupItem InItem)
    -> void
{
    if (NOT InItem.IsValid() || InItem->IsGroupHeader)
    { return; }

    DoNavigate_ToCleanupRow(InItem->Row);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnCleanupContextMenu()
    -> TSharedPtr<SWidget>
{
    using namespace ck_optimization_debugger_window;
    using namespace ck_optimization_debugger_cleanup_commands;

    if (ck::Is_NOT_Valid(_CleanupList))
    { return nullptr; }

    auto Names = TArray<FString>{};
    auto Paths = TArray<FString>{};
    auto Summaries = TArray<FString>{};

    for (const auto& Item : _CleanupList->GetSelectedItems())
    {
        if (NOT Item.IsValid() || Item->IsGroupHeader)
        { continue; }

        Names.Add(Item->Row.DisplayName);
        Paths.Add(Item->Row.AssetPath);
        Summaries.Add(Build_CleanupRowSummary(Item->Row));
    }

    if (Summaries.IsEmpty())
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};

    // The actions come first and the copy entries after — the same ordering the findings and memory menus use.
    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Show in Content Browser")),
        FText::FromString(TEXT("Sync the Content Browser to this asset — the same thing double-clicking the row does")),
        FSlateIcon{},
        FUIAction{FExecuteAction::CreateSP(this,
            &SCkOptimizationDebuggerWindow::DoNavigate_ToSelectedCleanupRow)});

    if (const auto* Action = TryGet_ActiveCleanupAction())
    {
        const auto Applicable = Get_ApplicableRows(*Action, Get_SelectedCleanupRows());

        // Present only when it would actually do something. A greyed entry in a context menu is a line the reader
        // reads once and never again.
        if (Can_RunAction(*Action, Get_SelectedCleanupRows()) && Get_CanRunActions())
        {
            MenuBuilder.AddMenuEntry(
                FText::FromString(Build_ActionButtonLabel(*Action, Applicable)),
                FText::FromString(Action->Description),
                FSlateIcon{},
                FUIAction{FExecuteAction::CreateSP(this, &SCkOptimizationDebuggerWindow::DoRun_CleanupAction)});
        }
    }

    MenuBuilder.AddMenuSeparator();

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Name")),
        FText::FromString(TEXT("Copy the selected assets' names, one per line")),
        FString::Join(Names, TEXT("\n")));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Path")),
        FText::FromString(TEXT("Copy the selected assets' paths, one per line")),
        FString::Join(Paths, TEXT("\n")));

    ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
        FText::FromString(TEXT("Copy Summary")),
        FText::FromString(TEXT("Copy category, name, path, class, size and reason for each selected row")),
        FString::Join(Summaries, TEXT("\n")));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------
// Profiling launcher
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnProfileCommandClicked(
        FName InCommandId)
    -> FReply
{
    // Looked up by id rather than captured by value, so the catalog stays the single copy of every entry — a widget
    // holding its own snapshot would be a second place the wording and the lever could drift apart.
    if (const auto* Command = ck_optimization_debugger_profile_commands::TryGet_Command(InCommandId))
    { DoExecute_ProfileCommand(*Command); }

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoExecute_ProfileCommand(
        const FCkOptimizationDebugger_ProfileCommand& InCommand)
    -> void
{
    const auto Result = ck_optimization_debugger_profile_commands::Execute_Command(InCommand);

    // Nothing is rebuilt here. Every control on the page reads the viewport back on its next paint, so the button
    // that was just pressed shows its new state without this window remembering anything about it.
    DoSet_Status(Result.Message, Result.DidRun ? ECk_Tone::Ok : ECk_Tone::Warn);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnRunCustomCommandClicked()
    -> FReply
{
    DoRun_CustomCommand();

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnCustomCommandCommitted(
        const FText& InText,
        ETextCommit::Type InCommitType)
    -> void
{
    (void)InText;

    // Enter ONLY — deliberately the opposite of `SCkDebug_NumericEditor`'s commit-on-focus-loss rule. A threshold
    // that commits when the reader clicks away is a value they meant; a console command that runs when they click
    // away is a command they did not press anything to run.
    if (InCommitType != ETextCommit::OnEnter)
    { return; }

    DoRun_CustomCommand();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRun_CustomCommand()
    -> void
{
    using namespace ck_optimization_debugger_profile_commands;

    if (ck::Is_NOT_Valid(_CustomCommandBox))
    { return; }

    const auto Command = _CustomCommandBox->GetText().ToString();
    const auto Result = Execute_CustomCommand(Command);

    DoSet_Status(Result.Message, Result.DidRun ? ECk_Tone::Ok : ECk_Tone::Warn);

    if (NOT Result.DidRun)
    { return; }

    // The text stays in the box on purpose: the command after a `r.ScreenPercentage 50` is usually
    // `r.ScreenPercentage 100`, and clearing the field would make the reader retype the half they wanted to keep.
    Push_RecentCommand(_RecentCommands, Command, k_MaxRecentCommands);

    DoRebuild_Profiling();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoRebuild_Profiling()
    -> void
{
    using namespace ck_optimization_debugger_window;

    if (ck::Is_NOT_Valid(_RecentCommandsBox))
    { return; }

    _RecentCommandsBox->ClearChildren();

    // No rail at all until something has been run. An empty "RECENT" heading over nothing is a promise the page has
    // not kept yet.
    if (_RecentCommands.IsEmpty())
    { return; }

    auto Chips = SNew(SWrapBox)
        .UseAllottedSize(true)
        .InnerSlotPadding(FVector2D{CkStyle::SpaceS, CkStyle::SpaceS});

    for (const auto& Recent : _RecentCommands)
    {
        Chips->AddSlot()
        [
            // A CLICKABLE chip, which is legal here and nowhere near a list: this is a fixed panel, so there is no
            // STableRow whose selection click the chip could eat. Right-click copies the command text.
            SNew(SCkDebug_Chip)
            .Text(FText::FromString(Recent))
            .Kind(ECkDebug_ChipKind::Neutral)
            .ShowDot(false)
            .CopyText(Recent)
            .OnClicked(FOnCkDebug_ChipClicked::CreateLambda([this, Recent]() -> void
            {
                // Put it back in the box first, then run THAT — so re-running goes down the same path a typed
                // command does, and the reader can see what was run rather than inferring it.
                if (ck::IsValid(_CustomCommandBox))
                { _CustomCommandBox->SetText(FText::FromString(Recent)); }

                DoRun_CustomCommand();
            }))
        ];
    }

    _RecentCommandsBox->AddSlot()
    .AutoHeight()
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("RECENT")))
        .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
        .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
    ];

    _RecentCommandsBox->AddSlot()
    .AutoHeight()
    .Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
    [
        Chips
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    Get_CachedMemoryCount(
        ECkOptimizationDebugger_MemoryTable InTable) const
    -> int32
{
    const auto Index = static_cast<int32>(InTable);

    return _VisibleMemoryCountByTable.IsValidIndex(Index) ? _VisibleMemoryCountByTable[Index] : 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    Get_CachedCleanupCount(
        ECkOptimizationDebugger_CleanupCategory InCategory) const
    -> int32
{
    const auto Index = static_cast<int32>(InCategory);

    return _VisibleCleanupCountByCategory.IsValidIndex(Index) ? _VisibleCleanupCountByCategory[Index] : 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    Get_CachedCategoryCount(
        ECkOptimizationDebugger_Category InCategory) const
    -> int32
{
    const auto Index = static_cast<int32>(InCategory);

    return _VisibleCategoryCounts.IsValidIndex(Index) ? _VisibleCategoryCounts[Index] : 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoSelect_Page(
        ECkOptimizationDebugger_Page InPage)
    -> void
{
    _Model.Set_ActivePage(InPage);

    if (ck::Is_NOT_Valid(_PageSwitcher))
    { return; }

    _PageSwitcher->SetActiveWidgetIndex(ck_optimization_debugger_model::Get_PageIndex(InPage));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
    DoOnDeferredDashboardRebuild(
        double InCurrentTime,
        float InDeltaTime)
    -> EActiveTimerReturnType
{
    (void)InCurrentTime;
    (void)InDeltaTime;

    // Consumed here rather than at the call site, so a second edit that ended in the same frame cannot queue a
    // second rebuild for the same request.
    if (_ThresholdEditGuard.IsValid() && _ThresholdEditGuard->Consume_PendingRebuild())
    { DoRebuild_Dashboard(); }

    return EActiveTimerReturnType::Stop;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationDebuggerWindow::
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
    SCkOptimizationDebuggerWindow::
    DoOnSessionInvalidated()
    -> void
{
    // Entering or leaving PIE swaps the world an actor path resolves against. Findings from the previous session
    // would still LOOK right and navigate nowhere, so they go — the user re-scans the world they are now in. The
    // resident census goes with them for a different reason: a play session loads and unloads content wholesale, so
    // a memory table taken before the boundary describes objects that may not be there any more.
    //
    // The cleanup census SURVIVES, minus one category. "An asset nothing references is unreferenced whether or not
    // somebody pressed Play" is true of unreferenced assets, duplicates and redirectors — all registry facts. It is
    // NOT true of dirty packages, which are live editor state a play session changes. Keeping those while the status
    // strip below says "results cleared" had the page contradicting itself in two places at once.
    const auto DroppedDirtyRows = _Model.Drop_DirtyPackageRows();

    if (NOT _Model.Get_HasScanned() && NOT _Model.Get_HasMemoryScan() && DroppedDirtyRows == 0)
    { return; }

    _SelectedFindingKey.Empty();

    _MemoryItems.Reset();
    _MemoryItemsByPath.Reset();
    _MemoryRowOrder.Reset();

    // Dropping rows changes the cleanup list's membership, so its row-identity map has to go with them — a stale
    // entry would keep a vanished row's pointer alive and the order comparison would not see the change.
    if (DroppedDirtyRows > 0)
    {
        _CleanupItems.Reset();
        _CleanupItemsByKey.Reset();
        _CleanupRowOrder.Reset();
    }

    _Model.Reset();

    DoRebuild_All(/*InRefreshStatus*/ false);

    // Written LAST, after `DoRebuild_All` — which routes through `DoRebuild_Findings` and therefore through
    // `DoRefresh_Status`. Whatever that wrote is overwritten here on purpose: this is the sentence that explains why
    // the window just emptied.
    DoSet_Status(DroppedDirtyRows > 0
        ? ck::Format_UE(TEXT("Results cleared — the play session changed what is loaded and what the paths point at. Re-scan. ({} dirty-package row(s) dropped; the rest of the cleanup census still stands.)"),
            DroppedDirtyRows)
        : FString{TEXT("Results cleared — the play session changed what is loaded and what the paths point at. Re-scan.")},
        ECk_Tone::Neutral);
}

// --------------------------------------------------------------------------------------------------------------------
