#pragma once

#include "Framework/Application/IInputProcessor.h"

#include "Math/Vector2D.h"
#include "Templates/SharedPointer.h"

class FCkCrowdDebugger_ViewModel;

// ====================================================================================================================
// RTS-style in-world move command — RMB-click (no drag) on the ground while
// EJECTED commands the crowd debugger's selected agent to that point.
//
// A PASSIVE Slate input pre-processor: it observes RMB down/move/up but never
// consumes — consuming would starve the level viewport's own RMB tracking and
// wedge its camera-look capture. Drags (past the Slate drag threshold) are
// camera-look and are ignored; only a clean click commands.
//
// Gates, evaluated at release time: crowd window open (== processor
// registered), ejected/simulate view active, an agent selected. Commanding
// auto-arms the agent's debug override, so no "Take Control" click is needed.
// Registered by SCkCrowdDebuggerWindow for its lifetime; holds no FCk_Handles
// (the selected agent is re-read from the ViewModel per event) — EndPIE-safe.
// ====================================================================================================================

class FCkCrowdDebugger_WorldCommandProcessor : public IInputProcessor
{
public:
	explicit FCkCrowdDebugger_WorldCommandProcessor(TWeakPtr<FCkCrowdDebugger_ViewModel> InViewModel);

	auto Tick(const float InDeltaTime, FSlateApplication& InSlateApp, TSharedRef<ICursor> InCursor) -> void override {}

	auto HandleMouseButtonDownEvent(FSlateApplication& InSlateApp, const FPointerEvent& InMouseEvent) -> bool override;
	auto HandleMouseButtonUpEvent(FSlateApplication& InSlateApp, const FPointerEvent& InMouseEvent) -> bool override;
	auto HandleMouseMoveEvent(FSlateApplication& InSlateApp, const FPointerEvent& InMouseEvent) -> bool override;

private:
	auto DoTryCommand(const FVector2D& InScreenPos) -> void;

private:
	TWeakPtr<FCkCrowdDebugger_ViewModel> _ViewModel;

	bool      _RmbDown = false;
	bool      _RmbDragged = false;
	FVector2D _RmbDownPos = FVector2D::ZeroVector;
};

// --------------------------------------------------------------------------------------------------------------------
