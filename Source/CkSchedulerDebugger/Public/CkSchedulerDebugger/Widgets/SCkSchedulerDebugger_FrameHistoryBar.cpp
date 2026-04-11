#include "SCkSchedulerDebugger_FrameHistoryBar.h"

#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_FrameHistoryBar::
	Construct(
		const FArguments& InArgs)
	-> void
{
	_ViewModel = InArgs._ViewModel;
}

// --------------------------------------------------------------------------------------------------------------------

SCkSchedulerDebugger_FrameHistoryBar::~SCkSchedulerDebugger_FrameHistoryBar()
{
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_FrameHistoryBar::
	ComputeDesiredSize(
		float InLayoutScaleMultiplier) const
	-> FVector2D
{
	return FVector2D{800.0f, 44.0f};
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_FrameHistoryBar::
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
		FLinearColor(0.071f, 0.071f, 0.098f));

	if (NOT _ViewModel.IsValid())
	{ return LayerId; }

	const auto& DataCollector = _ViewModel->Get_DataCollector();
	const auto& Snapshots = DataCollector.Get_FrameSnapshots();
	const auto SelectedOffset = _ViewModel->Get_SelectedFrameOffset();

	if (Snapshots.IsEmpty())
	{ return LayerId; }

	const auto BarCount = Snapshots.Num();
	const auto BarStride = BarWidth + BarGap;
	const auto TotalContentWidth = static_cast<float>(BarCount) * BarStride;
	const auto VisibleWidth = GeometrySize.X - BarLeftPad - BarRightPad;

	// ---- Right-align: when content is shorter than visible width, offset so bars
	// anchor to the right edge (LIVE = rightmost). This prevents a gap on the right
	// after world switches when the history buffer is still filling up.
	const auto RightAlignOffset = FMath::Max(0.0f, VisibleWidth - TotalContentWidth);

	// ---- Auto-scroll to keep the latest frame visible when live
	if (SelectedOffset == 0 && TotalContentWidth > VisibleWidth)
	{
		_ScrollOffsetX = TotalContentWidth - VisibleWidth;
	}

	// ---- Clamp scroll
	const auto MaxScroll = FMath::Max(0.0f, TotalContentWidth - VisibleWidth);
	_ScrollOffsetX = FMath::Clamp(_ScrollOffsetX, 0.0f, MaxScroll);

	const auto BarAreaHeight = GeometrySize.Y - 8.0f;

	// ---- Find max time for normalization
	auto MaxTimeMs = 0.5;
	for (const auto& Snapshot : Snapshots)
	{
		MaxTimeMs = FMath::Max(MaxTimeMs, Snapshot.TotalFrameTimeMs);
	}

	// ---- Draw frame bars (only visible ones)
	const auto FirstVisibleIdx = FMath::Max(0, FMath::FloorToInt32(_ScrollOffsetX / BarStride));
	const auto LastVisibleIdx = FMath::Min(BarCount - 1,
		FMath::CeilToInt32((_ScrollOffsetX + VisibleWidth) / BarStride));

	for (auto Idx = FirstVisibleIdx; Idx <= LastVisibleIdx; ++Idx)
	{
		const auto& Snapshot = Snapshots[Idx];
		const auto Offset = BarCount - 1 - Idx;
		const auto IsSelected = (Offset == SelectedOffset);

		const auto NormalizedHeight = FMath::Clamp(
			static_cast<float>(Snapshot.TotalFrameTimeMs / MaxTimeMs), 0.02f, 1.0f);
		const auto BarHeight = NormalizedHeight * BarAreaHeight;
		const auto BarX = BarLeftPad + RightAlignOffset + static_cast<float>(Idx) * BarStride - _ScrollOffsetX;
		const auto BarY = GeometrySize.Y - 4.0f - BarHeight;

		// ---- Selection highlight background
		if (IsSelected)
		{
			const auto HighlightGeometry = AllottedGeometry.MakeChild(
				FVector2D{BarWidth + 4.0f, GeometrySize.Y},
				FSlateLayoutTransform{FVector2D{BarX - 2.0f, 0.0f}});

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				HighlightGeometry.ToPaintGeometry(),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.235f, 0.510f, 0.863f, 0.3f));
		}

		// ---- Bar color based on timing
		auto BarColor = FCkSchedulerDebuggerStyle::Get_TimingColor(Snapshot.TotalFrameTimeMs);
		BarColor.A = IsSelected ? 1.0f : 0.7f;

		const auto BarGeometry = AllottedGeometry.MakeChild(
			FVector2D{BarWidth, BarHeight},
			FSlateLayoutTransform{FVector2D{BarX, BarY}});

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			BarGeometry.ToPaintGeometry(),
			WhiteBrush,
			ESlateDrawEffect::None,
			BarColor);

		// ---- Pump indicator dot at bottom
		if (Snapshot.PumpIterationCount > 0)
		{
			auto PumpColor = FCkSchedulerDebuggerStyle::Color_Pumped;
			PumpColor.A = 0.6f;

			const auto PumpGeometry = AllottedGeometry.MakeChild(
				FVector2D{BarWidth, 2.0f},
				FSlateLayoutTransform{FVector2D{BarX, GeometrySize.Y - 3.0f}});

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				PumpGeometry.ToPaintGeometry(),
				WhiteBrush,
				ESlateDrawEffect::None,
				PumpColor);
		}
	}

	// ---- Selection marker vertical line
	{
		const auto SelIdx = BarCount - 1 - SelectedOffset;
		const auto LineX = BarLeftPad + RightAlignOffset + static_cast<float>(SelIdx) * BarStride + BarWidth * 0.5f - _ScrollOffsetX;

		if (LineX >= BarLeftPad - 5.0f && LineX <= GeometrySize.X - BarRightPad + 5.0f)
		{
			const auto LineGeometry = AllottedGeometry.MakeChild(
				FVector2D{2.0f, GeometrySize.Y},
				FSlateLayoutTransform{FVector2D{LineX - 1.0f, 0.0f}});

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 3,
				LineGeometry.ToPaintGeometry(),
				WhiteBrush,
				ESlateDrawEffect::None,
				FCkSchedulerDebuggerStyle::Color_Selection);

			// ---- Triangle marker at top
			const auto TrianglePoints = TArray<FVector2D>{
				FVector2D{LineX - 4.0f, 0.0f},
				FVector2D{LineX + 4.0f, 0.0f},
				FVector2D{LineX, 6.0f}
			};

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 4,
				AllottedGeometry.ToPaintGeometry(),
				TrianglePoints,
				ESlateDrawEffect::None,
				FCkSchedulerDebuggerStyle::Color_Selection,
				true,
				2.0f);
		}
	}

	// ---- Label text: "LIVE" or "Frame #N (-K)"
	{
		const auto LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
		auto LabelText = FString{};
		auto LabelColor = FLinearColor{};

		if (SelectedOffset == 0)
		{
			LabelText = TEXT("LIVE");
			LabelColor = FCkSchedulerDebuggerStyle::Color_Active;
		}
		else
		{
			const auto SelIdx = BarCount - 1 - SelectedOffset;
			if (Snapshots.IsValidIndex(SelIdx))
			{
				LabelText = FString::Printf(TEXT("Frame #%llu (-%d)"),
					Snapshots[SelIdx].FrameNumber, SelectedOffset);
			}
			else
			{
				LabelText = FString::Printf(TEXT("(-%d)"), SelectedOffset);
			}
			LabelColor = FCkSchedulerDebuggerStyle::Color_Pumped;
		}

		const auto FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const auto TextSize = FontMeasureService->Measure(LabelText, LabelFont);
		const auto TextX = GeometrySize.X - BarRightPad + 8.0f;
		const auto TextY = 2.0f;

		// ---- Text background
		const auto BgGeometry = AllottedGeometry.MakeChild(
			FVector2D{TextSize.X + 8.0f, TextSize.Y + 4.0f},
			FSlateLayoutTransform{FVector2D{TextX - 4.0f, TextY - 1.0f}});

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 4,
			BgGeometry.ToPaintGeometry(),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.071f, 0.071f, 0.098f, 0.85f));

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 5,
			AllottedGeometry.MakeChild(
				TextSize,
				FSlateLayoutTransform{FVector2D{TextX, TextY}}).ToPaintGeometry(),
			LabelText,
			LabelFont,
			ESlateDrawEffect::None,
			LabelColor);
	}

	// ---- "FROZEN" badge when on historical frame
	if (SelectedOffset > 0)
	{
		const auto BadgeFont = FCoreStyle::GetDefaultFontStyle("Bold", 8);
		const auto BadgeText = TEXT("FROZEN");
		const auto FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const auto BadgeSize = FontMeasureService->Measure(BadgeText, BadgeFont);

		const auto BadgeX = BarLeftPad;
		const auto BadgeY = 2.0f;

		const auto BadgeBgGeometry = AllottedGeometry.MakeChild(
			FVector2D{BadgeSize.X + 10.0f, BadgeSize.Y + 4.0f},
			FSlateLayoutTransform{FVector2D{BadgeX, BadgeY}});

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 4,
			BadgeBgGeometry.ToPaintGeometry(),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.937f, 0.325f, 0.314f, 0.2f));

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 5,
			AllottedGeometry.MakeChild(
				BadgeSize,
				FSlateLayoutTransform{FVector2D{BadgeX + 5.0f, BadgeY + 2.0f}}).ToPaintGeometry(),
			BadgeText,
			BadgeFont,
			ESlateDrawEffect::None,
			FCkSchedulerDebuggerStyle::Color_Warning);
	}

	// ---- Overflow indicators (chevrons showing more content left/right)
	{
		const auto IndicatorFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
		const auto IndicatorColor = FCkSchedulerDebuggerStyle::Color_Text_Muted;
		const auto HasMoreLeft = (_ScrollOffsetX > 1.0f);
		const auto HasMoreRight = (_ScrollOffsetX < MaxScroll - 1.0f);

		if (HasMoreLeft)
		{
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId + 6,
				AllottedGeometry.MakeChild(
					FVector2D{10.0f, GeometrySize.Y},
					FSlateLayoutTransform{FVector2D{0.0f, GeometrySize.Y * 0.5f - 8.0f}}).ToPaintGeometry(),
				TEXT("\u25C0"),
				IndicatorFont,
				ESlateDrawEffect::None,
				IndicatorColor);
		}

		if (HasMoreRight)
		{
			const auto ArrowX = GeometrySize.X - BarRightPad - 2.0f;

			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId + 6,
				AllottedGeometry.MakeChild(
					FVector2D{10.0f, GeometrySize.Y},
					FSlateLayoutTransform{FVector2D{ArrowX, GeometrySize.Y * 0.5f - 8.0f}}).ToPaintGeometry(),
				TEXT("\u25B6"),
				IndicatorFont,
				ESlateDrawEffect::None,
				IndicatorColor);
		}
	}

	return LayerId + 6;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_FrameHistoryBar::
	DoComputeFrameOffsetFromPosition(
		const FGeometry& InGeometry,
		float InLocalX) const
	-> int32
{
	if (NOT _ViewModel.IsValid())
	{ return 0; }

	const auto& Snapshots = _ViewModel->Get_DataCollector().Get_FrameSnapshots();
	if (Snapshots.IsEmpty())
	{ return 0; }

	const auto BarCount = Snapshots.Num();
	const auto BarStride = BarWidth + BarGap;
	const auto TotalContentWidth = static_cast<float>(BarCount) * BarStride;
	const auto VisibleWidth = InGeometry.GetLocalSize().X - BarLeftPad - BarRightPad;
	const auto RightAlignOffset = FMath::Max(0.0f, VisibleWidth - TotalContentWidth);

	// Convert screen X to content X (account for scroll offset and right-alignment)
	const auto ContentX = InLocalX - BarLeftPad - RightAlignOffset + _ScrollOffsetX;
	const auto Idx = FMath::FloorToInt32(ContentX / BarStride);
	const auto ClampedIdx = FMath::Clamp(Idx, 0, BarCount - 1);

	return BarCount - 1 - ClampedIdx;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_FrameHistoryBar::
	OnMouseButtonDown(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent)
	-> FReply
{
	const auto LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// ---- Left-click: scrub (select frame)
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		_IsScrubbing = true;

		const auto Offset = DoComputeFrameOffsetFromPosition(MyGeometry, LocalPosition.X);
		if (_ViewModel.IsValid())
		{
			_ViewModel->Set_SelectedFrameOffset(Offset);
		}

		return FReply::Handled().CaptureMouse(SharedThis(const_cast<SCkSchedulerDebugger_FrameHistoryBar*>(this)));
	}

	// ---- Right-click: pan
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		_IsPanning = true;
		_PanStartX = LocalPosition.X;
		_PanStartScrollX = _ScrollOffsetX;

		return FReply::Handled().CaptureMouse(SharedThis(const_cast<SCkSchedulerDebugger_FrameHistoryBar*>(this)));
	}

	return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_FrameHistoryBar::
	OnMouseButtonUp(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent)
	-> FReply
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && _IsScrubbing)
	{
		_IsScrubbing = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && _IsPanning)
	{
		_IsPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_FrameHistoryBar::
	OnMouseMove(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent)
	-> FReply
{
	const auto LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	if (_IsScrubbing)
	{
		const auto Offset = DoComputeFrameOffsetFromPosition(MyGeometry, LocalPosition.X);
		if (_ViewModel.IsValid())
		{
			_ViewModel->Set_SelectedFrameOffset(Offset);
		}
		return FReply::Handled();
	}

	if (_IsPanning)
	{
		const auto DeltaX = LocalPosition.X - _PanStartX;
		_ScrollOffsetX = _PanStartScrollX - DeltaX;
		// Clamping happens in OnPaint
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_FrameHistoryBar::
	OnMouseButtonDoubleClick(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent)
	-> FReply
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{ return FReply::Unhandled(); }

	// Double-click returns to live
	if (_ViewModel.IsValid())
	{
		_ViewModel->Set_SelectedFrameOffset(0);
	}

	return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------
