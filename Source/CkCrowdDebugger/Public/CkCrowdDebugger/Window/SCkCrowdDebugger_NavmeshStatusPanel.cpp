#include "CkCrowdDebugger/Window/SCkCrowdDebugger_NavmeshStatusPanel.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"
#include "CkCrowdDebugger/CkCrowdDebuggerStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	auto BuildRow(
		const FString& InLabel,
		TAttribute<FText> InValueText,
		TAttribute<FSlateColor> InValueColor) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InLabel))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
				.MinDesiredWidth(110.0f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InValueText)
				.ColorAndOpacity(InValueColor)
			];
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_NavmeshStatusPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	ChildSlot
	[
		SNew(SBorder).Padding(FMargin(8, 6))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("NAVMESH STATUS")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				BuildRow(TEXT("NavSystem"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_NavSystemText),
					TAttribute<FSlateColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_NavSystemColor))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				BuildRow(TEXT("NavData"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_NavDataText),
					TAttribute<FSlateColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_NavDataColor))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				BuildRow(TEXT("Filter"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_FilterText),
					TAttribute<FSlateColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_FilterColor))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				BuildRow(TEXT("Supported Agents"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_SupportedAgentsText),
					TAttribute<FSlateColor>::Create([]() { return FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)); }))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)
			[
				BuildRow(TEXT("Health Check"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthCheckText),
					TAttribute<FSlateColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthCheckColor))
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_NavSystemText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("(no view-model)")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)              { return FText::FromString(TEXT("(no PIE world)")); }
	if (NOT Status._NavSystemPresent)     { return FText::FromString(TEXT("MISSING")); }
	return FText::FromString(TEXT("UNavigationSystemV1  OK"));
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_NavSystemColor() const -> FSlateColor
{
	if (NOT _ViewModel.IsValid()) { return FSlateColor(CkCrowdDebuggerStyle::StatusAsleep); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)              { return FSlateColor(CkCrowdDebuggerStyle::StatusAsleep); }
	return Status._NavSystemPresent
		? FSlateColor(CkCrowdDebuggerStyle::StatusOk)
		: FSlateColor(CkCrowdDebuggerStyle::StatusError);
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_NavDataText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("—")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)              { return FText::FromString(TEXT("(no PIE world)")); }
	if (Status._NavDataClassName.IsEmpty()) { return FText::FromString(TEXT("(no NavData)")); }
	return FText::FromString(FString::Printf(TEXT("%s  %s"),
		*Status._NavDataClassName,
		Status._DefaultFilterValid ? TEXT("OK") : TEXT("FILTER MISSING")));
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_NavDataColor() const -> FSlateColor
{
	if (NOT _ViewModel.IsValid()) { return FSlateColor(CkCrowdDebuggerStyle::StatusAsleep); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)              { return FSlateColor(CkCrowdDebuggerStyle::StatusAsleep); }
	if (Status._NavDataClassName.IsEmpty()) { return FSlateColor(CkCrowdDebuggerStyle::StatusError); }
	return Status._DefaultFilterValid
		? FSlateColor(CkCrowdDebuggerStyle::StatusOk)
		: FSlateColor(CkCrowdDebuggerStyle::StatusWarn);
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_FilterText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("—")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)              { return FText::FromString(TEXT("(no PIE world)")); }
	return Status._DefaultFilterValid
		? FText::FromString(TEXT("Default filter present"))
		: FText::FromString(TEXT("DefaultQueryFilter is null"));
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_FilterColor() const -> FSlateColor
{
	if (NOT _ViewModel.IsValid()) { return FSlateColor(CkCrowdDebuggerStyle::StatusAsleep); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)              { return FSlateColor(CkCrowdDebuggerStyle::StatusAsleep); }
	return Status._DefaultFilterValid
		? FSlateColor(CkCrowdDebuggerStyle::StatusOk)
		: FSlateColor(CkCrowdDebuggerStyle::StatusError);
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_SupportedAgentsText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("—")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	return FText::FromString(FString::Printf(TEXT("%d"), Status._SupportedAgents));
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

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthCheckColor() const -> FSlateColor
{
	if (NOT _ViewModel.IsValid()) { return FSlateColor(CkCrowdDebuggerStyle::StatusAsleep); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._HealthCheckRun)
	{ return FSlateColor(CkCrowdDebuggerStyle::StatusAsleep); }
	return Status._HealthCheckPassed
		? FSlateColor(CkCrowdDebuggerStyle::StatusOk)
		: FSlateColor(CkCrowdDebuggerStyle::StatusError);
}

// --------------------------------------------------------------------------------------------------------------------
