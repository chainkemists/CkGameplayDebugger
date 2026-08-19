#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"

#include "CoreMinimal.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

struct FSlateDynamicImageBrush;

// --------------------------------------------------------------------------------------------------------------------

/** Draws ONE snapshot's captured image, aspect-fit inside whatever room it is given.
 *
 *  It holds a raw pointer INTO the model's snapshot array, which the window owns. That is safe only because the
 *  window drives both: it calls `Set_Snapshot(nullptr)` before any mutation that can reallocate `_Snapshots`
 *  (capture, delete) and re-points afterwards. A widget that cached the snapshot by value would double the memory
 *  of every stored capture; one that held a shared pointer would keep a deleted snapshot alive behind the reader.
 *
 *  Exactly one decoded brush is cached — the active snapshot's — and it is released before another is built. A
 *  dynamic brush per stored snapshot is a GPU allocation the reader never asked for. */
class CKOPTIMIZATIONDEBUGGER_API SCkOptimizationSnapshotViewer : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkOptimizationSnapshotViewer) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual ~SCkOptimizationSnapshotViewer() override;

public:
    /** Points the viewer at a snapshot the caller keeps alive, or at nothing. Rebuilding the brush is deferred to
     *  the first paint after the id changes, so switching snapshots costs nothing until one is actually shown. */
    auto Set_Snapshot(const FCkOptimizationDebugger_Snapshot* InSnapshot) -> void;

protected:
    virtual auto OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

private:
    auto DoRelease_Brush() -> void;
    auto DoEnsure_Brush() const -> void;

private:
    const FCkOptimizationDebugger_Snapshot* _Snapshot = nullptr;

    // Mutable because the decode happens on the first paint that needs it: `Set_Snapshot` is called from rebuild
    // paths that may point at a snapshot the reader never looks at.
    mutable TSharedPtr<FSlateDynamicImageBrush> _ColorBrush;
    mutable FGuid _BrushSnapshotId;
    mutable bool _BrushDecodeFailed = false;
};

// --------------------------------------------------------------------------------------------------------------------
