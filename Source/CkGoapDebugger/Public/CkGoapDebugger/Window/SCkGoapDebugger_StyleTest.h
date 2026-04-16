#pragma once

#include "Widgets/SCompoundWidget.h"
#include "CoreMinimal.h"

// ====================================================================================================================
// Style test panel — shows the same actions rendered in multiple visual styles
// so the user can compare and pick their favorite.
// ====================================================================================================================

class SCkGoapDebugger_StyleTest : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebugger_StyleTest) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

private:
	// Each style builds a row of sample nodes
	auto BuildStyleRow(int32 InStyleIndex, const FString& InLabel) -> TSharedRef<SWidget>;

	// Individual node card in a given style
	auto BuildNode(int32 InStyleIndex, const FString& InName, float InCost, bool InPlan, int32 InStep, bool InDimmed) -> TSharedRef<SWidget>;

	// Arrow between nodes
	auto BuildArrow(bool InPlanEdge) -> TSharedRef<SWidget>;
};

// ====================================================================================================================
