#include "SCkGoapDebug_HistoryRail.h"

#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_HistoryRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_RailContainer.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Widgets/Text/STextBlock.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

// ====================================================================================================================

namespace
{
	auto DoGetStatusTone(ECk_GoapPlanStatus InStatus) -> ECkDebug_Tone
	{
		switch (InStatus)
		{
			case ECk_GoapPlanStatus::PlanFound:  return ECkDebug_Tone::Ok;
			case ECk_GoapPlanStatus::PlanFailed: return ECkDebug_Tone::Err;
			case ECk_GoapPlanStatus::Planning:   return ECkDebug_Tone::Info;
			default:                             return ECkDebug_Tone::Neutral;
		}
	}

	auto DoGetStatusText(ECk_GoapPlanStatus InStatus) -> FString
	{
		switch (InStatus)
		{
			case ECk_GoapPlanStatus::PlanFound:  return TEXT("Plan Found");
			case ECk_GoapPlanStatus::PlanFailed: return TEXT("Plan Failed");
			case ECk_GoapPlanStatus::Planning:   return TEXT("Planning");
			default:                             return TEXT("Idle");
		}
	}
}

// ====================================================================================================================

auto
	SCkGoapDebug_HistoryRail::
	Construct(const FArguments& InArgs)
	-> void
{
	_ViewModel = InArgs._ViewModel;
	_Graph = InArgs._Graph;
	_OnScrubChanged = InArgs._OnScrubChanged;

	ChildSlot
	[
		SAssignNew(_Rail, SCkDebug_RailContainer)
		.Title(FText::FromString(TEXT("Plan History")))
	];
}

auto
	SCkGoapDebug_HistoryRail::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(SCkGoapDebuggerWindow::WindowId))
	{ return; }

	if (!_ViewModel.IsValid() || !_Rail.IsValid()) { return; }

	const auto* History = _ViewModel->Get_PlanHistory(_ViewModel->Get_SelectedEntityHandle());
	const auto Count = History ? History->Num() : 0;
	const auto ScrubIdx = _ViewModel->Get_ScrubHistoryIndex();
	const auto IsScrub = _ViewModel->Get_ViewMode() == ECk_GoapDebugger_ViewMode::Scrub;

	auto H = uint32{0};
	H = HashCombine(H, GetTypeHash(Count));
	H = HashCombine(H, GetTypeHash(IsScrub ? ScrubIdx : -1));

	if (H != _LastHash)
	{
		_LastHash = H;
		RebuildRows();
	}
}

// ====================================================================================================================

auto
	SCkGoapDebug_HistoryRail::
	RebuildRows()
	-> void
{
	_Rail->ClearChildren();

	const auto* History = _ViewModel->Get_PlanHistory(_ViewModel->Get_SelectedEntityHandle());
	const auto Count = History ? History->Num() : 0;
	_Rail->Set_CountText(FText::AsNumber(Count));

	if (History == nullptr || Count == 0)
	{
		_Rail->AddChild(
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No plan history yet")))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", CkDebugStyle::FontSizeSmall()))
			.Margin(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
		);
		return;
	}

	const auto ScrubIdx = _ViewModel->Get_ScrubHistoryIndex();
	const auto IsScrub = _ViewModel->Get_ViewMode() == ECk_GoapDebugger_ViewMode::Scrub;
	auto* Graph = _Graph.Get();

	for (auto Idx = History->Num() - 1; Idx >= 0; --Idx)
	{
		const auto& Entry = (*History)[Idx];
		const auto IsSelected = IsScrub && ScrubIdx == Idx;

		// Build the plan chain subtitle: Action1 → Action2 → … [Goal]
		const auto& Snap = Entry.Snapshot;
		auto ChainText = FString{};
		for (auto StepIdx = 0; StepIdx < Snap.PlanActionNames.Num(); ++StepIdx)
		{
			if (StepIdx > 0) { ChainText += TEXT(" \u2192 "); }
			ChainText += Graph
				? UCkGoapDebugGraph::ComputeDisplayName(Snap.PlanActionNames[StepIdx], Graph->NameDepth)
				: Snap.PlanActionNames[StepIdx];
		}
		auto GoalText = FString{};
		for (const auto& Goal : Snap.Goals)
		{
			if (Goal.IsActiveGoal)
			{
				const auto GoalName = Graph
					? UCkGoapDebugGraph::ComputeDisplayName(Goal.ClassName, Graph->NameDepth)
					: Goal.ClassName;
				GoalText = FString::Printf(TEXT(" [%s]"), *GoalName);
				break;
			}
		}
		if (ChainText.IsEmpty())
		{
			ChainText = Entry.FinalStatus == ECk_GoapPlanStatus::PlanFound
				? TEXT("(goal already satisfied)")
				: TEXT("(no plan)");
		}
		ChainText += GoalText;

		const auto ClickedIdx = Idx;
		_Rail->AddChild(
			SNew(SCkDebug_HistoryRow)
			.Tone(DoGetStatusTone(Entry.FinalStatus))
			.TitleText(FText::FromString(FString::Printf(TEXT("%s — %d steps, cost %.0f"),
				*DoGetStatusText(Entry.FinalStatus), Entry.PlanLength, Entry.PlanCost)))
			.RightText(FText::FromString(FString::Printf(TEXT("F#%lld"), Entry.FrameNumber)))
			.SubtitleText(FText::FromString(ChainText))
			.IsSelected(IsSelected)
			.OnClicked_Lambda([this, ClickedIdx]()
			{
				_ViewModel->Set_ScrubHistoryIndex(ClickedIdx);
				if (auto* G = _Graph.Get()) { G->ForceRebuild(); }
				_OnScrubChanged.ExecuteIfBound();
			})
		);
	}
}

// ====================================================================================================================
