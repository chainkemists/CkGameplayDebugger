#include "SCkSchedulerDebugger_Sparkline.h"

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Sparkline::
	Construct(
		const FArguments& InArgs)
	-> void
{
	_TimingHistory = InArgs._TimingHistory;
	_DesiredHeight = InArgs._DesiredHeight;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Sparkline::
	ComputeDesiredSize(
		float InLayoutScaleMultiplier) const
	-> FVector2D
{
	return FVector2D{200.0f, _DesiredHeight};
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Sparkline::
	OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const
	-> int32
{
	const auto GeometrySize = AllottedGeometry.GetLocalSize();
	const auto WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	// ---- Background
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		WhiteBrush,
		ESlateDrawEffect::None,
		FCkSchedulerDebuggerStyle::Color_Background_Dark);

	if (_TimingHistory.Num() < 2)
	{
		return LayerId;
	}

	// ---- Find max for normalization
	auto MaxTimeMs = 0.001;
	for (const auto TimeMs : _TimingHistory)
	{
		MaxTimeMs = FMath::Max(MaxTimeMs, TimeMs);
	}

	const auto NumPoints = _TimingHistory.Num();
	const auto PaddingX = 2.0f;
	const auto PaddingY = 3.0f;
	const auto GraphWidth = GeometrySize.X - PaddingX * 2.0f;
	const auto GraphHeight = GeometrySize.Y - PaddingY * 2.0f;

	// ---- Build line points
	auto LinePoints = TArray<FVector2D>{};
	LinePoints.Reserve(NumPoints);

	for (auto Idx = 0; Idx < NumPoints; ++Idx)
	{
		const auto X = PaddingX + (static_cast<float>(Idx) / static_cast<float>(NumPoints - 1)) * GraphWidth;
		const auto NormalizedY = FMath::Clamp(
			static_cast<float>(_TimingHistory[Idx] / MaxTimeMs), 0.0f, 1.0f);
		const auto Y = PaddingY + (1.0f - NormalizedY) * GraphHeight;

		LinePoints.Add(FVector2D{X, Y});
	}

	// ---- Draw the line with timing-based color from the latest value
	const auto LatestTimeMs = _TimingHistory.Last();
	const auto LineColor = FCkSchedulerDebuggerStyle::Get_TimingColor(LatestTimeMs);

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(),
		LinePoints,
		ESlateDrawEffect::None,
		LineColor,
		true,
		1.5f);

	// ---- Draw a filled area under the line (subtle)
	// Approximate with vertical bars at each data point
	auto FillColor = LineColor;
	FillColor.A = 0.15f;

	const auto BarStepX = GraphWidth / static_cast<float>(NumPoints - 1);
	for (auto Idx = 0; Idx < NumPoints; ++Idx)
	{
		const auto& Point = LinePoints[Idx];
		const auto BarHeight = GeometrySize.Y - Point.Y;

		if (BarHeight <= 0.0f)
		{ continue; }

		const auto BarGeometry = AllottedGeometry.MakeChild(
			FVector2D{FMath::Max(BarStepX, 1.0f), BarHeight},
			FSlateLayoutTransform{FVector2D{Point.X, Point.Y}});

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			BarGeometry.ToPaintGeometry(),
			WhiteBrush,
			ESlateDrawEffect::None,
			FillColor);
	}

	// ---- Draw max time label in top-right corner
	const auto LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 7);
	const auto MaxLabel = FString::Printf(TEXT("%.3f ms"), MaxTimeMs);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 2,
		AllottedGeometry.MakeChild(
			FVector2D{60.0f, 10.0f},
			FSlateLayoutTransform{FVector2D{GeometrySize.X - 62.0f, 1.0f}}).ToPaintGeometry(),
		MaxLabel,
		LabelFont,
		ESlateDrawEffect::None,
		FCkSchedulerDebuggerStyle::Color_Text_Muted);

	// ---- Draw current (latest) value label in bottom-right
	const auto CurrentLabel = FString::Printf(TEXT("%.3f ms"), LatestTimeMs);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 2,
		AllottedGeometry.MakeChild(
			FVector2D{60.0f, 10.0f},
			FSlateLayoutTransform{FVector2D{GeometrySize.X - 62.0f, GeometrySize.Y - 11.0f}}).ToPaintGeometry(),
		CurrentLabel,
		LabelFont,
		ESlateDrawEffect::None,
		FCkSchedulerDebuggerStyle::Get_TimingColor(LatestTimeMs));

	return LayerId + 2;
}

// --------------------------------------------------------------------------------------------------------------------
