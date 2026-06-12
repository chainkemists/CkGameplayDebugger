#pragma once

#include "CoreMinimal.h"

// Editor-only: FConnectionDrawingPolicy + GraphEditor types. GraphEditor is
// gated to bBuildEditor in CkDebuggerCommon.Build.cs, so this type is neither
// available nor needed in non-editor DeveloperTool/cooked targets. Its only
// consumers are the editor-only graph debuggers (Goap / Scheduler / Sm).
#if WITH_EDITOR

#include "ConnectionDrawingPolicy.h"

// --------------------------------------------------------------------------------------------------------------------
// Base connection drawing policy for debug graph visualizations.
// Draws straight lines with rotated arrows instead of UE's default splines.
// Resolves edge geometry to owning-node positions (node-to-node) rather than pin positions.
//
// Subclasses override DetermineWiringStyle for domain-specific wire colors/thickness,
// and optionally DetermineLinkGeometry for specialized geometry resolution (e.g., transition badges).
// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API FCkDebugConnectionPolicyBase : public FConnectionDrawingPolicy
{
public:
	FCkDebugConnectionPolicyBase(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements);

	// FConnectionDrawingPolicy overrides
	virtual auto Draw(
		TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries,
		FArrangedChildren& InArrangedNodes) -> void override;

	virtual auto DetermineLinkGeometry(
		FArrangedChildren& InArrangedNodes,
		TSharedRef<SWidget>& InOutputPinWidget,
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FArrangedWidget*& OutStartWidgetGeometry,
		FArrangedWidget*& OutEndWidgetGeometry) -> void override;

	virtual auto DetermineWiringStyle(
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FConnectionParams& OutParams) -> void override;

	virtual auto DrawSplineWithArrow(
		const FVector2D& InStartPoint,
		const FVector2D& InEndPoint,
		const FConnectionParams& InParams) -> void override;

	virtual auto DrawSplineWithArrow(
		const FGeometry& InStartGeom,
		const FGeometry& InEndGeom,
		const FConnectionParams& InParams) -> void override;

	virtual auto ComputeSplineTangent(
		const FVector2D& InStart,
		const FVector2D& InEnd) const -> FVector2D override;

protected:
	auto DrawLineWithArrow(
		const FVector2D& InStart,
		const FVector2D& InEnd,
		const FConnectionParams& InParams) -> void;

	auto DrawArrow(
		const FVector2D& InPos,
		const FVector2D& InDelta,
		const FConnectionParams& InParams) -> void;

protected:
	TMap<UEdGraphNode*, int32> _NodeWidgetMap;
};

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR
