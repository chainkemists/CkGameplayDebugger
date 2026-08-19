#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_SnapshotLens.h"

#include "CoreMinimal.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

struct FSlateDynamicImageBrush;

// --------------------------------------------------------------------------------------------------------------------

/** Draws ONE snapshot's captured image, aspect-fit inside whatever room it is given, and — in selection mode —
 *  reports which mesh the cursor is over and which one was clicked.
 *
 *  It holds a raw pointer INTO the model's snapshot array, which the window owns. That is safe only because the
 *  window drives both: it calls `Set_Snapshot(nullptr)` before any mutation that can reallocate `_Snapshots`
 *  (capture, delete) and re-points afterwards. A widget that cached the snapshot by value would double the memory
 *  of every stored capture; one that held a shared pointer would keep a deleted snapshot alive behind the reader.
 *
 *  It never MUTATES the snapshot. Clicks are reported upwards and the window applies them, so there is one
 *  mutation path and one refresh path rather than two that can disagree about what is selected. */
class CKOPTIMIZATIONDEBUGGER_API SCkOptimizationSnapshotViewer : public SCompoundWidget
{
public:
    DECLARE_DELEGATE_OneParam(FOnHoveredPrimChanged, TOptional<int32>);
    DECLARE_DELEGATE_TwoParams(FOnPrimClicked, TOptional<int32>, ECkOptimizationDebugger_SnapshotClickModifier);

    SLATE_BEGIN_ARGS(SCkOptimizationSnapshotViewer) {}
        SLATE_EVENT(FOnHoveredPrimChanged, OnHoveredPrimChanged)
        SLATE_EVENT(FOnPrimClicked, OnPrimClicked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual ~SCkOptimizationSnapshotViewer() override;

public:
    /** Points the viewer at a snapshot the caller keeps alive, or at nothing. Rebuilding the brush is deferred to
     *  the first paint after the id changes, so switching snapshots costs nothing until one is actually shown. */
    auto Set_Snapshot(const FCkOptimizationDebugger_Snapshot* InSnapshot) -> void;

    /** Off by default: with it off, mouse events pass straight through and no ID map is ever decoded. */
    auto Set_InteractionEnabled(bool InEnabled) -> void;

    /** Which of the snapshot's images to draw: the capture, a computed lens, or a stored auxiliary view.
     *
     *  Thresholds arrive WITH the view rather than being read here, because a lens is a measurement and the budgets
     *  it measures against belong to the caller — the same rule every check in this module follows. The brush is
     *  rebuilt on the next paint after a change, and exactly one is ever held. */
    auto Set_View(
        const FCkOptimizationDebugger_SnapshotView& InView,
        const FCkOptimizationDebugger_Thresholds& InThresholds) -> void;

    auto Get_View() const -> const FCkOptimizationDebugger_SnapshotView& { return _View; }

    /** Isolate the selection: everything that is not selected — sky included — dims hard, so a mesh picked out of a
     *  crowded frame can be SEEN rather than merely outlined. Off means the normal hover/selection tint. */
    auto Set_SoloMode(bool InEnabled) -> void;

    /** Pixel counts per prim for the snapshot on screen, computed off the ID map this widget already decodes for
     *  picking. Empty when there is no identification to count. The window reads it rather than decoding a second
     *  copy of a map that is one uint32 per pixel. */
    auto Get_ScreenCoverage() const -> const TArray<int32>&;

    /** Called by the window after IT has changed the selection, since the overlay paints selection as well as
     *  hover and the viewer is deliberately not the thing that knows a click landed. */
    auto Invalidate_Overlay() -> void;

    auto Get_HoveredPrim() const -> TOptional<int32> { return _HoveredPrim; }

protected:
    virtual auto OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

    virtual auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InEvent) -> FReply override;
    virtual auto OnMouseLeave(const FPointerEvent& InEvent) -> void override;

private:
    auto DoRelease_Brushes() -> void;
    auto DoEnsure_ViewBrush() const -> void;
    auto DoEnsure_Overlay() const -> void;
    auto DoEnsure_DecodedIds() const -> void;

    auto TryGet_PrimAt(const FGeometry& InGeometry, const FVector2D& InScreenPosition) const -> TOptional<int32>;

private:
    const FCkOptimizationDebugger_Snapshot* _Snapshot = nullptr;

    FOnHoveredPrimChanged _OnHoveredPrimChanged;
    FOnPrimClicked _OnPrimClicked;

    bool _InteractionEnabled = false;

    FCkOptimizationDebugger_SnapshotView _View;
    FCkOptimizationDebugger_Thresholds _Thresholds;

    bool _SoloMode = false;

    TOptional<int32> _HoveredPrim;

    // Mutable because all three caches are filled on the first paint or interaction that needs them: `Set_Snapshot`
    // is called from rebuild paths that may point at a snapshot the reader never looks at, and decoding an ID map
    // for a snapshot nobody clicks into would be pure waste.
    // ONE brush, for whichever view is active. A viewer that cached a brush per lens would hold eleven full-size
    // textures for a page the reader looks at one view of at a time. The counter is part of the resource NAME for
    // the same reason the overlay's is: a renderer may cache by name, so new bytes need a new name.
    mutable TSharedPtr<FSlateDynamicImageBrush> _ViewBrush;
    mutable FGuid _BrushSnapshotId;
    mutable FCkOptimizationDebugger_SnapshotView _BrushView;
    mutable bool _BrushDecodeFailed = false;
    mutable int32 _BrushCounter = 0;

    mutable TArray<uint32> _DecodedIds;
    mutable TArray<int32> _Coverage;
    mutable FGuid _DecodedIdsSnapshotId;

    // Rebuilt on hover-change, selection-change and snapshot-switch — never per paint, because it costs one pass
    // over every pixel. The counter is part of the resource NAME: a renderer may cache by name, so reusing one
    // name with new bytes is not guaranteed to show the new bytes.
    mutable TSharedPtr<FSlateDynamicImageBrush> _OverlayBrush;
    mutable bool _OverlayDirty = true;
    mutable int32 _OverlayCounter = 0;
};

// --------------------------------------------------------------------------------------------------------------------
