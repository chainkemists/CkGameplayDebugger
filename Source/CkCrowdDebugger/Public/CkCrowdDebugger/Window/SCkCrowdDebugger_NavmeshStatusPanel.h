#pragma once

#include "Widgets/SCompoundWidget.h"

class FCkCrowdDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebugger_NavmeshStatusPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_NavmeshStatusPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkCrowdDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

private:
	auto Get_NavSystemText() const -> FText;
	auto Get_NavSystemColor() const -> FLinearColor;
	auto Get_NavDataText() const -> FText;
	auto Get_NavDataColor() const -> FLinearColor;
	auto Get_FilterText() const -> FText;
	auto Get_FilterColor() const -> FLinearColor;
	auto Get_SupportedAgentsText() const -> FText;
	auto Get_HealthCheckText() const -> FText;
	auto Get_HealthCheckColor() const -> FLinearColor;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
};

// --------------------------------------------------------------------------------------------------------------------
