#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

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

public:
	// The neutral header and the provider detail beneath it are decided by the sampled status alone,
	// so what the panel shows for a given provider can be read back without a live world behind a
	// view-model.
	static auto Format_ProviderText(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FText;
	static auto Format_HealthText(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FText;
	static auto Resolve_HealthColor(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FLinearColor;
	static auto Format_SurfaceRevisionText(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FText;
	static auto Format_BoundsText(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FText;
	static auto Resolve_BoundsColor(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FLinearColor;
	static auto Resolve_RecastDetailVisibility(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> EVisibility;

private:
	auto Get_ProviderText() const -> FText;
	auto Get_ProviderColor() const -> FLinearColor;
	auto Get_HealthText() const -> FText;
	auto Get_HealthColor() const -> FLinearColor;
	auto Get_SurfaceRevisionText() const -> FText;
	auto Get_BoundsText() const -> FText;
	auto Get_BoundsColor() const -> FLinearColor;
	auto Get_RecastDetailVisibility() const -> EVisibility;

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
