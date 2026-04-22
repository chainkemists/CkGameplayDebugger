#include "SCkGoapDebugger_FailureAnalysisPanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

// ====================================================================================================================

auto
	SCkGoapDebugger_FailureAnalysisPanel::
	Construct(const FArguments& InArgs)
	-> void
{
	_ViewModel = InArgs._ViewModel;

	// The outer InspectorPanel owns the heading — this panel just renders
	// its content to stay consistent with the other inspector bodies.
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SAssignNew(_ContentBox, SVerticalBox)
		]
	];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_FailureAnalysisPanel::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(SCkGoapDebuggerWindow::WindowId))
	{ return; }

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();

	// Content hash — rebuild when any tree node changes. The tree encodes all
	// relevant state (goal, per-condition status, per-action recursion), so
	// hashing its flattened form covers every render-affecting field.
	auto Hash = uint32{0};
	if (Info != nullptr)
	{
		const auto& FA = Info->FailureAnalysis;
		Hash = HashCombine(Hash, GetTypeHash(static_cast<int32>(Info->PlanStatus)));
		Hash = HashCombine(Hash, GetTypeHash(FA.ActiveGoalClassName));
		Hash = HashCombine(Hash, GetTypeHash(FA.HasActiveGoal));
		Hash = HashCombine(Hash, GetTypeHash(FA.UnreachableConditionCount));
		Hash = HashCombine(Hash, GetTypeHash(FA.UnsatisfiedConditionCount));
		Hash = HashCombine(Hash, GetTypeHash(FA.DeepUnreachableNodeCount));
		Hash = HashCombine(Hash, GetTypeHash(FA.PlanTree.Num()));
		// Include the search log's size in the hash so the panel rebuilds
		// as new snapshots arrive each frame during planning.
		if (const auto* Log = _ViewModel->Get_SearchLog(Info->Handle))
		{
			Hash = HashCombine(Hash, GetTypeHash(Log->Num()));
			if (Log->Num() > 0)
			{
				Hash = HashCombine(Hash, GetTypeHash(Log->Last().FrameNumber));
				Hash = HashCombine(Hash, GetTypeHash(Log->Last().ClosedSetSize));
			}
		}

		for (const auto& Node : FA.PlanTree)
		{
			Hash = HashCombine(Hash, GetTypeHash(Node.Depth));
			Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Node.Kind)));
			Hash = HashCombine(Hash, GetTypeHash(Node.Condition.Key));
			Hash = HashCombine(Hash, GetTypeHash(Node.Condition.Value));
			Hash = HashCombine(Hash, GetTypeHash(Node.HasCurrentValue));
			Hash = HashCombine(Hash, GetTypeHash(Node.CurrentValue));
			Hash = HashCombine(Hash, GetTypeHash(Node.IsSatisfiedByWorldState));
			Hash = HashCombine(Hash, GetTypeHash(Node.IsUnreachable));
			Hash = HashCombine(Hash, GetTypeHash(Node.CandidateActionCount));
			Hash = HashCombine(Hash, GetTypeHash(Node.ActionClassName));
			Hash = HashCombine(Hash, GetTypeHash(Node.ActionCost));
			Hash = HashCombine(Hash, GetTypeHash(Node.Note));
		}
	}

	if (Hash != _LastContentHash)
	{
		_LastContentHash = Hash;
		RebuildContent();
	}
}

// ====================================================================================================================

auto
	SCkGoapDebugger_FailureAnalysisPanel::
	RebuildContent()
	-> void
{
	_ContentBox->ClearChildren();

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info == nullptr)
	{
		_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No entity selected")))
			.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
		];
		return;
	}

	const auto& FA = Info->FailureAnalysis;

	if (FA.PlanTree.Num() == 0)
	{
		const auto Message = Info->Goals.Num() == 0
			? FString(TEXT("No goals registered"))
			: FString(TEXT("Goal has no conditions"));
		_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Message))
			.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
		];
		return;
	}

	// Header: goal name + active-or-fallback badge + summary counts.
	const auto GoalLabel = FA.HasActiveGoal
		? FString::Printf(TEXT("Goal: %s (active)"), *FA.ActiveGoalClassName)
		: FString::Printf(TEXT("Goal: %s (fallback — no active goal)"), *FA.ActiveGoalClassName);

	_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 2.0f, CkGoapDebuggerStyle::PanelPadding, 4.0f)
	[
		SNew(SEditableText)
		.Text(FText::FromString(GoalLabel))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		.ColorAndOpacity(FLinearColor(0.96f, 0.78f, 0.18f))
		.IsReadOnly(true)
	];

	// Prominent status banner — explain in plain language what the planner is
	// doing and, crucially, why a failure happened. The A* search is
	// time-sliced across frames (ContinueSearch returns InProgress when the
	// per-frame budget is hit, resumes next frame), so "Planning" with
	// budget% > 100 is NOT an error — it's healthy amortization. Only two
	// outcomes are real failures: the open set genuinely exhausted, or the
	// cost threshold was hit.
	struct FStatusBanner
	{
		FString Headline;
		FString Detail;
		FLinearColor Bg;
		FLinearColor Fg;
	};

	const auto Banner = [&]() -> FStatusBanner
	{
		switch (Info->PlanStatus)
		{
		case ECk_GoapPlanStatus::Idle:
			return {
				TEXT("Idle — no plan requested"),
				TEXT(""),
				FLinearColor(0.10f, 0.12f, 0.16f),
				CkGoapDebuggerStyle::TextMuted
			};
		case ECk_GoapPlanStatus::Planning:
			return {
				TEXT("Planning — search in progress, amortizing across frames"),
				TEXT("The A* search is time-sliced. A budget reading above 100% simply means "
				     "total search time has exceeded one frame's budget; the search resumes next frame "
				     "until it finds a plan or exhausts the open set."),
				FLinearColor(0.08f, 0.18f, 0.28f),
				FLinearColor(0.80f, 0.92f, 1.00f)
			};
		case ECk_GoapPlanStatus::PlanFound:
		{
			const auto bTrivial = Info->PlanActionNames.Num() == 0;
			return {
				bTrivial
					? TEXT("Plan found — goal already satisfied (0 actions)")
					: TEXT("Plan found"),
				bTrivial
					? TEXT("The active goal's conditions are already true in the current world state, "
					       "so no actions are required. This is a valid plan — just an empty one.")
					: TEXT(""),
				FLinearColor(0.06f, 0.22f, 0.10f),
				FLinearColor(0.82f, 1.00f, 0.86f)
			};
		}
		case ECk_GoapPlanStatus::PlanFailed:
			return {
				TEXT("Plan FAILED — open set exhausted (genuine infeasibility)"),
				TEXT("A* enumerated every reachable regressive state from the goal and none of them "
				     "were satisfied by the current world state. This is NOT a budget or timeout — "
				     "the registered action set cannot produce a plan from here. Check the tree below "
				     "for conflicts (effect Set values mismatching each other), type mismatches "
				     "(bool/int/enum stored vs compared), or deeper UNREACHABLE leaves not visible "
				     "at the configured depth."),
				FLinearColor(0.30f, 0.06f, 0.06f),
				FLinearColor(1.00f, 0.86f, 0.80f)
			};
		case ECk_GoapPlanStatus::CostThresholdReached:
			return {
				TEXT("Plan FAILED — cost threshold reached"),
				TEXT("A* aborted because the current best f-score exceeded the configured cost cap. "
				     "Either raise FFragment_AStar_Params::_CostThreshold, lower individual action "
				     "costs, or expose cheaper actions. This is different from the per-frame "
				     "time budget (which only suspends the search)."),
				FLinearColor(0.30f, 0.06f, 0.06f),
				FLinearColor(1.00f, 0.86f, 0.80f)
			};
		default:
			return {TEXT("<unknown status>"), TEXT(""), FLinearColor::Black, CkGoapDebuggerStyle::TextMuted};
		}
	}();

	auto BannerBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Banner.Headline))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			.ColorAndOpacity(Banner.Fg)
			.AutoWrapText(true)
		];
	if (NOT Banner.Detail.IsEmpty())
	{
		BannerBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Banner.Detail))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(Banner.Fg)
			.AutoWrapText(true)
		];
	}

	_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 2.0f)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(Banner.Bg)
		.Padding(FMargin(8.0f, 6.0f))
		[
			BannerBox
		]
	];

	// Compact numeric line — unambiguous labels so "budget" doesn't look
	// like a threshold being exceeded when it's amortized across frames.
	const auto StatsLine = FString::Printf(
		TEXT("this-frame: %d iters, %.0f us  \u2022  search state: open %d / closed %d  \u2022  total search time: %.0f%% of one frame-budget"),
		Info->IterationsThisFrame,
		static_cast<double>(Info->TimeThisFrameMicroseconds),
		Info->OpenSetSize,
		Info->ClosedSetSize,
		Info->BudgetUsagePercent);

	_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 0.0f, CkGoapDebuggerStyle::PanelPadding, 4.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(StatsLine))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		.ColorAndOpacity(CkGoapDebuggerStyle::TextSecondary)
		.AutoWrapText(true)
	];

	// Per-frame search log — shows ongoing progress so the dev can tell
	// whether a long-running search is actually converging (closed grows +
	// open stabilizes) or thrashing (open grows unboundedly). Only shown
	// when there are snapshots to display — avoids cluttering idle entities.
	const auto* Log = _ViewModel->Get_SearchLog(Info->Handle);
	if (Log != nullptr && Log->Num() > 0)
	{
		_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 6.0f, CkGoapDebuggerStyle::PanelPadding, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("Search Log (last %d frames of current attempt):"), Log->Num())))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
		];

		// Show the most recent ~20 entries inline. The data collector already
		// caps the full log at SearchLog_MaxEntries; showing all of them
		// inline would swamp the panel height for long searches.
		constexpr auto MaxInline = 20;
		const auto StartIdx = FMath::Max(0, Log->Num() - MaxInline);
		for (auto Idx = StartIdx; Idx < Log->Num(); ++Idx)
		{
			const auto& S = (*Log)[Idx];
			// Delta closed/open from previous snapshot — lets the dev see
			// whether progress is accelerating, stalling, or inflating.
			auto CloseDelta = int32{0};
			auto OpenDelta = int32{0};
			if (Idx > 0)
			{
				CloseDelta = S.ClosedSetSize - (*Log)[Idx - 1].ClosedSetSize;
				OpenDelta = S.OpenSetSize - (*Log)[Idx - 1].OpenSetSize;
			}

			const auto Line = FString::Printf(
				TEXT("F#%lld  iters:%-5d  open:%-6d (%+d)  closed:%-6d (%+d)  %4lld us"),
				S.FrameNumber,
				S.IterationsThisFrame,
				S.OpenSetSize, OpenDelta,
				S.ClosedSetSize, CloseDelta,
				S.TimeThisFrameMicroseconds);

			_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 0.0f, CkGoapDebuggerStyle::PanelPadding, 0.0f)
			[
				SNew(SEditableText)
				.Text(FText::FromString(Line))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextSecondary)
				.IsReadOnly(true)
			];
		}
	}

	// Banner — prioritize the most actionable signal.
	if (FA.UnreachableConditionCount > 0)
	{
		const auto Line = FString::Printf(
			TEXT("\u26A0  %d goal condition(s) UNREACHABLE — no registered action addresses them."),
			FA.UnreachableConditionCount);
		_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 2.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(FLinearColor(0.35f, 0.08f, 0.08f))
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Line))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FLinearColor(1.00f, 0.82f, 0.76f))
				.AutoWrapText(true)
			]
		];
	}
	else if (FA.DeepUnreachableNodeCount > 0)
	{
		const auto Line = FString::Printf(
			TEXT("\u26A0  Goal conditions have candidate actions, but %d dead-end(s) lurk deeper in the precondition chain. "
			     "Look for red UNREACHABLE rows below."),
			FA.DeepUnreachableNodeCount);
		_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 2.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(FLinearColor(0.35f, 0.18f, 0.05f))
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Line))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FLinearColor(1.00f, 0.78f, 0.55f))
				.AutoWrapText(true)
			]
		];
	}
	else if (FA.UnsatisfiedConditionCount > 0
		&& (Info->PlanStatus == ECk_GoapPlanStatus::PlanFailed
		 || Info->PlanStatus == ECk_GoapPlanStatus::CostThresholdReached))
	{
		const auto IsBudget = Info->PlanStatus == ECk_GoapPlanStatus::CostThresholdReached;

		const auto Line = IsBudget
			? FString::Printf(TEXT("\u26A0  Planner hit the cost threshold after %d closed states. "
			                       "Every condition has candidate actions — look for conflicting effects "
			                       "between actions on the same key, or raise the cost threshold."),
			                  Info->ClosedSetSize)
			: FString(TEXT("\u26A0  No dead-end in the reachability tree, yet the plan failed. "
			               "Most likely: two actions set the same key to opposite values, creating a "
			               "conflict the search can't resolve. Inspect the action list for mirrored effects."));

		_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 2.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(FLinearColor(0.25f, 0.18f, 0.05f))
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Line))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(FLinearColor(0.98f, 0.88f, 0.60f))
				.AutoWrapText(true)
			]
		];
	}

	// Flat tree — each node indented by Depth * 16px.
	for (const auto& N : FA.PlanTree)
	{
		const auto Indent = static_cast<float>(N.Depth) * 16.0f;

		if (N.Kind == ECkGoapDebugger_TreeNodeKind::Condition)
		{
			const auto StatusColor = N.IsUnreachable
				? FLinearColor(0.95f, 0.25f, 0.25f)     // red
				: N.IsSatisfiedByWorldState
					? FLinearColor(0.35f, 0.80f, 0.35f) // green
					: FLinearColor(0.95f, 0.75f, 0.20f);// amber

			const auto BadgeText = N.IsUnreachable
				? FString(TEXT("DEAD END"))
				: N.IsSatisfiedByWorldState
					? FString(TEXT("OK"))
					: FString(TEXT("NEEDED"));

			const auto ConditionText = FString::Printf(TEXT("%s = %s"),
				*N.Condition.Key.ToString(),
				N.Condition.Value ? TEXT("true") : TEXT("false"));

			const auto CurrentText = N.HasCurrentValue
				? FString::Printf(TEXT("current: %s"), N.CurrentValue ? TEXT("true") : TEXT("false"))
				: FString(TEXT("current: <unset>"));

			_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding + Indent, 2.0f, CkGoapDebuggerStyle::PanelPadding, 2.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(StatusColor)
					.Padding(FMargin(4.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(BadgeText))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
						.ColorAndOpacity(FLinearColor(0.08f, 0.08f, 0.10f))
					]
				]

				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SEditableText)
					.Text(FText::FromString(ConditionText))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					.ColorAndOpacity(CkGoapDebuggerStyle::TextPrimary)
					.IsReadOnly(true)
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SEditableText)
					.Text(FText::FromString(CurrentText))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(N.HasCurrentValue
						? CkGoapDebuggerStyle::TextSecondary
						: CkGoapDebuggerStyle::TextMuted)
					.IsReadOnly(true)
				]
			];
		}
		else // Action
		{
			// A nameless Action row is the sentinel emitted at the depth cap;
			// render it as just its Note ("(depth limit…)") with no "↳" arrow.
			const auto Label = N.ActionClassName.IsEmpty()
				? N.Note
				: FString::Printf(TEXT("\u21B3 %s  $%.0f%s"),
					*N.ActionClassName,
					N.ActionCost,
					N.Note.IsEmpty() ? TEXT("") : *(FString(TEXT("  ")) + N.Note));

			const auto Color = N.ActionClassName.IsEmpty() || N.Note.Contains(TEXT("cycle"))
				? CkGoapDebuggerStyle::TextMuted
				: FLinearColor(0.72f, 0.85f, 1.00f);

			_ContentBox->AddSlot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding + Indent, 1.0f, CkGoapDebuggerStyle::PanelPadding, 1.0f)
			[
				SNew(SEditableText)
				.Text(FText::FromString(Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(Color)
				.IsReadOnly(true)
			];
		}
	}
}

// ====================================================================================================================
