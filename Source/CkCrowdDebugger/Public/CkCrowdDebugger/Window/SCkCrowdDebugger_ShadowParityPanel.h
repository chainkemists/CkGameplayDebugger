#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebugger_ShadowParityPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_ShadowParityPanel) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	// The diagnostics are PUSHED in rather than pulled through the view model on every read: the value
	// is a whole fragment - a name, a map of per-fixture counters and a list of ids - and an attribute
	// bound to it would copy all three every time a row repaints instead of once per refresh. The rows
	// below read the pushed copy, so they still track without being rebuilt.
	auto Set_Parity(FCkCrowdDebugger_ShadowParity InParity) -> void;

	auto Get_Parity() const -> const FCkCrowdDebugger_ShadowParity& { return _Parity; }

public:
	// Every row is decided by the copied diagnostics alone, so what the panel shows for a given run can
	// be read back with no world, no shadow entity and no Slate tree behind it.
	static auto Format_ActiveFixtureText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText;
	static auto Format_FixtureCountText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText;
	static auto Format_AgreementText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText;
	static auto Resolve_AgreementColor(const FCkCrowdDebugger_ShadowParity& InParity) -> FLinearColor;
	static auto Format_DivergingIdsText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText;
	static auto Resolve_DivergingIdsColor(const FCkCrowdDebugger_ShadowParity& InParity) -> FLinearColor;
	/** One line per fixture, in a fixed order. Held apart from the aggregate row above because a
	 *  run that agrees overall and a run where one fixture carries every disagreement read the same
	 *  from the aggregate, and telling those apart is the whole point of bucketing by fixture. */
	static auto Format_FixtureRowsText(const FCkCrowdDebugger_ShadowParity& InParity) -> FText;

private:
	auto Get_ActiveFixtureText() const -> FText;
	auto Get_ActiveFixtureColor() const -> FLinearColor;
	auto Get_FixtureCountText() const -> FText;
	auto Get_AgreementText() const -> FText;
	auto Get_AgreementColor() const -> FLinearColor;
	auto Get_DivergingIdsText() const -> FText;
	auto Get_DivergingIdsColor() const -> FLinearColor;
	auto Get_FixtureRowsText() const -> FText;

private:
	FCkCrowdDebugger_ShadowParity _Parity;
};

// --------------------------------------------------------------------------------------------------------------------
