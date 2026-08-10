#include "CkCrowdDebugger/Window/SCkCrowdDebugger_EventLogPanel.h"

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"
#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkCrowdDebugger/Window/CkCrowdDebugger_PanelAxes.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_crowd_debugger_event_log
{
	// Rolling window. Path trouble is bursty — a blocked doorway can emit one line per agent per
	// replan — so the cap is what keeps the panel readable rather than a scroll-forever transcript.
	constexpr auto MaxEntries = 300;

	// A path-trouble record whose navigation status actually failed is an error; a partial or
	// pending one is a warning. Same split the agent list uses for its status colour.
	auto Get_Tone(const FCkCrowdDebugger_AgentSnapshot& InAgent) -> ECk_Tone
	{
		return InAgent.TroubleNavigationStatus == ECk_Nav_PathStatus::Failed
			? ECk_Tone::Err
			: ECk_Tone::Warn;
	}

	// Short bucket for the leading chip: which layer reported the problem.
	auto Get_Category(const FCkCrowdDebugger_AgentSnapshot& InAgent) -> FString
	{
		return InAgent.HadPathNetworkFailure ? FString{TEXT("SIDEWALK")} : FString{TEXT("NAV")};
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	if (_ViewModel.IsValid())
	{
		_OnAgentListChangedHandle = _ViewModel->OnAgentListChanged.AddSP(
			SharedThis(this), &SCkCrowdDebugger_EventLogPanel::Handle_AgentListChanged);
	}

	_OnSessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
		SharedThis(this), &SCkCrowdDebugger_EventLogPanel::Handle_SessionInvalidated);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
		.Padding_Lambda([]()
		{
			return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS});
		})
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[
				ck::crowd_debugger_axes::Make_PaneHeading(TEXT("Event Log"))
			]

			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SAssignNew(_EventLog, SCkDebug_EventLog)
					.MaxEntries(ck_crowd_debugger_event_log::MaxEntries)
					.ShowTimestamps(true)
					.AutoScroll(true)
					// The panel is live but the feed is narrow: path trouble is the only event stream
					// the crowd collector actually records today. Say so rather than render an
					// unexplained blank list.
					.EmptyText(FText::FromString(
						FString{TEXT("No path-trouble events yet.\n")}
						+ TEXT("Lines land here when an agent's route fails or falls back. Sleep / pierce / ")
						+ TEXT("proxy categories follow once those systems record events of their own.")))
					.SelectedId_Lambda([this]() -> int32
					{
						if (NOT _ViewModel.IsValid())
						{ return INDEX_NONE; }

						const auto Selected = _ViewModel->Get_SelectedHandle();
						return ck::IsValid(Selected)
							? static_cast<int32>(GetTypeHash(Selected))
							: INDEX_NONE;
					})
					.OnEntrySelected(FOnCkDebug_EventLogEntrySelected::CreateSP(
						SharedThis(this), &SCkCrowdDebugger_EventLogPanel::Handle_EntrySelected))
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

SCkCrowdDebugger_EventLogPanel::~SCkCrowdDebugger_EventLogPanel()
{
	if (_ViewModel.IsValid() && _OnAgentListChangedHandle.IsValid())
	{ _ViewModel->OnAgentListChanged.Remove(_OnAgentListChangedHandle); }

	if (_OnSessionInvalidatedHandle.IsValid())
	{ ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_OnSessionInvalidatedHandle); }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Handle_AgentListChanged(
	const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents)
	-> void
{
	if (NOT _EventLog.IsValid())
	{ return; }

	// The broadcast fires every refresh with the SAME retained trouble record until the agent hits a
	// new problem, so the event time is the dedup key: one line per (agent, event), not per tick.
	auto NewEntries = TArray<FCkDebug_EventLogEntry>{};

	for (const auto& Agent : InAgents)
	{
		if (NOT Agent.HasPathTroubleEvent || Agent.PathTroubleEventTimeSeconds < 0.0)
		{ continue; }

		const auto SelectionId = Compute_SelectionId(Agent);

		if (const auto* LastLogged = _LastLoggedEventTimeById.Find(SelectionId);
			LastLogged != nullptr && *LastLogged >= Agent.PathTroubleEventTimeSeconds)
		{ continue; }

		_LastLoggedEventTimeById.Add(SelectionId, Agent.PathTroubleEventTimeSeconds);

		auto Entry = FCkDebug_EventLogEntry{};
		Entry.Message = Agent.OwnerName.IsEmpty()
			? ck::Format_UE(TEXT("{} — {}"), Agent.Handle.Get_Entity(), Agent.PathTroubleSummary)
			: ck::Format_UE(TEXT("{} ({}) — {}"),
				Agent.Handle.Get_Entity(), Agent.OwnerName, Agent.PathTroubleSummary);
		Entry.Category = ck_crowd_debugger_event_log::Get_Category(Agent);
		Entry.Tone = ck_crowd_debugger_event_log::Get_Tone(Agent);
		Entry.TimeSeconds = Agent.PathTroubleEventTimeSeconds;
		Entry.SelectionId = SelectionId;

		NewEntries.Add(MoveTemp(Entry));
	}

	if (NewEntries.IsEmpty())
	{ return; }

	// Append (never Set_Entries) so surviving rows keep their TSharedPtr and the user's selection
	// outlives the refresh — see the list-row contract in CkDebuggerCommon/CLAUDE.md.
	_EventLog->Add_Entries(MoveTemp(NewEntries));
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Handle_SessionInvalidated() -> void
{
	_LastLoggedEventTimeById.Reset();

	if (_EventLog.IsValid())
	{ _EventLog->Clear_Entries(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Handle_EntrySelected(int32 InSelectionId) -> void
{
	if (NOT _ViewModel.IsValid())
	{ return; }

	// Resolve the id against the LIVE agent list rather than a retained handle — the row may name an
	// agent that has since been destroyed, and this panel must never hold a PIE handle.
	for (const auto& Agent : _ViewModel->Get_AllAgents())
	{
		if (Compute_SelectionId(Agent) != InSelectionId)
		{ continue; }

		_ViewModel->Set_SelectedHandle(Agent.Handle);
		ck::DebugSelectionSync::Broadcast(Agent.Handle, TEXT("CrowdDebugger"));
		return;
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Compute_SelectionId(
	const FCkCrowdDebugger_AgentSnapshot& InAgent)
	-> int32
{
	return static_cast<int32>(GetTypeHash(InAgent.Handle));
}

// --------------------------------------------------------------------------------------------------------------------
