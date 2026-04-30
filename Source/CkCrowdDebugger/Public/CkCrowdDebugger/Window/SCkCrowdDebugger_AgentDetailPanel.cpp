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

	_CachedBody = FString::Printf(
		TEXT("Identity\n"
		     "  Handle id    : #%05u\n"
		     "  Primary tag  : %s\n"
		     "  Tag count    : %d\n"
		     "  Radius       : %.1f cm\n"
		     "  Height       : %.1f cm\n"
		     "\n"
		     "Other sections (Position / Velocity / Goal / Path / Steering / Neighbors /\n"
		     "Sleep & Replan) populate as later gates land — see PLAN.md."),
		GetTypeHash(InSnapshot->Handle),
		*InSnapshot->PrimaryTag,
		InSnapshot->Tags.Num(),
		InSnapshot->Radius,
		InSnapshot->Height);
}

// --------------------------------------------------------------------------------------------------------------------
