#include "CkCrowdDebugger/Window/SCkCrowdDebugger_NavmeshStatusPanel.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkCrowdDebugger/Window/CkCrowdDebugger_PanelAxes.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"

#include "HAL/PlatformTime.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	auto NavRow(
		const FString& InLabel,
		TAttribute<FText> InValueText,
		TAttribute<FLinearColor> InValueColor) -> TSharedRef<SWidget>
	{
		return SNew(SCkDebug_KeyValueRow)
			.KeyText(FText::FromString(InLabel))
			.Tone(ECkDebug_KeyValueTone::Custom)
			.ValueText(InValueText)
			.CustomValueColor(InValueColor);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_NavmeshStatusPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor::Transparent)
		.Padding_Lambda([]() { return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS}); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[
				ck::crowd_debugger_axes::Make_PaneHeading(TEXT("Navmesh Status"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				NavRow(TEXT("NavSystem"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_NavSystemText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_NavSystemColor))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				NavRow(TEXT("NavData"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_NavDataText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_NavDataColor))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				NavRow(TEXT("Filter"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_FilterText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_FilterColor))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				NavRow(TEXT("Supported Agents"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_SupportedAgentsText),
					TAttribute<FLinearColor>(CkStyle::Value_Numeric()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
			[
				NavRow(TEXT("Health Check"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthCheckText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthCheckColor))
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_NavSystemText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("(no view-model)")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)          { return FText::FromString(TEXT("(no PIE world)")); }
	if (NOT Status._NavSystemPresent) { return FText::FromString(TEXT("MISSING")); }
	return FText::FromString(TEXT("UNavigationSystemV1  OK"));
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_NavSystemColor() const -> FLinearColor
{
	if (NOT _ViewModel.IsValid()) { return CkStyle::TextMute(); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)      { return CkStyle::TextMute(); }
	return Status._NavSystemPresent ? CkStyle::Ok() : CkStyle::Err();
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_NavDataText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("—")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)                { return FText::FromString(TEXT("(no PIE world)")); }
	if (Status._NavDataClassName.IsEmpty()) { return FText::FromString(TEXT("(no NavData)")); }
	return FText::FromString(FString::Printf(TEXT("%s  %s"),
		*Status._NavDataClassName,
		Status._DefaultFilterValid ? TEXT("OK") : TEXT("FILTER MISSING")));
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_NavDataColor() const -> FLinearColor
{
	if (NOT _ViewModel.IsValid()) { return CkStyle::TextMute(); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)                { return CkStyle::TextMute(); }
	if (Status._NavDataClassName.IsEmpty()) { return CkStyle::Err(); }
	return Status._DefaultFilterValid ? CkStyle::Ok() : CkStyle::Warn();
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_FilterText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("—")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled) { return FText::FromString(TEXT("(no PIE world)")); }
	return Status._DefaultFilterValid
		? FText::FromString(TEXT("Default filter present"))
		: FText::FromString(TEXT("DefaultQueryFilter is null"));
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_FilterColor() const -> FLinearColor
{
	if (NOT _ViewModel.IsValid()) { return CkStyle::TextMute(); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled) { return CkStyle::TextMute(); }
	return Status._DefaultFilterValid ? CkStyle::Ok() : CkStyle::Err();
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_SupportedAgentsText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("—")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	return FText::AsNumber(Status._SupportedAgents);
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthCheckText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("—")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._HealthCheckRun)
	{ return FText::FromString(TEXT("Not run yet — click 'Run Health Check'")); }

	const auto AgeSec = FPlatformTime::Seconds() - Status._HealthCheckTimestamp;
	if (Status._HealthCheckPassed)
	{
		return FText::FromString(FString::Printf(
			TEXT("PASS  %.2f ms  %d waypoints  (%.1f s ago)"),
			Status._HealthCheckDurationMs,
			Status._HealthCheckWaypoints,
			AgeSec));
	}
	return FText::FromString(FString::Printf(
		TEXT("FAIL  %s  (%.1f s ago)"),
		*Status._HealthCheckFailReason,
		AgeSec));
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthCheckColor() const -> FLinearColor
{
	if (NOT _ViewModel.IsValid()) { return CkStyle::TextMute(); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._HealthCheckRun) { return CkStyle::TextMute(); }
	return Status._HealthCheckPassed ? CkStyle::Ok() : CkStyle::Err();
}

// --------------------------------------------------------------------------------------------------------------------
