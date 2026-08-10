#pragma once

#include "Widgets/SCompoundWidget.h"

class FCkCrowdDebugger_ViewModel;
class SCkDebug_EventLog;

struct FCkCrowdDebugger_AgentSnapshot;

// --------------------------------------------------------------------------------------------------------------------
// Crowd event log.
//
// The panel is a thin feed on top of the shared SCkDebug_EventLog: it watches the collector's
// per-agent path-trouble records (the only event stream CkCrowdDebugger actually collects today)
// and appends one line per NEW event. Nothing here reaches into CkFoundation — every field read is
// already in FCkCrowdDebugger_AgentSnapshot.
//
// No FCk_Handle is retained. Rows carry the entity's type hash as their selection id and the click
// handler resolves it against the view model's live agent list, so a dead PIE registry can't be
// dereferenced through this panel.
// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebugger_EventLogPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_EventLogPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkCrowdDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	~SCkCrowdDebugger_EventLogPanel();

private:
	auto Handle_AgentListChanged(const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents) -> void;
	auto Handle_SessionInvalidated() -> void;
	auto Handle_EntrySelected(int32 InSelectionId) -> void;

	/** Selection id for a row — the same entity type hash the overlay/viewport selection uses. */
	static auto Compute_SelectionId(const FCkCrowdDebugger_AgentSnapshot& InAgent) -> int32;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
	TSharedPtr<SCkDebug_EventLog> _EventLog;

	FDelegateHandle _OnAgentListChangedHandle;
	FDelegateHandle _OnSessionInvalidatedHandle;

	// Last path-trouble timestamp already turned into a line, per agent selection id. Plain data —
	// no handles — so it survives a registry teardown without holding anything alive.
	TMap<int32, double> _LastLoggedEventTimeById;
};

// --------------------------------------------------------------------------------------------------------------------
