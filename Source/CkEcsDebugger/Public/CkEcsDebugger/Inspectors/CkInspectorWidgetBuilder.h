#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"

#include "GameplayTagContainer.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
class FCkDebuggerModel_EntitySelection;
class FCkInspectorEditGuard;
class FCkInspectorEditScope;

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

/**
 * One flat button in an AddActionRow. Requirement gates the button against the inspected entity's
 * world net mode (ck::DebugRequestGate) — a gated-out button renders greyed and carries the reason
 * appended to its own tooltip, instead of firing a request the processor will ensure-and-drop.
 */
struct FCkInspector_Action
{
    FText ButtonLabel;
    FText Tooltip;
    TFunction<void()> OnClicked;
    ECk_DebugRequest_Requirement Requirement = ECk_DebugRequest_Requirement::LocalOk;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Per-row change tracking for the FlashOnChange axis.
 *
 * ROW-OWNED, like AddSparklineRow's sample ring: the builder allocates one only when the axis is on,
 * the row's own attributes capture it by value, and it dies with the row widget. Off allocates
 * nothing and composes the same widget tree as before the axis existed.
 *
 * The clock is Slate PAINT time, not a tick count — attributes re-evaluate on paint, so a row that is
 * filtered out or scrolled away never advances its own flash, and the panel's refresh gate does not
 * have to know the flash exists.
 */
struct FCkInspector_FlashState
{
    FString LastValue;
    double  LastChangeSeconds = 0.0;

    /** The first observation SEEDS; it never flashes, or every rebuild would light the whole panel. */
    bool    HasSeenValue = false;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Compose-time capture of one builder pass's label -> value pairs — how the inspector panel's
 * multi-select DIFF mode samples the OTHER selected entities without duplicating any inspector's row
 * authoring. While a scope is active, Build() records and composes nothing: the returned widget is
 * the null widget and the caller is expected to discard it.
 *
 * Values are sampled BEFORE the row filter, so the map is filter-independent — a row hidden for one
 * entity and shown for another must not read as "this label is missing".
 *
 * Duplicate labels within one pass collapse (last wins), consistently for every entity.
 * Scopes nest (innermost wins) and are main-thread only, like every other Slate compose path.
 */
class FCkInspector_RowCaptureScope
{
public:
    FCkInspector_RowCaptureScope();
    ~FCkInspector_RowCaptureScope();

    FCkInspector_RowCaptureScope(const FCkInspector_RowCaptureScope&) = delete;
    auto operator=(const FCkInspector_RowCaptureScope&) -> FCkInspector_RowCaptureScope& = delete;

    auto Get_Rows() const -> const TMap<FString, FString>& { return _Rows; }

private:
    friend class FCkInspectorWidgetBuilder;

    TMap<FString, FString>        _Rows;
    FCkInspector_RowCaptureScope* _Previous = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Marks the rows whose label compared UNEQUAL across the selected entities. Panel-owned: the set is
 * computed once per gated rebuild out of FCkInspector_RowCaptureScope samples, then installed around
 * the REAL Build_Inspector call. A null set is an inactive scope, so the non-diff path costs nothing
 * and composes the same tree it always did.
 */
class FCkInspector_DiffMarkScope
{
public:
    explicit FCkInspector_DiffMarkScope(const TSet<FString>* InDifferingLabels);
    ~FCkInspector_DiffMarkScope();

    FCkInspector_DiffMarkScope(const FCkInspector_DiffMarkScope&) = delete;
    auto operator=(const FCkInspector_DiffMarkScope&) -> FCkInspector_DiffMarkScope& = delete;

private:
    friend class FCkInspectorWidgetBuilder;

    const TSet<FString>*        _Labels   = nullptr;
    FCkInspector_DiffMarkScope* _Previous = nullptr;
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
     *
     * InTone is an attribute, so a threshold tone (Info → Warn → Err as a budget fills) follows the
     * data without a rebuild; a plain ECk_Tone still binds as a constant.
     *
     * When InOnClicked is bound the LABEL becomes the click target, exactly as in AddClickableRow —
     * the meter itself stays click-passive.
     */
    auto AddMeterRow(
        const FText& InLabel,
        TAttribute<float> InFraction,
        TAttribute<ECk_Tone> InTone = ECk_Tone::Accent,
        TAttribute<FText> InValueText = TAttribute<FText>{},
        FOnClicked InOnClicked = nullptr) -> FCkInspectorWidgetBuilder&;

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
        TAttribute<ECk_Tone> InTone = ECk_Tone::Accent,
        TAttribute<FText> InValueText = TAttribute<FText>{},
        int32 InHistoryLength = 48) -> FCkInspectorWidgetBuilder&;

    /**
     * Single-word live state (NetRole, SM state, enabled/disabled) as a toned pill.
     * InOnClicked follows the AddMeterRow contract — label clicks, pill stays passive.
     */
    auto AddStatusPillRow(
        const FText& InLabel,
        TAttribute<FText> InText,
        TAttribute<ECk_Tone> InTone,
        FOnClicked InOnClicked = nullptr) -> FCkInspectorWidgetBuilder&;

    /**
     * A live count as a badge (collection sizes, pending-op counts, body/track counts). InSuffix is
     * the fixed unit label beside the number ("nodes", "legs"); leave it empty for a bare count.
     * The tone is fixed: a count carries no threshold of its own, so nothing here decides that 0 is
     * a defect. InOnClicked follows the AddMeterRow contract — label clicks, badge stays passive.
     */
    auto AddCountBadgeRow(
        const FText& InLabel,
        TAttribute<int32> InCount,
        ECk_Tone InTone = ECk_Tone::Neutral,
        const FText& InSuffix = FText::GetEmpty(),
        FOnClicked InOnClicked = nullptr) -> FCkInspectorWidgetBuilder&;

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

    /**
     * Place a pre-built widget directly in the value column, next to a plain label.
     * A widget row has no ValueGetter, so it only participates in VALUE search when the caller
     * supplies InFilterValueGetter — the text the widget is showing (an entity id, a name).
     * Without it the row filters on its label alone.
     */
    auto AddWidgetRow(
        const FText& InLabel,
        TSharedRef<SWidget> InWidget) -> FCkInspectorWidgetBuilder&;

    auto AddWidgetRow(
        const FText& InLabel,
        TSharedRef<SWidget> InWidget,
        FFilterValueGetter InFilterValueGetter) -> FCkInspectorWidgetBuilder&;

    /**
     * Same as AddClickableRow, but the value column hosts an arbitrary widget instead of dynamic text.
     * The label column remains a clickable button that fires the supplied delegate.
     */
    auto AddClickableWidgetRow(
        const FText& InLabel,
        TSharedRef<SWidget> InValueWidget,
        FOnClicked InOnClicked) -> FCkInspectorWidgetBuilder&;

    auto AddClickableWidgetRow(
        const FText& InLabel,
        TSharedRef<SWidget> InValueWidget,
        FOnClicked InOnClicked,
        FFilterValueGetter InFilterValueGetter) -> FCkInspectorWidgetBuilder&;

    // ================================================================================================================
    // INTERACTIVE VOCABULARY
    //
    // Everything below composes a CONTROL into the value column. Three cross-cutting rules, honoured
    // by every one of them so an inspector author never re-derives them:
    //
    //  1. EditControlStyle axis (CkDebuggerStyleSelection). Hidden => the row degrades to its
    //     read-only equivalent (an action row disappears entirely — read-only inspectors, exactly as
    //     before this vocabulary existed). OnHover => the read-only value is what you see, and the
    //     control swaps in while the row is hovered. Inline (default) => the control is always there.
    //     The choice is made when the row is COMPOSED, so a flip takes effect on the next rebuild;
    //     the hover swap itself is live.
    //  2. Gate. Every control takes an ECk_DebugRequest_Requirement, evaluated LIVE against the
    //     inspected entity's world. A gated-out control is greyed AND carries the reason in its
    //     tooltip — never a live control that silently drops the request.
    //  3. Edit guard. Rows whose interaction has DURATION — the numeric / vector / name / tag
    //     type-in fields — register into the builder's FCkInspectorEditGuard for their focus window,
    //     so the panel DEFERS its rebuild until the edit ends. Atomic controls (switch, button,
    //     dropdown) take no scope: their input completes within one event, and a control that could
    //     fail to report its end (a dropdown dismissed without a selection) would wedge the guard,
    //     which is a worse failure than a rebuild behind an open popup.
    //
    // Values are written through the caller's setter, which must call a public Utils Request_*/Set_*
    // — never a fragment mutation. Capture the typed handle BY VALUE and ck::IsValid it on fire.
    // ================================================================================================================

    /** Panel-scoped guard, threaded panel -> inspector -> builder -> row. Null is valid (no deferral). */
    auto SetEditGuard(TSharedPtr<FCkInspectorEditGuard> InGuard) -> FCkInspectorWidgetBuilder&;

    /** A row of flat buttons — the arg-free verbs (Pause, Restart, Clear, ForceRefresh, …). */
    auto AddActionRow(
        const FText& InLabel,
        const TArray<FCkInspector_Action>& InActions) -> FCkInspectorWidgetBuilder&;

    /** Boolean via SCkDebug_Switch. Read-only fallback renders the current state as text. */
    auto AddToggleRow(
        const FText& InLabel,
        TAttribute<bool> InGet,
        TFunction<void(bool)> InSet,
        ECk_DebugRequest_Requirement InRequirement = ECk_DebugRequest_Requirement::LocalOk) -> FCkInspectorWidgetBuilder&;

    /** Single float via SCkDebug_NumericEditor (commit on enter / lost focus, never per keystroke). */
    auto AddNumericRow(
        const FText& InLabel,
        TAttribute<float> InGet,
        TFunction<void(float)> InSet,
        TOptional<float> InMin = TOptional<float>{},
        TOptional<float> InMax = TOptional<float>{},
        ECk_DebugRequest_Requirement InRequirement = ECk_DebugRequest_Requirement::LocalOk) -> FCkInspectorWidgetBuilder&;

    /** Single int32; same widget in Integer kind (rounds, no fractional part). */
    auto AddIntegerRow(
        const FText& InLabel,
        TAttribute<int32> InGet,
        TFunction<void(int32)> InSet,
        TOptional<int32> InMin = TOptional<int32>{},
        TOptional<int32> InMax = TOptional<int32>{},
        ECk_DebugRequest_Requirement InRequirement = ECk_DebugRequest_Requirement::LocalOk) -> FCkInspectorWidgetBuilder&;

    /**
     * Three aligned numeric editors sharing the aligned-column width, coloured by the X/Y/Z axis
     * roles (Get_AxisColor) — the same RGB convention the read-only Transform rows use. Committing
     * one component re-reads the whole vector and writes back a single modified copy.
     */
    auto AddVectorRow(
        const FText& InLabel,
        TAttribute<FVector> InGet,
        TFunction<void(const FVector&)> InSet,
        ECk_DebugRequest_Requirement InRequirement = ECk_DebugRequest_Requirement::LocalOk) -> FCkInspectorWidgetBuilder&;

    /**
     * As AddVectorRow, in the debugger's established (Roll, Pitch, Yaw) component order so index
     * 0/1/2 colouring matches the axis each angle turns about — label the row accordingly
     * ("Rotation (R,P,Y):"), exactly as CkInspector_Transform already does.
     */
    auto AddRotatorRow(
        const FText& InLabel,
        TAttribute<FRotator> InGet,
        TFunction<void(const FRotator&)> InSet,
        ECk_DebugRequest_Requirement InRequirement = ECk_DebugRequest_Requirement::LocalOk) -> FCkInspectorWidgetBuilder&;

    /**
     * Exclusive choice via SComboBox over caller-supplied display texts. The caller maps index <->
     * enum; ESelectInfo::Direct is ignored so a programmatic restore never echoes back as a request.
     */
    auto AddEnumDropdownRow(
        const FText& InLabel,
        const TArray<FText>& InOptions,
        TAttribute<int32> InGetSelectedIndex,
        TFunction<void(int32)> InSetSelectedIndex,
        ECk_DebugRequest_Requirement InRequirement = ECk_DebugRequest_Requirement::LocalOk) -> FCkInspectorWidgetBuilder&;

    /** Text entry committing on enter / lost focus, handed to the caller as an FName. */
    auto AddNameEntryRow(
        const FText& InLabel,
        TAttribute<FText> InGet,
        TFunction<void(FName)> InCommit,
        ECk_DebugRequest_Requirement InRequirement = ECk_DebugRequest_Requirement::LocalOk) -> FCkInspectorWidgetBuilder&;

    /**
     * Gameplay-tag entry. TEXT ENTRY, deliberately: the real picker (SGameplayTagPicker) lives in
     * GameplayTagsEditor, which no debugger module depends on — adding that dependency is not
     * "trivially available" and is left for a later unit. Only a tag that RESOLVES is committed;
     * unknown text is left uncommitted rather than clearing the caller's tag.
     */
    auto AddTagEntryRow(
        const FText& InLabel,
        TAttribute<FText> InGet,
        TFunction<void(FGameplayTag)> InCommit,
        ECk_DebugRequest_Requirement InRequirement = ECk_DebugRequest_Requirement::LocalOk) -> FCkInspectorWidgetBuilder&;

    // ================================================================================================================

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

    // ----- FlashOnChange (pure state machine) ---------------------------------

    /** Flash window, in seconds — the overlay focus card's 0.4s idiom, shared by both flash scopes. */
    static constexpr double FlashDurationSeconds = 0.4;

    /**
     * Fold one observed value into a row's flash state. The FIRST observation only seeds: a rebuild
     * (selection change, filter keystroke) allocates fresh state, and seeding is what keeps it from
     * flashing every row at once. Re-observing the SAME value is a no-op, so an attribute evaluated
     * several times per paint cannot restart or extend a flash.
     */
    static auto Observe_FlashChange(
        FCkInspector_FlashState& InOutState,
        const FString& InValue,
        double InNowSeconds) -> void;

    /**
     * 1 at the instant of change, decaying linearly to 0 at FlashDurationSeconds; 0 when nothing has
     * changed yet. A clock that ran BACKWARDS (new Slate app, new PIE session) reads as settled
     * rather than as a permanently lit row.
     */
    static auto Get_FlashAlpha(const FCkInspector_FlashState& InState, double InNowSeconds) -> float;

    // ----- Multi-select diff (pure comparison) --------------------------------

    /**
     * Labels whose value is NOT identical across every sampled entity. A label missing from any one
     * entity's map counts as differing — inspectors whose row set is dynamic per entity are exactly
     * the case the user wants surfaced. Fewer than two maps compare to nothing.
     */
    static auto Compute_DifferingLabels(
        const TArray<TMap<FString, FString>>& InPerEntityRows) -> TSet<FString>;

    /**
     * Badge text for a count. Grouped ("1,024") so four- and five-digit counts stay readable; a
     * negative count is a caller bug and renders verbatim rather than being clamped away.
     */
    static auto Format_Count(int32 InCount) -> FText;

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

    /**
     * The one value string a row contributes to filtering, diff capture, and flash tracking: its live
     * ValueGetter when it has one, otherwise its widget-hosted FilterValueGetter. Empty means the row
     * has no readable value and therefore never filters on value, never flashes, and never diffs.
     */
    static auto Resolve_RowValue(
        const FRowDefinition& InRow,
        const FCk_Handle& InEntity) -> FString;

    auto DoAddWidgetValueRow(
        const FText& InLabel,
        TSharedRef<SWidget> InValueWidget,
        FFilterValueGetter InFilterValueGetter,
        FOnClicked InOnClicked = nullptr) -> FCkInspectorWidgetBuilder&;

    // ----- Interactive-row plumbing -------------------------------------------

    /** A fresh RAII scope on the builder's guard; rows capture it BY VALUE so it dies with the widget. */
    auto DoMakeEditScope() const -> TSharedPtr<FCkInspectorEditScope>;

    /** Live enabled-ness of a control under InRequirement, read against the entity Build was given. */
    auto DoMakeGateEnabled(ECk_DebugRequest_Requirement InRequirement) const -> TAttribute<bool>;

    /** InOwnTooltip with the gate's reason appended when there is one. */
    auto DoMakeGateTooltip(
        TAttribute<FText> InOwnTooltip,
        ECk_DebugRequest_Requirement InRequirement) const -> TAttribute<FText>;

    /** Applies the gate (enabled + tooltip) to a composed control in place, and returns it. */
    auto DoApplyGate(
        TSharedRef<SWidget> InControl,
        TAttribute<FText> InOwnTooltip,
        ECk_DebugRequest_Requirement InRequirement) const -> TSharedRef<SWidget>;

    /**
     * Wraps a control for the EditControlStyle axis. Inline returns the control untouched; OnHover
     * returns a hover host that shows InReadOnlyText and swaps in the control while hovered (both
     * children keep their layout footprint, so hovering never re-flows the row).
     */
    static auto DoComposeEditControl(
        TSharedRef<SWidget> InControl,
        TAttribute<FText> InReadOnlyText) -> TSharedRef<SWidget>;

    TArray<FRowDefinition> Rows;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;

    // Filled by Build. Interactive rows are composed BEFORE the entity is known, so they capture this
    // box by value and read the handle live — which also keeps the gate honest when the world changes.
    TSharedRef<FCk_Handle> _GateEntity = MakeShared<FCk_Handle>();
};
