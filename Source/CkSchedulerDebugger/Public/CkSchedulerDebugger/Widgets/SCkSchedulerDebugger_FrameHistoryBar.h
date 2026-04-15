#pragma once

#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class FCkSchedulerDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Frame History Bar - thin horizontal strip showing frames as colored columns.
// Left-click/drag to scrub (select) frames. Right-click drag to pan left/right.
// Double-click to return to live. Auto-freezes when a historical frame is selected.
// --------------------------------------------------------------------------------------------------------------------

class SCkSchedulerDebugger_FrameHistoryBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCkSchedulerDebugger_FrameHistoryBar) {}
		SLATE_ARGUMENT(TSharedPtr<FCkSchedulerDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	~SCkSchedulerDebugger_FrameHistoryBar();

	// ---- SWidget

	virtual auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

	virtual auto OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const -> int32 override;

	virtual auto OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
	virtual auto OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
	virtual auto OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
	virtual auto OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
	virtual auto OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) -> FReply override;

	virtual auto IsInteractable() const -> bool override { return true; }
	virtual auto SupportsKeyboardFocus() const -> bool override { return true; }

	// ---- Highlight filter: frames whose processors match this string are
	// outlined in the history bar. Empty string disables highlighting.
	auto Set_HighlightFilter(const FString& InFilter) -> void;

private:
	auto DoComputeFrameOffsetFromPosition(const FGeometry& InGeometry, float InLocalX) const -> int32;
	auto DoScrollToLive(const FGeometry& InGeometry) -> void;

	TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
	FDelegateHandle _DataRefreshedHandle;

	// ---- Scrubbing (left-click drag to select frames)
	bool _IsScrubbing = false;

	// ---- Panning (right-click drag to scroll left/right)
	bool _IsPanning = false;
	float _PanStartX = 0.0f;
	float _PanStartScrollX = 0.0f;

	// ---- Highlight filter
	FString _HighlightFilter;

	// ---- Layout
	mutable float _ScrollOffsetX = 0.0f;
	static constexpr float BarWidth = 5.0f;
	static constexpr float BarGap = 1.0f;
	static constexpr float BarLeftPad = 10.0f;
	static constexpr float BarRightPad = 80.0f;
};

// --------------------------------------------------------------------------------------------------------------------
