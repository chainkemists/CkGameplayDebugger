#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentDetailPanel.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;
	_CachedBody = TEXT("Click an agent in the list to see its details here.\n\n"
	                   "Identity is the only section populated in Gate 0; subsequent gates fill in\n"
	                   "Position / Velocity / Goal / Path / Steering / Neighbors / Sleep & Replan.");

	if (_ViewModel.IsValid())
	{
		_OnRefreshedHandle = _ViewModel->OnAgentDataRefreshed.AddSP(
			SharedThis(this), &SCkCrowdDebugger_AgentDetailPanel::OnAgentDataRefreshed);
	}

	ChildSlot
	[
		SNew(SBorder).Padding(FMargin(8, 6))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("AGENT DETAIL")))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(STextBlock)
				.Text(this, &SCkCrowdDebugger_AgentDetailPanel::Get_BodyText)
				.AutoWrapText(true)
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

SCkCrowdDebugger_AgentDetailPanel::~SCkCrowdDebugger_AgentDetailPanel()
{
	if (_ViewModel.IsValid() && _OnRefreshedHandle.IsValid())
	{
		_ViewModel->OnAgentDataRefreshed.Remove(_OnRefreshedHandle);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::Get_BodyText() const -> FText
{
	return FText::FromString(_CachedBody);
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::OnAgentDataRefreshed(
	const FCkCrowdDebugger_AgentSnapshot* InSnapshot)
	-> void
{
	if (InSnapshot == nullptr)
	{
		_CachedBody = TEXT("(no agent selected)\n\n"
		                   "Click an agent in the list to populate this panel.");
		return;
	}

	auto Body = FString::Printf(
		TEXT("Identity\n"
		     "  Handle id    : #%05u\n"
		     "  Primary tag  : %s\n"
		     "  Tag count    : %d\n"
		     "  Radius       : %.1f cm\n"
		     "  Height       : %.1f cm\n"),
		GetTypeHash(InSnapshot->Handle),
		*InSnapshot->PrimaryTag,
		InSnapshot->Tags.Num(),
		InSnapshot->Radius,
		InSnapshot->Height);

	// Gate 3 — Steering Forces. Only the Separation row is wired up in this gate; PathFollow
	// + Pierce + PlayerYield rows land in their respective gates. Magnitude is shown alongside
	// the vector so a quick glance answers "is separation active?" without parsing 3 floats.
	const auto SepMag = InSnapshot->SeparationForce.Size();
	Body += FString::Printf(
		TEXT("\nSteering Forces\n"
		     "  Separation   : (%+7.1f, %+7.1f, %+7.1f)  |%.1f|  weight=%.2f\n"
		     "  PathFollow   : (Gate 3C wires this row up)\n"
		     "  Pierce       : (Gate 4 wires this row up)\n"
		     "  PlayerYield  : (Gate 5 wires this row up)\n"),
		InSnapshot->SeparationForce.X,
		InSnapshot->SeparationForce.Y,
		InSnapshot->SeparationForce.Z,
		SepMag,
		InSnapshot->SeparationWeight);

	// Gate 3 — Neighbors. Header reports count + the radius they're being measured against.
	// Per-row: id (cyan in the mockup, plain text here until the panel gets rich text),
	// distance in cm, relative offset for sanity-checking direction.
	Body += FString::Printf(
		TEXT("\nNeighbors  (count=%d, sep_radius=%.0fcm)\n"),
		InSnapshot->Neighbors.Num(),
		InSnapshot->SeparationRadius);

	if (InSnapshot->Neighbors.Num() == 0)
	{
		Body += TEXT("  (none in range)\n");
	}
	else
	{
		for (const auto& Nbr : InSnapshot->Neighbors)
		{
			Body += FString::Printf(
				TEXT("  #%05u  d=%6.1fcm  off=(%+7.1f, %+7.1f, %+7.1f)\n"),
				GetTypeHash(Nbr.Handle),
				Nbr.Distance,
				Nbr.RelativeOffset.X,
				Nbr.RelativeOffset.Y,
				Nbr.RelativeOffset.Z);
		}
	}

	Body += TEXT("\nOther sections (Position / Velocity / Goal / Path / Sleep & Replan) populate\n"
	             "as later gates land — see PLAN.md.");

	_CachedBody = MoveTemp(Body);
}

// --------------------------------------------------------------------------------------------------------------------
