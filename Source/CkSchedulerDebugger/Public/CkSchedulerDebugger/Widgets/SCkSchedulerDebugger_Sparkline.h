#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// Sparkline - mini line graph showing timing history for a single processor.
// Renders the last N frames of MainPassTimeMs as a line with timing heat coloring.
// --------------------------------------------------------------------------------------------------------------------

class SCkSchedulerDebugger_Sparkline : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCkSchedulerDebugger_Sparkline)
		: _DesiredHeight(40.0f)
	{}
		SLATE_ARGUMENT(TArray<double>, TimingHistory)
		SLATE_ARGUMENT(float, DesiredHeight)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	virtual auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

	virtual auto OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const -> int32 override;

private:
	TArray<double> _TimingHistory;
	float _DesiredHeight = 40.0f;
};

// --------------------------------------------------------------------------------------------------------------------
