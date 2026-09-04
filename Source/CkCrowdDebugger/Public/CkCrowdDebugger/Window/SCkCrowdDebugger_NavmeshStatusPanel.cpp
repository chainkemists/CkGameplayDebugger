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
				NavRow(TEXT("Provider"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_ProviderText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_ProviderColor))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				NavRow(TEXT("Provider Health"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthColor))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				NavRow(TEXT("Surface Revision"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_SurfaceRevisionText),
					TAttribute<FLinearColor>(CkStyle::Value_Numeric()))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				NavRow(TEXT("Bounds"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_BoundsText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_BoundsColor))
			]
			// Everything below reads a Recast navmesh directly, so it shows only while Recast is the
			// provider answering this world. Under any other provider these rows would describe
			// geometry no query goes to.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SCkCrowdDebugger_NavmeshStatusPanel::Get_RecastDetailVisibility))
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceXS)
				[
					ck::crowd_debugger_axes::Make_PaneHeading(TEXT("Recast Detail"))
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

auto SCkCrowdDebugger_NavmeshStatusPanel::Format_ProviderText(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FText
{
	if (NOT InStatus._Sampled) { return FText::FromString(TEXT("(no PIE world)")); }

	const auto* Enum = StaticEnum<ECk_NavSurface_Provider>();
	return FText::FromString(Enum != nullptr
		? Enum->GetNameStringByValue(static_cast<int64>(InStatus._Provider))
		: FString{TEXT("Unknown")});
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Format_HealthText(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FText
{
	if (NOT InStatus._Sampled) { return FText::FromString(TEXT("(no PIE world)")); }

	const auto* Enum = StaticEnum<ECk_NavSurface_ProviderHealth>();
	return FText::FromString(Enum != nullptr
		? Enum->GetNameStringByValue(static_cast<int64>(InStatus._ProviderHealth))
		: FString{TEXT("Unknown")});
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Resolve_HealthColor(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FLinearColor
{
	if (NOT InStatus._Sampled) { return CkStyle::TextMute(); }

	switch (InStatus._ProviderHealth)
	{
		case ECk_NavSurface_ProviderHealth::Ready:    return CkStyle::Ok();
		case ECk_NavSurface_ProviderHealth::Building: return CkStyle::Warn();
		case ECk_NavSurface_ProviderHealth::NoData:
		case ECk_NavSurface_ProviderHealth::Error:    return CkStyle::Err();
	}

	return CkStyle::TextMute();
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Format_SurfaceRevisionText(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FText
{
	if (NOT InStatus._Sampled) { return FText::FromString(TEXT("(no PIE world)")); }
	return FText::AsNumber(InStatus._SurfaceRevision);
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Format_BoundsText(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FText
{
	if (NOT InStatus._Sampled)        { return FText::FromString(TEXT("(no PIE world)")); }
	if (NOT InStatus._NavBoundsValid) { return FText::FromString(TEXT("Unknown")); }

	const auto Size = InStatus._NavBoundsMax - InStatus._NavBoundsMin;
	return FText::FromString(FString::Printf(TEXT("Valid  %.0f x %.0f x %.0f"), Size.X, Size.Y, Size.Z));
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Resolve_BoundsColor(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> FLinearColor
{
	if (NOT InStatus._Sampled) { return CkStyle::TextMute(); }
	return InStatus._NavBoundsValid ? CkStyle::Ok() : CkStyle::Warn();
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Resolve_RecastDetailVisibility(const FCkCrowdDebugger_NavmeshStatus& InStatus) -> EVisibility
{
	return InStatus._Sampled && InStatus._ProviderIsRecast ? EVisibility::Visible : EVisibility::Collapsed;
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_ProviderText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("(no view-model)")); }
	return Format_ProviderText(_ViewModel->Get_NavmeshStatus());
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_ProviderColor() const -> FLinearColor
{
	if (NOT _ViewModel.IsValid()) { return CkStyle::TextMute(); }
	return _ViewModel->Get_NavmeshStatus()._Sampled ? CkStyle::Value_Enum() : CkStyle::TextMute();
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("(no view-model)")); }
	return Format_HealthText(_ViewModel->Get_NavmeshStatus());
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_HealthColor() const -> FLinearColor
{
	if (NOT _ViewModel.IsValid()) { return CkStyle::TextMute(); }
	return Resolve_HealthColor(_ViewModel->Get_NavmeshStatus());
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_SurfaceRevisionText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("(no view-model)")); }
	return Format_SurfaceRevisionText(_ViewModel->Get_NavmeshStatus());
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_BoundsText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("(no view-model)")); }
	return Format_BoundsText(_ViewModel->Get_NavmeshStatus());
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_BoundsColor() const -> FLinearColor
{
	if (NOT _ViewModel.IsValid()) { return CkStyle::TextMute(); }
	return Resolve_BoundsColor(_ViewModel->Get_NavmeshStatus());
}

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_RecastDetailVisibility() const -> EVisibility
{
	if (NOT _ViewModel.IsValid()) { return EVisibility::Collapsed; }
	return Resolve_RecastDetailVisibility(_ViewModel->Get_NavmeshStatus());
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_NavmeshStatusPanel::Get_NavSystemText() const -> FText
{
	if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("(no view-model)")); }
	const auto& Status = _ViewModel->Get_NavmeshStatus();
	if (NOT Status._Sampled)          { return FText::FromString(TEXT("(no PIE world)")); }
	if (NOT Status._NavSystemPresent) { return FText::FromString(TEXT("MISSING")); }
	return FText::FromString(TEXT("OK"));
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
