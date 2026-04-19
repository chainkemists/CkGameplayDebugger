#include "SCkGoapDebug_PlanStrip.h"

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
	_OnStepClicked = InArgs._OnStepClicked;

	const auto RoundedBrush = FAppStyle::GetBrush(TEXT("RoundedSelection_16x"));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkDebugStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg1))
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
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH4))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::Text))
					.TransformPolicy(ETextTransformPolicy::ToUpper)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(_MetaText, STextBlock)
					.Text(FText::FromString(TEXT("no plan")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeBody))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Goal:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeBody))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(CkDebugStyle::SpaceS, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(_GoalNameText, STextBlock)
					.Text(FText::FromString(TEXT("—")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH3))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent))
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
	if (Info == nullptr || Info->PlanActionNames.Num() == 0)
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
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", CkDebugStyle::FontSizeBody))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute))
			];
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
	if (_GoalNameText.IsValid())
	{
		_GoalNameText->SetText(FText::FromString(ActiveGoal ? ActiveGoal->ClassName : FString(TEXT("—"))));
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
			.Padding(i == 0 ? FMargin(CkDebugStyle::SpaceM, 10.0f, 0.0f, 0.0f) : FMargin(0.0f, 10.0f, 0.0f, 0.0f))
			[
				BuildStepPill(i, Info->PlanActionNames[i], Cost, bDone, bActive)
			];

		// Arrow after each step pill (including the last, which points at the goal).
		_FlowBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
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
			.Padding(FMargin(CkDebugStyle::SpaceS, 10.0f, CkDebugStyle::SpaceM, 0.0f))
			[
				BuildGoalPill(*ActiveGoal)
			];
	}
}

// ====================================================================================================================

auto
	SCkGoapDebug_PlanStrip::
	BuildStepPill(
		int32 InStepIdx,
		const FString& InActionName,
		float InCost,
		bool bDone,
		bool bActive)
	-> TSharedRef<SWidget>
{
	const auto RoundedBrush = FAppStyle::GetBrush(TEXT("RoundedSelection_16x"));
	const auto FilledBrush  = CkDebugStyle::GetFilledBrush();

	const auto BorderColor = bActive ? CkDebugStyle::Accent
	                       : bDone   ? CkDebugStyle::OverlayOf(CkDebugStyle::Ok, 0.55f)
	                                 : CkDebugStyle::Border;
	const auto FillColor   = bActive ? CkDebugStyle::OverlayOf(CkDebugStyle::Accent, 0.10f)
	                       : bDone   ? CkDebugStyle::OverlayOf(CkDebugStyle::Ok,     0.08f)
	                                 : CkDebugStyle::Bg2;
	const auto BadgeColor  = bActive ? CkDebugStyle::Accent
	                       : bDone   ? CkDebugStyle::Ok
	                                 : CkDebugStyle::Bg3;
	const auto BadgeTextColor = bActive || bDone ? CkDebugStyle::BgRoot : CkDebugStyle::TextDim;
	const auto StateText = bDone   ? FString(TEXT("done"))
	                    : bActive  ? FString(TEXT("executing"))
	                              : FString(TEXT("pending"));
	const auto StateColor = bActive ? CkDebugStyle::Accent
	                       : bDone   ? CkDebugStyle::Ok
	                                 : CkDebugStyle::TextMute;

	const auto ClassNameCaptured = InActionName;
	return SNew(SBox)
		.MinDesiredWidth(130.0f)
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
				SNew(SOverlay)

				// Step-number badge floats above the pill's top-left.
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(FMargin(CkDebugStyle::SpaceM, -9.0f, 0.0f, 0.0f))
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
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall))
							.ColorAndOpacity(FSlateColor(BadgeTextColor))
						]
					]
				]

				// Pill body
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(RoundedBrush)
					.BorderBackgroundColor(FSlateColor(BorderColor))
					.Padding(FMargin(1.0f))
					[
						SNew(SBorder)
						.BorderImage(RoundedBrush)
						.BorderBackgroundColor(FSlateColor(FillColor))
						.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, CkDebugStyle::SpaceS, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(InActionName))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeBody))
								.ColorAndOpacity(FSlateColor(CkDebugStyle::Text))
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
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall))
									.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent))
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(FText::FromString(StateText))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeMicro))
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
	const auto Color = bDone   ? CkDebugStyle::Ok
	                : bActive  ? CkDebugStyle::Accent
	                          : CkDebugStyle::TextMute;

	return SNew(SBox)
		.WidthOverride(24.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("\u2794")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeH3))
			.ColorAndOpacity(FSlateColor(Color))
		];
}

// ====================================================================================================================

auto
	SCkGoapDebug_PlanStrip::
	BuildGoalPill(const FCkGoapDebugger_GoalInfo& InGoal)
	-> TSharedRef<SWidget>
{
	const auto RoundedBrush = FAppStyle::GetBrush(TEXT("RoundedSelection_16x"));

	auto CondStr = FString{};
	for (auto i = 0; i < InGoal.Conditions.Num(); ++i)
	{
		if (i > 0) { CondStr += TEXT(", "); }
		CondStr += InGoal.Conditions[i].AsString();
	}

	return SNew(SBox)
		.MinDesiredWidth(200.0f)
		[
			SNew(SBorder)
			.BorderImage(RoundedBrush)
			.BorderBackgroundColor(FSlateColor(CkDebugStyle::Accent))
			.Padding(FMargin(1.5f))
			[
				SNew(SBorder)
				.BorderImage(RoundedBrush)
				.BorderBackgroundColor(FSlateColor(CkDebugStyle::OverlayOf(CkDebugStyle::Accent, 0.14f)))
				.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("GOAL · PRIORITY %d"), InGoal.Priority)))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeMicro))
						.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent))
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 1.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(InGoal.ClassName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH3))
						.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, CkDebugStyle::SpaceS, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(CondStr))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall))
						.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim))
					]
				]
			]
		];
}

// ====================================================================================================================
