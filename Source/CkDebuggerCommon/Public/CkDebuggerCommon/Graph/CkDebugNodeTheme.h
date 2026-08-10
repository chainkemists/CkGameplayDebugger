#pragma once

#include "CkEditorTools/Style/CkStyle.h"

#include "CoreMinimal.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================
// Shared debug node theme — used by GOAP, SM, and future debuggers.
// Controls visual appearance of graph nodes across all Ck debug tools.
//
// Every color stop resolves from a CkStyle:: role, so a palette edit (Editor Preferences -> Ck ->
// Style) moves the graph nodes with the rest of the suite. The roles are resolved per-construction
// rather than cached in statics — CkStyle reads the settings CDO, which is not available at
// static-init time.
// ====================================================================================================================

UENUM()
enum class ECkDebugNodeThemeStyle : uint8
{
	// SM-Style: rounded Body+ColorSpill brushes, soft shadows
	Rounded,

	// Flat: sharp corners, no Body brush, clean minimal look
	Flat,
};

// ====================================================================================================================

struct CKDEBUGGERCOMMON_API FCkDebugNodeTheme
{
	// Identification
	FString Name;
	ECkDebugNodeThemeStyle Style = ECkDebugNodeThemeStyle::Rounded;

	// Node border colors
	FLinearColor ActiveBorder = CkStyle::NodeBorder_InPlan();
	FLinearColor InactiveBorder = CkStyle::NodeBorder_Inactive();
	FLinearColor SelectedBorder = CkStyle::Warn();
	FLinearColor GoalBorder = CkStyle::NodeBorder_Goal();
	FLinearColor GoalText = CkStyle::NodeBorder_Goal();

	// Node fill / background
	FLinearColor ActiveFill = CkStyle::BgRoot();
	FLinearColor InactiveFill = CkStyle::NodeFill_Inactive();

	// Text
	FLinearColor ActiveText = CkStyle::Text();
	FLinearColor InactiveText = CkStyle::TextDim();
	FLinearColor CostText = CkStyle::Warn();

	// Ports
	FLinearColor PreconditionSatisfied = CkStyle::Ok();
	FLinearColor PreconditionUnsatisfied = CkStyle::Err();
	FLinearColor EffectPort = CkStyle::Info();
	FLinearColor PortLabel = CkStyle::TextDim();

	// Edges
	FLinearColor ActiveEdge = CkStyle::Ok();
	FLinearColor InactiveEdge = CkStyle::Graph_Edge();
	float ActiveEdgeThickness = 2.5f;
	float InactiveEdgeThickness = 1.0f;

	// Opacity
	float InactiveAlpha = 0.4f;

	// ----------------------------------------------------------------------------------------------------------------
	// BRUSH HELPERS
	// ----------------------------------------------------------------------------------------------------------------

	auto
	GetBodyBrush() const -> const FSlateBrush*
	{
		if (Style == ECkDebugNodeThemeStyle::Flat)
		{
			return FAppStyle::GetBrush(TEXT("WhiteBrush"));
		}
		return FAppStyle::GetBrush(TEXT("Graph.StateNode.Body"));
	}

	auto
	GetContentBrush() const -> const FSlateBrush*
	{
		if (Style == ECkDebugNodeThemeStyle::Flat)
		{
			return FAppStyle::GetBrush(TEXT("NoBorder"));
		}
		return FAppStyle::GetBrush(TEXT("Graph.StateNode.ColorSpill"));
	}

	auto
	GetBodyPadding() const -> FMargin
	{
		if (Style == ECkDebugNodeThemeStyle::Flat)
		{
			return FMargin(1.5f);
		}
		return FMargin(0.0f);
	}

	// ----------------------------------------------------------------------------------------------------------------
	// PRESETS
	// ----------------------------------------------------------------------------------------------------------------

	static auto Rounded() -> FCkDebugNodeTheme
	{
		auto T = FCkDebugNodeTheme{};
		T.Name = TEXT("Rounded");
		T.Style = ECkDebugNodeThemeStyle::Rounded;
		T.ActiveBorder = CkStyle::NodeBorder_InPlan();
		T.InactiveBorder = CkStyle::NodeBorder_Inactive();
		T.ActiveFill = CkStyle::BgRoot();
		T.InactiveFill = CkStyle::NodeFill_Inactive();
		return T;
	}

	static auto Flat() -> FCkDebugNodeTheme
	{
		auto T = FCkDebugNodeTheme{};
		T.Name = TEXT("Flat");
		T.Style = ECkDebugNodeThemeStyle::Flat;
		T.ActiveBorder = CkStyle::Accent();
		T.InactiveBorder = CkStyle::NodeBorder_Inactive();
		T.ActiveFill = CkStyle::NodeFill_InPlan();
		T.InactiveFill = CkStyle::NodeFill_Inactive();
		T.ActiveText = CkStyle::Text();
		T.InactiveText = CkStyle::TextDim();
		T.CostText = CkStyle::Warn();
		T.ActiveEdge = CkStyle::Accent();
		T.InactiveEdge = CkStyle::Border();
		return T;
	}

	static auto GetTheme(ECkDebugNodeThemeStyle InStyle) -> FCkDebugNodeTheme
	{
		switch (InStyle)
		{
		case ECkDebugNodeThemeStyle::Flat: return Flat();
		default: return Rounded();
		}
	}
};

// ====================================================================================================================
