#include "CkCrowdDebugger/Window/SCkCrowdDebugger_ShadowParityPanel.h"

#include "CkCrowdDebugger/Window/CkCrowdDebugger_PanelAxes.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_crowd_debugger_shadow_parity_panel
{
	auto ParityRow(
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

	const auto NoDiagnostics = FString{TEXT("(no shadow diagnostics)")};

	// Both providers answered the same way. A comparison where one found a route and the other did not
	// is a disagreement whichever way round it went, so neither one-sided column counts here.
	auto Get_AgreedCount(
		const ck::groundnav::FCk_GroundNav_ShadowFixtureCounters& InCounters) -> int32
	{
		return InCounters._BothSucceeded + InCounters._BothFailed;
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ShadowParityPanel::Construct(const FArguments&) -> void
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor::Transparent)
		.Padding_Lambda([]() { return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS}); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[
				ck::crowd_debugger_axes::Make_PaneHeading(TEXT("Shadow Parity"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ck_crowd_debugger_shadow_parity_panel::ParityRow(TEXT("Active Fixture"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_ShadowParityPanel::Get_ActiveFixtureText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_ShadowParityPanel::Get_ActiveFixtureColor))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ck_crowd_debugger_shadow_parity_panel::ParityRow(TEXT("Fixtures"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_ShadowParityPanel::Get_FixtureCountText),
					TAttribute<FLinearColor>(CkStyle::Value_Numeric()))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ck_crowd_debugger_shadow_parity_panel::ParityRow(TEXT("Agreement"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_ShadowParityPanel::Get_AgreementText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_ShadowParityPanel::Get_AgreementColor))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ck_crowd_debugger_shadow_parity_panel::ParityRow(TEXT("Diverging Ids"),
					TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_ShadowParityPanel::Get_DivergingIdsText),
					TAttribute<FLinearColor>::CreateSP(this, &SCkCrowdDebugger_ShadowParityPanel::Get_DivergingIdsColor))
			]
			// The per-fixture breakdown is one text block rather than a row per fixture: the fixture
			// SET changes as a run opens and closes them, and a rebuilt row list loses hover, scroll
			// and paint continuity every time it does. One block whose text tracks does not.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font_Static(&ck::crowd_debugger_axes::Get_Font_Micro)
				.ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
				.Text(TAttribute<FText>::CreateSP(this, &SCkCrowdDebugger_ShadowParityPanel::Get_FixtureRowsText))
			]
		]
	];
}

auto SCkCrowdDebugger_ShadowParityPanel::Set_Parity(FCkCrowdDebugger_ShadowParity InParity) -> void
{
	_Parity = MoveTemp(InParity);
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ShadowParityPanel::Format_ActiveFixtureText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText
{
	if (NOT InParity._Sampled) { return FText::FromString(ck_crowd_debugger_shadow_parity_panel::NoDiagnostics); }

	// An open fixture and no open fixture are different states of a live run: comparisons still land
	// while none is open, bucketed under the world's map name, and a panel that showed the two alike
	// would report a run in progress as one that never started.
	const auto Active = InParity._Diagnostics.Get_ActiveFixture();
	return Active.IsNone()
		? FText::FromString(TEXT("(none open)"))
		: FText::FromString(Active.ToString());
}

auto SCkCrowdDebugger_ShadowParityPanel::Format_FixtureCountText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText
{
	if (NOT InParity._Sampled) { return FText::FromString(ck_crowd_debugger_shadow_parity_panel::NoDiagnostics); }
	return FText::AsNumber(InParity._Diagnostics.Get_PerFixture().Num());
}

auto SCkCrowdDebugger_ShadowParityPanel::Format_AgreementText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText
{
	if (NOT InParity._Sampled) { return FText::FromString(ck_crowd_debugger_shadow_parity_panel::NoDiagnostics); }

	auto Comparisons = 0;
	auto Agreed = 0;
	for (const auto& Fixture : InParity._Diagnostics.Get_PerFixture())
	{
		Comparisons += Fixture.Value._Comparisons;
		Agreed += ck_crowd_debugger_shadow_parity_panel::Get_AgreedCount(Fixture.Value);
	}

	// Zero comparisons is not agreement. A run that has recorded nothing has nothing to agree about,
	// and reporting it as 0/0 in the same colour as a clean run is the one reading that would matter.
	if (Comparisons == 0) { return FText::FromString(TEXT("(nothing compared yet)")); }

	return FText::FromString(FString::Printf(TEXT("%d / %d"), Agreed, Comparisons));
}

auto SCkCrowdDebugger_ShadowParityPanel::Resolve_AgreementColor(const FCkCrowdDebugger_ShadowParity& InParity) -> FLinearColor
{
	if (NOT InParity._Sampled) { return CkStyle::TextMute(); }

	auto Comparisons = 0;
	auto Agreed = 0;
	for (const auto& Fixture : InParity._Diagnostics.Get_PerFixture())
	{
		Comparisons += Fixture.Value._Comparisons;
		Agreed += ck_crowd_debugger_shadow_parity_panel::Get_AgreedCount(Fixture.Value);
	}

	if (Comparisons == 0) { return CkStyle::TextMute(); }
	return Agreed == Comparisons ? CkStyle::Ok() : CkStyle::Warn();
}

auto SCkCrowdDebugger_ShadowParityPanel::Format_DivergingIdsText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText
{
	if (NOT InParity._Sampled) { return FText::FromString(ck_crowd_debugger_shadow_parity_panel::NoDiagnostics); }

	const auto& Ids = InParity._Diagnostics.Get_DivergingQueryIds();
	if (Ids.IsEmpty()) { return FText::FromString(TEXT("None")); }

	// Named rather than counted: a diverging id is what a developer re-runs, and a count cannot be
	// re-run. Capped because this row is a lead - the shadow report itself prints the whole list.
	constexpr auto MaxNamed = 8;
	auto Named = TArray<FString>{};
	for (auto Index = 0; Index < FMath::Min(Ids.Num(), MaxNamed); ++Index)
	{ Named.Add(Ids[Index].ToString()); }

	const auto Listed = FString::Join(Named, TEXT(", "));
	return FText::FromString(Ids.Num() > MaxNamed
		? FString::Printf(TEXT("%d  %s, +%d more"), Ids.Num(), *Listed, Ids.Num() - MaxNamed)
		: FString::Printf(TEXT("%d  %s"), Ids.Num(), *Listed));
}

auto SCkCrowdDebugger_ShadowParityPanel::Resolve_DivergingIdsColor(const FCkCrowdDebugger_ShadowParity& InParity) -> FLinearColor
{
	if (NOT InParity._Sampled) { return CkStyle::TextMute(); }
	return InParity._Diagnostics.Get_DivergingQueryIds().IsEmpty() ? CkStyle::Ok() : CkStyle::Err();
}

auto SCkCrowdDebugger_ShadowParityPanel::Format_FixtureRowsText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText
{
	if (NOT InParity._Sampled) { return FText::GetEmpty(); }

	const auto& PerFixture = InParity._Diagnostics.Get_PerFixture();
	if (PerFixture.IsEmpty()) { return FText::FromString(TEXT("no fixtures recorded")); }

	// Sorted by name because a TMap hands its rows back in whatever order it stored them, and a block
	// that reshuffles on every refresh reads as churn rather than as data.
	auto Names = TArray<FName>{};
	PerFixture.GenerateKeyArray(Names);
	Names.Sort([](const FName& InLeft, const FName& InRight) { return InLeft.LexicalLess(InRight); });

	auto Lines = TArray<FString>{};
	Lines.Reserve(Names.Num());
	for (const auto& Name : Names)
	{
		const auto& Counters = PerFixture[Name];
		Lines.Add(FString::Printf(TEXT("%s  %d/%d agree  recast-only %d  groundnav-only %d"),
			*Name.ToString(),
			ck_crowd_debugger_shadow_parity_panel::Get_AgreedCount(Counters),
			Counters._Comparisons,
			Counters._RecastOnly,
			Counters._GroundNavOnly));
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_ShadowParityPanel::Get_ActiveFixtureText() const -> FText
{
	return Format_ActiveFixtureText(_Parity);
}

auto SCkCrowdDebugger_ShadowParityPanel::Get_ActiveFixtureColor() const -> FLinearColor
{
	return _Parity._Sampled ? CkStyle::Value_Tag() : CkStyle::TextMute();
}

auto SCkCrowdDebugger_ShadowParityPanel::Get_FixtureCountText() const -> FText
{
	return Format_FixtureCountText(_Parity);
}

auto SCkCrowdDebugger_ShadowParityPanel::Get_AgreementText() const -> FText
{
	return Format_AgreementText(_Parity);
}

auto SCkCrowdDebugger_ShadowParityPanel::Get_AgreementColor() const -> FLinearColor
{
	return Resolve_AgreementColor(_Parity);
}

auto SCkCrowdDebugger_ShadowParityPanel::Get_DivergingIdsText() const -> FText
{
	return Format_DivergingIdsText(_Parity);
}

auto SCkCrowdDebugger_ShadowParityPanel::Get_DivergingIdsColor() const -> FLinearColor
{
	return Resolve_DivergingIdsColor(_Parity);
}

auto SCkCrowdDebugger_ShadowParityPanel::Get_FixtureRowsText() const -> FText
{
	return Format_FixtureRowsText(_Parity);
}

// --------------------------------------------------------------------------------------------------------------------
