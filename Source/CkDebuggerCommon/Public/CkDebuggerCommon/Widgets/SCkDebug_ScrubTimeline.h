#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// ====================================================================================================================
// Scrub timeline — a color-coded segment track with a draggable cursor, event marks, and a ruler.
//
// Merges the two scrub tracks the suite grew independently:
//   CkSmDebugger    SCkSmDebugger_Timeline    state segments · per-frame subdivision lines ·
//                                             Live/Scrub modes · pause "cut" marks · bookmarks ·
//                                             cursor + ruler · pan/zoom
//   CkGoapDebugger  SCkGoapDebugger_ScrubTrack  selectable event dots · run bands · "now" bar
//
// Pure data-in: the owner pushes segments + marks through Set_Content and owns mode/time state
// (bound through Mode / ScrubTime / LiveTime and reported through OnScrubbed).
//
// VIEW WINDOW is owned by the WIDGET, not the caller — pan/zoom mutate it directly and report via
// OnViewChanged, so an adopter does not need view state in its view-model. In Live mode the window
// follows LiveTime until the user pans; Focus_OnCursor (or the F key) re-attaches it.
//
// INPUT: LMB drag scrubs (auto-pans at the edges) · LMB on a selectable mark selects it ·
// Ctrl+LMB or RMB/MMB drag pans · wheel zooms about the cursor · F re-centres ·
// RMB WITHOUT a drag opens the "Copy Text" menu when CopyText is set.
// ====================================================================================================================

enum class ECkDebug_ScrubMode : uint8
{
    // Cursor pins to LiveTime and the window follows it.
    Live,

    // Cursor sits at ScrubTime; the window stays where the user left it.
    Scrub,
};

// --------------------------------------------------------------------------------------------------------------------

enum class ECkDebug_ScrubMarkKind : uint8
{
    // Thin full-height tick — a plain "something happened here".
    Tick,

    // Offset half-bars meeting at the midline; reads as a snip across the track (SM pause markers).
    Cut,

    // Tab hanging from the top edge with a guide line below it (SM bookmarks).
    Flag,

    // Filled dot on the track midline; the selectable shape (GOAP history events).
    Dot,
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkDebug_ScrubSegment
{
    double StartSeconds = 0.0;
    double EndSeconds = 0.0;
    FLinearColor Color = FLinearColor::White;

    /** Centred inside the segment when it fits. Shorten composite names before passing them in. */
    FString Label;

    FString Tooltip;

    /**
     * > 1 draws that many equal cells inside the segment, separated by darker boundary lines
     * (the SM timeline's per-frame slicing). Lines are suppressed automatically when the cells
     * would be under ~2px wide.
     */
    int32 SubdivisionCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkDebug_ScrubMark
{
    double TimeSeconds = 0.0;
    ECkDebug_ScrubMarkKind Kind = ECkDebug_ScrubMarkKind::Tick;
    FLinearColor Color = FLinearColor::White;
    FString Tooltip;

    /** >= 0 makes the mark clickable; reported through OnMarkSelected. */
    int32 SelectionId = INDEX_NONE;
};

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DELEGATE_OneParam(FOnCkDebug_ScrubTimelineScrubbed, double /* TimeSeconds */);
DECLARE_DELEGATE_OneParam(FOnCkDebug_ScrubTimelineMarkSelected, int32 /* SelectionId */);
DECLARE_DELEGATE_TwoParams(FOnCkDebug_ScrubTimelineViewChanged, double /* ViewStart */, double /* ViewDuration */);
DECLARE_DELEGATE_RetVal_OneParam(FString, FOnCkDebug_ScrubTimelineFormatTime, double /* TimeSeconds */);

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_ScrubTimeline : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_ScrubTimeline)
        : _DesiredHeight(40.0f)
        , _Mode(ECkDebug_ScrubMode::Live)
        , _ScrubTime(0.0)
        , _LiveTime(0.0)
        , _InitialViewDuration(10.0)
        , _MinViewDuration(0.5)
        , _MaxViewDuration(300.0)
        , _SegmentHeightFraction(0.5f)
        , _ShowRuler(true)
        , _RulerLabelCount(5)
        , _SelectedMarkId(INDEX_NONE)
        , _AllowZoom(true)
    {}
        SLATE_ARGUMENT(float, DesiredHeight)

        SLATE_ATTRIBUTE(ECkDebug_ScrubMode, Mode)
        SLATE_ATTRIBUTE(double, ScrubTime)

        // "Now" — where the Live cursor sits, and the upper clamp for scrubbing.
        SLATE_ATTRIBUTE(double, LiveTime)

        SLATE_ARGUMENT(double, InitialViewDuration)
        SLATE_ARGUMENT(double, MinViewDuration)
        SLATE_ARGUMENT(double, MaxViewDuration)

        // Share of the widget height the segment band occupies; the rest is ruler + labels.
        SLATE_ARGUMENT(float, SegmentHeightFraction)

        SLATE_ARGUMENT(bool, ShowRuler)
        SLATE_ARGUMENT(int32, RulerLabelCount)

        SLATE_ATTRIBUTE(int32, SelectedMarkId)

        // Non-empty enables right-click → Copy on the track (right-DRAG still pans).
        SLATE_ATTRIBUTE(FString, CopyText)

        SLATE_ARGUMENT(bool, AllowZoom)

        // Ruler label text. Default is "%.1fs"; SM-style consumers return "f<frame> [<mod>]".
        SLATE_EVENT(FOnCkDebug_ScrubTimelineFormatTime, OnFormatTime)

        SLATE_EVENT(FOnCkDebug_ScrubTimelineScrubbed, OnScrubbed)
        SLATE_EVENT(FOnCkDebug_ScrubTimelineMarkSelected, OnMarkSelected)
        SLATE_EVENT(FOnCkDebug_ScrubTimelineViewChanged, OnViewChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** Replace the rendered track. Cheap — two array moves; paint is immediate-mode. */
    auto Set_Content(TArray<FCkDebug_ScrubSegment> InSegments, TArray<FCkDebug_ScrubMark> InMarks) -> void;

    /** Explicit window override. Also re-attaches Live follow when the window ends at LiveTime. */
    auto Set_View(double InViewStart, double InViewDuration) -> void;

    auto Get_ViewStart() const -> double { return _ViewStart; }
    auto Get_ViewDuration() const -> double { return _ViewDuration; }

    /** Re-centre the window on the cursor and resume Live follow. Bound to the F key. */
    auto Focus_OnCursor() -> void;

    // ---- SWidget ----------------------------------------------------------------------------------------------------

    auto OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

    auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

    auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseLeave(const FPointerEvent& InMouseEvent) -> void override;
    auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;

    auto IsInteractable() const -> bool override { return true; }
    auto SupportsKeyboardFocus() const -> bool override { return true; }

private:
    auto Compute_X(double InTime, float InWidth) const -> float;
    auto Compute_Time(float InLocalX, float InWidth) const -> double;

    /** Cursor time for the current mode — LiveTime in Live, ScrubTime in Scrub. */
    auto Compute_CursorTime() const -> double;

    /** Nearest selectable mark within the pick radius, or INDEX_NONE. */
    auto Find_MarkAt(const FVector2D& InLocal, const FVector2D& InSize) const -> int32;

    /** Index of the segment containing a time, or INDEX_NONE. */
    auto Find_SegmentAt(double InTime) const -> int32;

    auto Do_ApplyView(double InViewStart, double InViewDuration) -> void;

    /** Live-follow re-anchors the window's right edge on LiveTime. Paint-time, hence const. */
    auto Do_UpdateLiveFollow() const -> void;

    auto Paint_Segments(
        const FGeometry& InGeometry, FSlateWindowElementList& OutDrawElements,
        int32 InLayerId, float InSegmentHeight) const -> void;

    auto Paint_Marks(
        const FGeometry& InGeometry, FSlateWindowElementList& OutDrawElements,
        int32 InLayerId, float InSegmentHeight) const -> void;

    auto Paint_Cursor(
        const FGeometry& InGeometry, FSlateWindowElementList& OutDrawElements, int32 InLayerId) const -> void;

    auto Paint_Ruler(
        const FGeometry& InGeometry, FSlateWindowElementList& OutDrawElements,
        int32 InLayerId, float InSegmentHeight) const -> void;

private:
    TArray<FCkDebug_ScrubSegment> _Segments;
    TArray<FCkDebug_ScrubMark> _Marks;

    float _DesiredHeight = 40.0f;
    TAttribute<ECkDebug_ScrubMode> _Mode;
    TAttribute<double> _ScrubTime;
    TAttribute<double> _LiveTime;
    TAttribute<int32> _SelectedMarkId;
    TAttribute<FString> _CopyText;

    double _MinViewDuration = 0.5;
    double _MaxViewDuration = 300.0;
    float _SegmentHeightFraction = 0.5f;
    bool _ShowRuler = true;
    int32 _RulerLabelCount = 5;
    bool _AllowZoom = true;

    FOnCkDebug_ScrubTimelineFormatTime _OnFormatTime;
    FOnCkDebug_ScrubTimelineScrubbed _OnScrubbed;
    FOnCkDebug_ScrubTimelineMarkSelected _OnMarkSelected;
    FOnCkDebug_ScrubTimelineViewChanged _OnViewChanged;

    // ---- View state. Mutable because Live-follow re-anchors at paint time, where LiveTime is read.
    mutable double _ViewStart = 0.0;
    mutable double _ViewDuration = 10.0;
    mutable bool _FollowLive = true;

    // ---- Interaction state
    bool _IsScrubbing = false;
    bool _IsPanning = false;
    bool _PanWasDragged = false;
    float _PanStartX = 0.0f;
    int32 _HoveredMarkIndex = INDEX_NONE;
    int32 _HoveredSegmentIndex = INDEX_NONE;
};

// ====================================================================================================================
