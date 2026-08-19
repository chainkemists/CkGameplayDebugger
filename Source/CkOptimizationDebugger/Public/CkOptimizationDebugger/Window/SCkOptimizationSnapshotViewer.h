#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"

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
    auto DoEnsure_ColorBrush() const -> void;
    auto DoEnsure_Overlay() const -> void;
    auto DoEnsure_DecodedIds() const -> void;

    auto TryGet_PrimAt(const FGeometry& InGeometry, const FVector2D& InScreenPosition) const -> TOptional<int32>;

private:
    const FCkOptimizationDebugger_Snapshot* _Snapshot = nullptr;

    FOnHoveredPrimChanged _OnHoveredPrimChanged;
    FOnPrimClicked _OnPrimClicked;

    bool _InteractionEnabled = false;

    TOptional<int32> _HoveredPrim;

    // Mutable because all three caches are filled on the first paint or interaction that needs them: `Set_Snapshot`
    // is called from rebuild paths that may point at a snapshot the reader never looks at, and decoding an ID map
    // for a snapshot nobody clicks into would be pure waste.
    mutable TSharedPtr<FSlateDynamicImageBrush> _ColorBrush;
    mutable FGuid _BrushSnapshotId;
    mutable bool _BrushDecodeFailed = false;

    mutable TArray<uint32> _DecodedIds;
    mutable FGuid _DecodedIdsSnapshotId;

    // Rebuilt on hover-change, selection-change and snapshot-switch — never per paint, because it costs one pass
    // over every pixel. The counter is part of the resource NAME: a renderer may cache by name, so reusing one
    // name with new bytes is not guaranteed to show the new bytes.
    mutable TSharedPtr<FSlateDynamicImageBrush> _OverlayBrush;
    mutable bool _OverlayDirty = true;
    mutable int32 _OverlayCounter = 0;
};

// --------------------------------------------------------------------------------------------------------------------
