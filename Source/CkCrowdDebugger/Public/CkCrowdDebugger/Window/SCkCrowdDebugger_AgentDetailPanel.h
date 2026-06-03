#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"

class FCkCrowdDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebugger_AgentDetailPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_AgentDetailPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkCrowdDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual ~SCkCrowdDebugger_AgentDetailPanel();

private:
	auto OnAgentDataRefreshed(const FCkCrowdDebugger_AgentSnapshot* InSnapshot) -> void;

	// A mono key/value row whose value is bound live to the cached snapshot (shows "—" with no selection).
	auto Kv(const FString& InKey, TFunction<FString()> InValue, FLinearColor InColor) -> TSharedRef<class SWidget>;

	// A live, tone-colored status pill (dot + label) for the selected agent's status.
	auto StatusBadge() -> TSharedRef<class SWidget>;
	auto Status_Text() const -> FText;
	auto Status_Color() const -> FLinearColor;

	// One labeled SSpinBox<float> tuner row. Get reads the cached value; Set is called live as the
	// user drags/types — it updates the cache and pushes every tuner into the selected agent's params.
	auto MakeTunerRow(
		const FString& InLabel,
		TFunction<float()> InGet,
		TFunction<void(float)> InSet,
		float InMin,
		float InMax,
		float InDelta) -> TSharedRef<class SWidget>;

	// Push the cached tuner values into the live selected agent's FCk_Fragment_CrowdAgent_Params.
	auto WriteSelectedParams() -> void;
	auto Get_TunersEnabled() const -> bool;

	// Debug "take control" — toggle the override tag on the selected agent (NPC SM stops fighting).
	auto Toggle_DebugOverride() -> FReply;
	auto Get_HasDebugOverride() const -> bool;
	auto Get_OverrideButtonText() const -> FText;

	// Orbit-diagnosis derivations read from the cached snapshot.
	auto Diag_EffArrival() const -> float;
	auto Diag_Speed() const -> double;
	auto Diag_TurnRadius() const -> double;
	auto Diag_PredictedOrbit() const -> float;
	auto Diag_WillOrbit() const -> bool;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
	FDelegateHandle _OnRefreshedHandle;

	// Latest selected-agent snapshot (copied — the source array rebuilds each tick).
	FCkCrowdDebugger_AgentSnapshot _Snapshot;
	bool _HasSelection = false;

	// Live tuner cache — refreshed from the selected agent each tick, written back on edit.
	float _Tuner_MaxSpeed = 240.0f;
	float _Tuner_MaxTurnRate = 4.0f;
	float _Tuner_MaxAcceleration = 480.0f;
	float _Tuner_ArrivalRadius = 30.0f;
	float _Tuner_SeparationRadius = 100.0f;
};

// --------------------------------------------------------------------------------------------------------------------
