#include "SCkGoapDebug_PlanStrip.h"

#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SCkGoapDebug_PlanStrip::
	Construct(const FArguments& InArgs)
	-> void
{
	_ViewModel = InArgs._ViewModel;
	_Graph = InArgs._Graph;
	_OnStepClicked = InArgs._OnStepClicked;

	const auto RoundedBrush = CkDebugStyle::GetRoundedBrush();

	ChildSlot
	[
		// No outer fill — editor chrome shows through. The pills provide all
		// the visual contrast.
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
		.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM, CkDebugStyle::SpaceL, CkDebugStyle::SpaceL))
		[
			SNew(SVerticalBox)

			// Head line: Plan · N steps · cost X · Y/N done ——— Goal: <name>
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceM)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("PLAN")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PlanStrip_TitleFontSize()))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::PaneHeadingColor()))
					.TransformPolicy(ETextTransformPolicy::ToUpper)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(_MetaText, STextBlock)
					.Text(FText::FromString(TEXT("no plan")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::PlanStrip_MetaFontSize()))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Goal:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::PlanStrip_GoalLabelFontSize()))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(CkDebugStyle::SpaceS, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(_GoalNameText, STextBlock)
					.Text(FText::FromString(TEXT("—")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PlanStrip_GoalNameFontSize()))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent()))
				]
			]

			// Flow of step pills + arrows + goal pill (horizontal scroll if too long)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				+ SScrollBox::Slot()
				[
					SAssignNew(_FlowBox, SHorizontalBox)
				]
			]
		]
	];
}

// ====================================================================================================================

auto
	SCkGoapDebug_PlanStrip::
	ComputeHash() const
	-> uint32
{
	if (!_ViewModel.IsValid()) { return 0; }
	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info == nullptr) { return 0; }

	auto Hash = uint32{0};
	Hash = HashCombine(Hash, GetTypeHash(Info->PlanActionNames.Num()));
	Hash = HashCombine(Hash, GetTypeHash(Info->PlanCost));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Info->PlanStatus)));
	// Rebuild when the toolbar's Name depth changes so pill labels follow it.
	if (const auto* G = _Graph.Get()) { Hash = HashCombine(Hash, GetTypeHash(G->NameDepth)); }
	for (const auto& Name : Info->PlanActionNames)
	{
		Hash = HashCombine(Hash, GetTypeHash(Name));
	}
	for (const auto& G : Info->Goals)
	{
		if (G.IsActiveGoal)
		{
			Hash = HashCombine(Hash, GetTypeHash(G.ClassName));
			Hash = HashCombine(Hash, GetTypeHash(G.Priority));
		}
	}
	return Hash;
}

auto
	SCkGoapDebug_PlanStrip::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const auto H = ComputeHash();
	if (H != _LastHash)
	{
		_LastHash = H;
		RebuildStrip();
	}
}

// ====================================================================================================================

auto
	SCkGoapDebug_PlanStrip::
	RebuildStrip()
	-> void
{
	if (!_FlowBox.IsValid()) { return; }
	_FlowBox->ClearChildren();

	const auto* Info = _ViewModel.IsValid() ? _ViewModel->Get_CurrentGoapInfo() : nullptr;

	// Resolve the goal up front so the satisfied-goal branch can render it.
	const FCkGoapDebugger_GoalInfo* ActiveGoalEarly = nullptr;
	if (Info != nullptr)
	{
		for (const auto& G : Info->Goals) { if (G.IsActiveGoal) { ActiveGoalEarly = &G; break; } }
		if (ActiveGoalEarly == nullptr && Info->Goals.Num() > 0) { ActiveGoalEarly = &Info->Goals[0]; }
	}

	const auto bTrivialSuccess = Info != nullptr
		&& Info->PlanStatus == ECk_GoapPlanStatus::PlanFound
		&& Info->PlanActionNames.Num() == 0
		&& ActiveGoalEarly != nullptr;

	if (Info == nullptr || (Info->PlanActionNames.Num() == 0 && !bTrivialSuccess))
	{
		if (_MetaText.IsValid())     { _MetaText->SetText(FText::FromString(TEXT("no plan"))); }
		if (_GoalNameText.IsValid()) { _GoalNameText->SetText(FText::FromString(TEXT("—"))); }

		_FlowBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(CkDebugStyle::SpaceM))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("(no plan)")))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", CkDebugStyle::FontSizeBody()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
			];
		return;
	}

	// Trivial success — goal already satisfied, no actions needed. Show just
	// the goal pill plus a "0 actions · goal already satisfied" meta line.
	if (bTrivialSuccess)
	{
		if (_MetaText.IsValid())
		{
			_MetaText->SetText(FText::FromString(TEXT("· 0 actions · goal already satisfied")));
		}
		if (_GoalNameText.IsValid())
		{
			auto* G = _Graph.Get();
			const auto Display = G
				? UCkGoapDebugGraph::ComputeDisplayName(ActiveGoalEarly->ClassName, G->NameDepth)
				: ActiveGoalEarly->ClassName;
			_GoalNameText->SetText(FText::FromString(Display));
		}

		_FlowBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(CkDebugStyle::SpaceM, 0.0f, CkDebugStyle::SpaceL, 0.0f))
			[
				BuildGoalPill(*ActiveGoalEarly)
			];

		_FlowBox->AddSlot().AutoWidth()[ SNew(SBox).WidthOverride(CkDebugStyle::SpaceXL) ];
		return;
	}

	// Find the active goal (first IsActiveGoal); fall back to first goal.
	const FCkGoapDebugger_GoalInfo* ActiveGoal = nullptr;
	for (const auto& G : Info->Goals) { if (G.IsActiveGoal) { ActiveGoal = &G; break; } }
	if (ActiveGoal == nullptr && Info->Goals.Num() > 0) { ActiveGoal = &Info->Goals[0]; }

	// Update meta line. We don't have live "current step" info on the ViewModel
	// — treat step 0 as active; everything after as pending. If you wire a real
	// executing step up later, pass it in and tweak this.
	const auto StepCount = Info->PlanActionNames.Num();
	const auto ActiveIdx = 0;
	if (_MetaText.IsValid())
	{
		_MetaText->SetText(FText::FromString(FString::Printf(
			TEXT("· %d step%s · cost %.0f · step %d / %d"),
			StepCount, StepCount == 1 ? TEXT("") : TEXT("s"),
			Info->PlanCost, ActiveIdx + 1, StepCount)));
	}
	// Condense names via the toolbar's Name-depth control, same as the graph
	// and history rail. Falls through to the raw class name when no graph is
	// bound.
	auto* Graph = _Graph.Get();
	const auto Condense = [Graph](const FString& InClassName) -> FString
	{
		return Graph ? UCkGoapDebugGraph::ComputeDisplayName(InClassName, Graph->NameDepth) : InClassName;
	};

	if (_GoalNameText.IsValid())
	{
		_GoalNameText->SetText(FText::FromString(ActiveGoal ? Condense(ActiveGoal->ClassName) : FString(TEXT("—"))));
	}

	// Step pills + arrows.
	for (auto i = 0; i < StepCount; ++i)
	{
		const auto bDone   = i < ActiveIdx;
		const auto bActive = i == ActiveIdx;

		auto Cost = 0.0f;
		for (const auto& A : Info->Actions)
		{
			if (A.ClassName == Info->PlanActionNames[i]) { Cost = A.Cost; break; }
		}

		_FlowBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(i == 0 ? FMargin(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f) : FMargin(0.0f))
			[
				BuildStepPill(i, Info->PlanActionNames[i], Condense(Info->PlanActionNames[i]), Cost, bDone, bActive)
			];

		// Arrow after each step pill (including the last, which points at the goal).
		_FlowBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				BuildArrow(bDone, bActive)
			];
	}

	// Goal pill at the end.
	if (ActiveGoal != nullptr)
	{
		_FlowBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(CkDebugStyle::SpaceS, 0.0f, CkDebugStyle::SpaceL, 0.0f))
			[
				BuildGoalPill(*ActiveGoal)
			];
	}

	// Trailing spacer so the goal pill's right edge always has breathing
	// room; without it, horizontal scroll looks glued to the pane border.
	_FlowBox->AddSlot().AutoWidth()[ SNew(SBox).WidthOverride(CkDebugStyle::SpaceXL) ];
}

// ====================================================================================================================

auto
	SCkGoapDebug_PlanStrip::
	BuildStepPill(
		int32 InStepIdx,
		const FString& InClassName,
		const FString& InDisplayName,
		float InCost,
		bool bDone,
		bool bActive)
	-> TSharedRef<SWidget>
{
	const auto RoundedBrush = CkDebugStyle::GetRoundedBrush();
	const auto FilledBrush  = CkDebugStyle::GetFilledBrush();

	const auto BorderColor = bActive ? CkDebugStyle::PlanStep_Border_Active()
	                       : bDone   ? CkDebugStyle::PlanStep_Border_Done()
	                                 : CkDebugStyle::PlanStep_Border_Pending();
	const auto FillColor   = bActive ? CkDebugStyle::PlanStep_Fill_Active()
	                       : bDone   ? CkDebugStyle::PlanStep_Fill_Done()
	                                 : CkDebugStyle::PlanStep_Fill_Pending();
	const auto BadgeColor  = bActive ? CkDebugStyle::PlanStep_Badge_Active()
	                       : bDone   ? CkDebugStyle::PlanStep_Badge_Done()
	                                 : CkDebugStyle::PlanStep_Badge_Pending();
	const auto BadgeTextColor = bActive || bDone ? CkDebugStyle::BgRoot() : CkDebugStyle::TextDim();
	const auto StateText = bDone   ? FString(TEXT("done"))
	                    : bActive  ? FString(TEXT("executing"))
	                              : FString(TEXT("pending"));
	// StateColor follows the pill border so "EXECUTING" never looks amber
	// (which belongs to goals) or green (which belongs to the done state).
	const auto StateColor = bActive ? CkDebugStyle::PlanStep_Border_Active()
	                       : bDone   ? CkDebugStyle::Ok()
	                                 : CkDebugStyle::TextMute();

	const auto ClassNameCaptured = InClassName;
	return SNew(SBox)
		.MinDesiredWidth(150.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ContentPadding(FMargin(0.0f))
			.OnClicked_Lambda([this, ClassNameCaptured]()
			{
				_OnStepClicked.ExecuteIfBound(ClassNameCaptured);
				return FReply::Handled();
			})
			[
				// Border + fill nested the same way graph nodes do.
				SNew(SBorder)
				.BorderImage(RoundedBrush)
				.BorderBackgroundColor(FSlateColor(BorderColor))
				.Padding(FMargin(CkDebugStyle::NodeBorderThickness()))
				[
					SNew(SBorder)
					.BorderImage(RoundedBrush)
					.BorderBackgroundColor(FSlateColor(FillColor))
					.Padding(FMargin(CkDebugStyle::SpaceM, CkDebugStyle::SpaceS))
					[
						SNew(SHorizontalBox)

						// Number badge — inline leading column, not a floating
						// overlay. Floating overlays were being clipped by the
						// enclosing button's content rect.
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
						[
							SNew(SBox)
							.WidthOverride(18.0f)
							.HeightOverride(18.0f)
							[
								SNew(SBorder)
								.BorderImage(RoundedBrush)
								.BorderBackgroundColor(FSlateColor(BadgeColor))
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.Padding(FMargin(0.0f))
								[
									SNew(STextBlock)
									.Text(FText::AsNumber(InStepIdx + 1))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall()))
									.ColorAndOpacity(FSlateColor(BadgeTextColor))
								]
							]
						]

						// Name + sub
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(InDisplayName))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PlanStrip_StepNameFontSize()))
								.ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, CkDebugStyle::SpaceXS, 0.0f, 0.0f)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(FText::FromString(FString::Printf(TEXT("$%.0f"), InCost)))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PlanStrip_StepCostFontSize()))
									.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent()))
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(FText::FromString(StateText))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PlanStrip_StepStateFontSize()))
									.ColorAndOpacity(FSlateColor(StateColor))
									.TransformPolicy(ETextTransformPolicy::ToUpper)
								]
							]
						]
					]
				]
			]
		];
}

// ====================================================================================================================

auto
	SCkGoapDebug_PlanStrip::
	BuildArrow(bool bDone, bool bActive)
	-> TSharedRef<SWidget>
{
	const auto Color = bDone   ? CkDebugStyle::Ok()
	                : bActive  ? CkDebugStyle::Accent()
	                          : CkDebugStyle::TextMute();

	return SNew(SBox)
		.WidthOverride(24.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("\u2794")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeH3()))
			.ColorAndOpacity(FSlateColor(Color))
		];
}

// ====================================================================================================================

auto
	SCkGoapDebug_PlanStrip::
	BuildGoalPill(const FCkGoapDebugger_GoalInfo& InGoal)
	-> TSharedRef<SWidget>
{
	const auto RoundedBrush = CkDebugStyle::GetRoundedBrush();

	auto CondStr = FString{};
	for (auto i = 0; i < InGoal.Conditions.Num(); ++i)
	{
		if (i > 0) { CondStr += TEXT(", "); }
		CondStr += InGoal.Conditions[i].AsString();
	}

	auto* Graph = _Graph.Get();
	const auto GoalDisplay = Graph
		? UCkGoapDebugGraph::ComputeDisplayName(InGoal.ClassName, Graph->NameDepth)
		: InGoal.ClassName;

	return SNew(SBox)
		.MinDesiredWidth(200.0f)
		[
			SNew(SBorder)
			.BorderImage(RoundedBrush)
			.BorderBackgroundColor(FSlateColor(CkDebugStyle::PlanStrip_GoalBorder()))
			.Padding(FMargin(CkDebugStyle::NodeBorderThickness()))
			[
				SNew(SBorder)
				.BorderImage(RoundedBrush)
				.BorderBackgroundColor(FSlateColor(CkDebugStyle::PlanStrip_GoalFill()))
				.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("GOAL · PRIORITY %d"), InGoal.Priority)))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeMicro()))
						.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent()))
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 1.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(GoalDisplay))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PlanStrip_GoalNameFontSize()))
						.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent()))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, CkDebugStyle::SpaceS, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(CondStr))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
						.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
					]
				]
			]
		];
}

// ====================================================================================================================
