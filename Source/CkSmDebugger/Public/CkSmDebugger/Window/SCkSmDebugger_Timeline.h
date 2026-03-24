#pragma once

#include "CkSmDebugger/ViewModel/CkSmDebugger_ViewModel.h"

#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// Timeline bar — renders color-coded segments, scrub cursor, busy-frame markers, pause markers.
// Supports both Live and Scrub view modes.
// --------------------------------------------------------------------------------------------------------------------

class SCkSmDebugger_Timeline : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkSmDebugger_Timeline)
        : _DesiredHeight(40.0f)
        {}
        SLATE_ARGUMENT(float, DesiredHeight)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkSmDebugger_ViewModel> InViewModel) -> void;

    // SWidget
    auto OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InMyCullingRect, FSlateWindowElementList& InOutDrawElements, int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InbParentEnabled) const -> int32 override;
    auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

    auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;

    auto IsInteractable() const -> bool override { return true; }

private:
    auto TimeToX(double InTime, double InViewStart, double InViewDuration, float InWidth) const -> float;
    auto XToTime(float InX, double InViewStart, double InViewDuration, float InWidth) const -> double;
    auto GetViewRange(double& OutStart, double& OutDuration) const -> void;

    auto PaintSegments(const FGeometry& InGeometry, FSlateWindowElementList& InOutDrawElements, int32 InLayerId, const FCkSmDebugger_RunInfo& InRun, double InViewStart, double InViewDuration) const -> void;
    auto PaintBusyFrames(const FGeometry& InGeometry, FSlateWindowElementList& InOutDrawElements, int32 InLayerId, const FCkSmDebugger_RunInfo& InRun, double InViewStart, double InViewDuration) const -> void;
    auto PaintScrubCursor(const FGeometry& InGeometry, FSlateWindowElementList& InOutDrawElements, int32 InLayerId, double InViewStart, double InViewDuration) const -> void;

private:
    TSharedPtr<FCkSmDebugger_ViewModel> _ViewModel;
    float _DesiredHeight = 40.0f;
    bool _IsScrubbing = false;
};

// --------------------------------------------------------------------------------------------------------------------
