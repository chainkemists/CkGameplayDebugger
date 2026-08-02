#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkCrowdDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Top-down 2D viewport for the Crowd Debugger. Renders the Recast navmesh polygons as a base layer
// then overlays the path-network ribbons, agents, and selected-agent diagnostics (goal, arrival/orbit rings, turn-radius
// circle, velocity, planned path). Zoom/pan-able; F frames the selected agent, Home fits the navmesh.
// World->screen mapping derives its extent from the navmesh bounding box.
//
// Built in slices: A=canvas, B=framing+agents, C=navmesh polys, D=selected overlay, E=zoom/pan+hotkeys,
// F=agent-focused default framing.
// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebugger_ViewportPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_ViewportPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkCrowdDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual ~SCkCrowdDebugger_ViewportPanel() override;

	virtual auto Tick(
		const FGeometry& AllottedGeometry,
		double InCurrentTime,
		float InDeltaTime) -> void override;

	virtual auto OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const -> int32 override;

	// Left-click selects the nearest agent dot; right/middle-drag pans.
	virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
	virtual auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
	virtual auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
	virtual auto OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
	// F = frame the selected agent; Home = fit the whole navmesh.
	virtual auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;
	virtual auto SupportsKeyboardFocus() const -> bool override { return true; }

private:
	// World XY -> panel-local screen. FitScreen is the auto-fit mapping; ToScreen additionally applies
	// the user zoom/pan. Both use the transform cached by the last OnPaint.
	auto FitScreen(double InX, double InY) const -> FVector2D;
	auto ToScreen(double InX, double InY) const -> FVector2D;
	auto ScreenToWorld(double InScreenX, double InScreenY) const -> FVector2D;
	auto FrameSelectedAgent() -> void;
	auto OnSelectedAgentChanged(FCk_Handle) -> void;
	auto ClearIntentPathFade() -> void;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
	FDelegateHandle _OnFrameRequestedHandle;
	FDelegateHandle _OnSelectedAgentChangedHandle;

	// Value-only history for the selected agent's last pending/failed intent. Never retain the
	// snapshot or its FCk_Handle here: Slate can outlive PIE and therefore the ECS registry.
	FVector _IntentPathFadeAgentPosition = FVector::ZeroVector;
	FVector _IntentPathFadeGoal = FVector::ZeroVector;
	FString _IntentPathFadeLabel;
	FLinearColor _IntentPathFadeColor = FLinearColor::Red;
	double  _IntentPathFadeEventTimeSeconds = -1.0;
	float   _IntentPathFadeAlpha = 0.0f;
	bool    _HasIntentPathFade = false;

	// User view: zoom multiplier + screen-space pan offset on top of the auto-fit. Home resets to 1/0.
	double _Zoom = 1.0;
	double _PanX = 0.0;
	double _PanY = 0.0;
	bool   _IsPanning = false;
	double _LastDragX = 0.0;
	double _LastDragY = 0.0;

	// RMB is command-or-pan: pan only engages past the drag threshold; an
	// under-threshold release commands the selected agent to the point (RTS-style).
	bool   _RmbDown = false;
	double _RmbDownX = 0.0;
	double _RmbDownY = 0.0;

	// Follow mode (default): keep the selected agent centered in a ~_FollowWindowCm window. Wheel
	// resizes the window; pan/Home drop to manual; F re-engages.
	bool   _AutoFrame = true;
	double _FollowWindowCm = 800.0;

	// World->screen transform cached each OnPaint so the click handler hit-tests with the same
	// mapping the dots were drawn with (at most one frame stale).
	mutable bool   _XformValid = false;
	mutable double _XfCenterX = 0.0;
	mutable double _XfCenterY = 0.0;
	mutable double _XfCosY = 1.0;
	mutable double _XfSinY = 0.0;
	mutable double _XfVMinX = 0.0;
	mutable double _XfVMaxY = 0.0;
	mutable double _XfScale = 1.0;
	mutable double _XfOriginX = 0.0;
	mutable double _XfOriginY = 0.0;
	mutable double _XfPanelCx = 0.0;
	mutable double _XfPanelCy = 0.0;
	// Effective zoom/pan actually used this paint (after follow-mode override) — for click hit-testing.
	mutable double _XfZoom = 1.0;
	mutable double _XfPanX = 0.0;
	mutable double _XfPanY = 0.0;
};

// --------------------------------------------------------------------------------------------------------------------
