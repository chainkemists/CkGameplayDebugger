#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
class FCkDebuggerModel_EntitySelection;

// --------------------------------------------------------------------------------------------------------------------

struct FCkInspector_Chip
{
    FText Text;
    ECk_Tone Tone = ECk_Tone::Neutral;
};

struct FCkInspector_TimelineContent
{
    double TimeMin = 0.0;
    double TimeMax = 1.0;
    TArray<FCkDebug_TimelineEvent> Events;
    TArray<FCkDebug_TimelineSpan> Spans;
};

/** How AddAlignedNumericRow lays its components out — see Get_NumericLayout. */
enum class ECkInspector_NumericLayout : uint8
{
    AlignedColumns,
    SingleLine_Left,
    SingleLine_Right,
};

// --------------------------------------------------------------------------------------------------------------------

class FCkInspectorWidgetBuilder
{
public:
    using FValueGetter = TFunction<FText(const FCk_Handle&)>;
    using FColorGetter = TFunction<FLinearColor(const FCk_Handle&)>;
    using FOnClicked = TFunction<void()>;

    /** Current text of a widget-hosted row's value, read once per filter pass. See Matches_Filter. */
    using FFilterValueGetter = TFunction<FString()>;

    auto SetSelectionModel(TSharedPtr<FCkDebuggerModel_EntitySelection> InModel) -> FCkInspectorWidgetBuilder&;

    auto AddRow(
        const FText& InLabel,
        FValueGetter InValueGetter,
        const FLinearColor& InValueColor = CkStyle::Text()) -> FCkInspectorWidgetBuilder&;

    auto AddConditionalRow(
        const FText& InLabel,
        FValueGetter InValueGetter,
        FColorGetter InColorGetter) -> FCkInspectorWidgetBuilder&;

    auto AddClickableRow(
        const FText& InLabel,
        FValueGetter InValueGetter,
        const FLinearColor& InValueColor,
        FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&;

    auto AddClickableRow(
        const FText& InLabel,
        FValueGetter InValueGetter,
        FColorGetter InColorGetter,
        FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&;

    auto AddHeader(const FText& InHeaderText) -> FCkInspectorWidgetBuilder&;

    /**
     * Bounded quantity as a track+fill meter (timer remaining, attribute current/max, playback
     * position). InFraction is clamped to 0..1 by the widget. InValueText, when set, renders the
     * number beside the bar and is what the value filter matches; otherwise the fraction is
     * matched as a percentage.
     */
    auto AddMeterRow(
        const FText& InLabel,
        TAttribute<float> InFraction,
        ECk_Tone InTone = ECk_Tone::Accent,
        TAttribute<FText> InValueText = TAttribute<FText>{}) -> FCkInspectorWidgetBuilder&;

    /**
     * Recent history of a scalar (velocity, rate) as a sparkline.
     *
     * The sample ring is ROW-OWNED: the builder allocates it here and it dies with the row widget,
     * so callers keep no per-entity state and OnDeactivated has nothing to release. Sampling is
     * driven by the row's own value attribute — i.e. the row samples while it is painted, at most
     * once per engine frame (GFrameCounter stamp), and not at all while filtered out or scrolled
     * away. Cadence is therefore paint cadence, not a fixed Hz: read the shape, not the time axis.
     */
    auto AddSparklineRow(
        const FText& InLabel,
        TAttribute<float> InSample,
        ECk_Tone InTone = ECk_Tone::Accent,
        TAttribute<FText> InValueText = TAttribute<FText>{},
        int32 InHistoryLength = 48) -> FCkInspectorWidgetBuilder&;

    /** Single-word live state (NetRole, SM state, enabled/disabled) as a toned pill. */
    auto AddStatusPillRow(
        const FText& InLabel,
        TAttribute<FText> InText,
        TAttribute<ECk_Tone> InTone) -> FCkInspectorWidgetBuilder&;

    /**
     * Tag / feature lists as a chip wrap-box, composed through the ChipStyle axis.
     * The chip set is a STRUCTURAL snapshot taken when the row is composed — a changed set needs
     * the owning inspector's RequestRebuild, exactly like MakeBadgeBox.
     */
    auto AddChipsRow(
        const FText& InLabel,
        const TArray<FCkInspector_Chip>& InChips) -> FCkInspectorWidgetBuilder&;

    /** Event history (SM transitions, replans) as a timeline. Snapshot semantics as AddChipsRow. */
    auto AddTimelineRow(
        const FText& InLabel,
        const TArray<FString>& InLaneLabels,
        const FCkInspector_TimelineContent& InContent,
        float InDesiredHeight = 96.0f) -> FCkInspectorWidgetBuilder&;

    /**
     * One aligned monospace numeric row. Components render right-aligned in equal fixed-width
     * columns; color by component index 0/1/2 = the X/Y/Z axis roles, further components neutral.
     * Honors the ValueAlignment axis — Left/Right degrade to one concatenated text with that
     * alignment. The layout is chosen when the row is composed, so an axis change takes effect on
     * the next inspector rebuild.
     */
    auto AddAlignedNumericRow(
        const FText& InLabel,
        const TArray<TAttribute<FText>>& InComponents) -> FCkInspectorWidgetBuilder&;

    /** Place a pre-built widget directly in the value column, next to a plain label. */
    auto AddWidgetRow(
        const FText& InLabel,
        TSharedRef<SWidget> InWidget) -> FCkInspectorWidgetBuilder&;

    /**
     * Same as AddClickableRow, but the value column hosts an arbitrary widget instead of dynamic text.
     * The label column remains a clickable button that fires the supplied delegate.
     */
    auto AddClickableWidgetRow(
        const FText& InLabel,
        TSharedRef<SWidget> InValueWidget,
        FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&;

    /**
     * Build a clickable entity badge wrap-box. Each handle becomes an
     * SCkDebug_EntityRef pill; clicking opens that entity in the CK ECS
     * Debugger via ck::DebugNav (right-click → copy works too). Used by
     * inspectors that show lists of related entities (probe overlaps,
     * interaction targets, scene-node siblings, etc.).
     */
    static auto MakeBadgeBox(
        const TArray<FCk_Handle>& InHandles) -> TSharedRef<SWrapBox>;

    /** Populate (or repopulate) an existing badge box with the given handles. */
    static auto PopulateBadgeBox(
        SWrapBox& InBox,
        const TArray<FCk_Handle>& InHandles) -> void;

    auto Build(const FCk_Handle& InEntity, const FString& InFilter = FString()) -> TSharedRef<SWidget>;

    // ----- Pure helpers (shared with the specs) -------------------------------

    /**
     * A row survives the filter when its LABEL matches, or when its current VALUE does. Both use the
     * same fuzzy matcher, so value matching is as loose as label matching has always been. The value
     * is sampled once per filter pass — that pass runs inside Build, which the inspector panel calls
     * on rebuild (selection change, structural refresh) and on every filter-text keystroke
     * (SCkDebuggerPanel_Inspector::OnInspectorFilterChanged). A value that changes afterwards does
     * NOT re-filter until the next such pass.
     */
    static auto Matches_Filter(
        const FString& InFilter,
        const FString& InLabel,
        const FString& InValue) -> bool;

    static auto Get_NumericLayout(ECkDebugAxis_ValueAlignment InAlignment) -> ECkInspector_NumericLayout;

    /**
     * Width of one aligned numeric column. Constant while the components fit the value budget, so
     * 1..3-component rows (Location / Rotation / Scale) line up with each other; wider rows share the
     * same budget down to a legible floor.
     */
    static auto Get_AlignedColumnWidth(int32 InComponentCount) -> float;

    static auto Get_AxisColor(int32 InComponentIndex) -> FLinearColor;

    static auto Join_NumericComponents(const TArray<FText>& InComponents) -> FText;

private:
    struct FRowDefinition
    {
        FText Label;
        FValueGetter ValueGetter;
        FColorGetter ColorGetter;
        FOnClicked OnClicked;
        TSharedPtr<SWidget> CustomWidget;
        bool IsHeader = false;
        FFilterValueGetter FilterValueGetter;
    };

    auto DoAddWidgetValueRow(
        const FText& InLabel,
        TSharedRef<SWidget> InValueWidget,
        FFilterValueGetter InFilterValueGetter) -> FCkInspectorWidgetBuilder&;

    TArray<FRowDefinition> Rows;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
};
