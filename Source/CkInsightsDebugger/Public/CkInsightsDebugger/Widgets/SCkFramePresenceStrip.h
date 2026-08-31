#pragma once

#include "CoreMinimal.h"

#include "CkInsightsAnalyzer/Report/CkFrameReport.h"
#include "CkInsightsAnalyzer/Report/CkMultiFrameReport.h"

#include "Widgets/SLeafWidget.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DELEGATE_OneParam(FOnPresenceStripFrameClicked, uint64 /*FrameIndex*/);
DECLARE_DELEGATE_OneParam(FOnPresenceStripRefineToRuns, const TArray<FCk_FrameRun>& /*Runs*/);

// --------------------------------------------------------------------------------------------------------------------

/**
 * Per-row magnitude presence strip for one merged hot-path node.
 *
 * One slot per analysed-frame ordinal of FCk_MergedHotPathNode::PerFrameInclusiveMs: a frame the
 * node was absent from leaves the track showing through, a frame it was present in paints a bar
 * whose height and opacity scale with its share of MaxInclusiveMs, and a sample at or above
 * P95InclusiveMs paints in the chart's spike red instead of its bar amber. Past one slot per pixel
 * the ordinals bucket — presence is any-present, magnitude is the bucket max, and the bucket goes
 * red if any sample in it crosses P95 — so a slot is never drawn narrower than a pixel.
 *
 * Left-clicking a present slot drills into that one frame; ctrl-clicking anywhere refines the
 * selection to the frames this node was present in. Both leave the click unhandled when they would
 * do nothing, so the owning table row still selects.
 */
class CKINSIGHTSDEBUGGER_API SCkFramePresenceStrip : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkFramePresenceStrip)
    {}
        SLATE_ARGUMENT(TSharedPtr<FCk_MergedHotPathNode>, Node)

        /** Ordinal → real frame index for the whole analysis. Shared by every row of one analysis. */
        SLATE_ARGUMENT(TSharedPtr<TArray<uint64>>, AnalysedFrameIndices)

        SLATE_EVENT(FOnPresenceStripFrameClicked, OnFrameClicked)
        SLATE_EVENT(FOnPresenceStripRefineToRuns, OnRefineToRuns)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // ---- SWidget Interface ----

    virtual auto ComputeDesiredSize(float LayoutScaleMultiplier) const -> FVector2D override;

    virtual auto OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                         const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                         int32 LayerId, const FWidgetStyle& InWidgetStyle,
                         bool bParentEnabled) const -> int32 override;

    virtual auto OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseLeave(const FPointerEvent& MouseEvent) -> void override;

private:
    /** Ordinals in this analysis, or zero when the strip has no data to paint. */
    auto Get_OrdinalCount() const -> int32;

    /** Slots actually painted at this width — never more than one per pixel. */
    auto Get_SlotCount(float InWidth) const -> int32;

    /** Half-open ordinal range a slot covers. */
    auto Get_SlotOrdinals(int32 InSlot, int32 InSlotCount, int32 InOrdinalCount) const -> TPair<int32, int32>;

    /** Slot under a local position, or INDEX_NONE when the strip is empty. */
    auto Get_SlotAt(const FGeometry& InGeometry, const FVector2D& InLocalPosition) const -> int32;

    /** Heaviest present ordinal in a slot, or INDEX_NONE when the whole slot is absent. */
    auto Get_PresentOrdinalInSlot(int32 InSlot, int32 InSlotCount) const -> int32;

    /** First ordinal a slot covers — the frame an absent slot's tooltip names. */
    auto Get_FirstOrdinalInSlot(int32 InSlot, int32 InSlotCount) const -> int32;

    auto DoUpdateTooltip(int32 InSlot, int32 InSlotCount) -> void;
    auto DoRefineToPresentFrames() -> void;

private:
    TSharedPtr<FCk_MergedHotPathNode> _Node;
    TSharedPtr<TArray<uint64>> _AnalysedFrameIndices;

    FOnPresenceStripFrameClicked _OnFrameClicked;
    FOnPresenceStripRefineToRuns _OnRefineToRuns;

    int32 _HoveredSlot = INDEX_NONE;

    TSharedPtr<SToolTip>   _SlotTooltip;
    TSharedPtr<STextBlock> _TooltipTextBlock;
};

// --------------------------------------------------------------------------------------------------------------------
