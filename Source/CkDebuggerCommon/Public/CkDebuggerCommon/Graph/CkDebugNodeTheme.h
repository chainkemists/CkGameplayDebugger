#pragma once

#include "CoreMinimal.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================
// Shared debug node theme — used by GOAP, SM, and future debuggers.
// Controls visual appearance of graph nodes across all Ck debug tools.
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
	FLinearColor ActiveBorder = FLinearColor(0.15f, 0.45f, 0.85f);
	FLinearColor InactiveBorder = FLinearColor(0.25f, 0.25f, 0.28f);
	FLinearColor SelectedBorder = FLinearColor(0.96f, 0.62f, 0.04f);

	// Node fill / background
	FLinearColor ActiveFill = FLinearColor(0.02f, 0.02f, 0.03f);
	FLinearColor InactiveFill = FLinearColor(0.04f, 0.04f, 0.04f);

	// Text
	FLinearColor ActiveText = FLinearColor(0.9f, 0.9f, 0.9f);
	FLinearColor InactiveText = FLinearColor(0.45f, 0.45f, 0.45f);
	FLinearColor CostText = FLinearColor(0.96f, 0.62f, 0.04f);

	// Ports
	FLinearColor PreconditionSatisfied = FLinearColor(0.13f, 0.77f, 0.37f);
	FLinearColor PreconditionUnsatisfied = FLinearColor(0.94f, 0.27f, 0.27f);
	FLinearColor EffectPort = FLinearColor(0.15f, 0.45f, 0.85f);
	FLinearColor PortLabel = FLinearColor(0.45f, 0.45f, 0.45f);

	// Edges
	FLinearColor ActiveEdge = FLinearColor(0.13f, 0.77f, 0.37f);
	FLinearColor InactiveEdge = FLinearColor(0.2f, 0.2f, 0.2f);
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
			return FAppStyle::GetBrush(TEXT("WhiteBrush"));
		}
		return FAppStyle::GetBrush(TEXT("Graph.StateNode.ColorSpill"));
	}

	// ----------------------------------------------------------------------------------------------------------------
	// PRESETS
	// ----------------------------------------------------------------------------------------------------------------

	static auto Rounded() -> FCkDebugNodeTheme
	{
		auto T = FCkDebugNodeTheme{};
		T.Name = TEXT("Rounded");
		T.Style = ECkDebugNodeThemeStyle::Rounded;
		T.ActiveBorder = FLinearColor(0.15f, 0.45f, 0.85f);
		T.InactiveBorder = FLinearColor(0.25f, 0.25f, 0.28f);
		T.ActiveFill = FLinearColor(0.02f, 0.02f, 0.03f);
		T.InactiveFill = FLinearColor(0.04f, 0.04f, 0.04f);
		return T;
	}

	static auto Flat() -> FCkDebugNodeTheme
	{
		auto T = FCkDebugNodeTheme{};
		T.Name = TEXT("Flat");
		T.Style = ECkDebugNodeThemeStyle::Flat;
		T.ActiveBorder = FLinearColor(0.3f, 0.5f, 1.0f);
		T.InactiveBorder = FLinearColor(0.2f, 0.2f, 0.22f);
		T.ActiveFill = FLinearColor(0.08f, 0.10f, 0.16f);
		T.InactiveFill = FLinearColor(0.06f, 0.06f, 0.07f);
		T.ActiveText = FLinearColor(0.9f, 0.9f, 0.95f);
		T.InactiveText = FLinearColor(0.4f, 0.4f, 0.4f);
		T.CostText = FLinearColor(1.0f, 0.7f, 0.2f);
		T.ActiveEdge = FLinearColor(0.3f, 0.6f, 1.0f);
		T.InactiveEdge = FLinearColor(0.15f, 0.15f, 0.17f);
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
