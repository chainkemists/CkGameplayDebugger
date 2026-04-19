#include "SGraphNode_GoapGoal.h"
#include "CkGoapDebugNode_Goal.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SGraphNode_GoapGoal::
	Construct(const FArguments& InArgs, UCkGoapDebugNode_Goal* InNode)
	-> void
{
	GraphNode = InNode;
	_GoalNode = InNode;
	UpdateGraphNode();
}

auto
	SGraphNode_GoapGoal::
	UpdateGraphNode()
	-> void
{
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	const auto IsActive = _GoalNode->Get_IsActiveGoal();
	const auto BorderColor = IsActive ? CkDebugStyle::NodeBorder_Goal() : CkDebugStyle::BorderStrong();
	const auto FillColor   = IsActive ? CkDebugStyle::NodeFill_Goal()   : CkDebugStyle::NodeFill_GoalInactive();
	const auto TitleColor  = IsActive ? CkDebugStyle::Accent()          : CkDebugStyle::TextDim();
	const auto Alpha       = IsActive ? 1.0f                            : CkDebugStyle::NodeInactiveOpacity();

	const auto RoundedBrush = CkDebugStyle::GetRoundedBrush();

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	// Conditions list — mono font, single column, centered.
	auto ConditionsList = SNew(SVerticalBox);
	for (const auto& C : _GoalNode->Get_Conditions())
	{
		ConditionsList->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(C.AsString()))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
		];
	}

	const auto GoalDisplayName = _GoalNode->Get_DisplayName().IsEmpty()
		? _GoalNode->Get_GoalName()
		: _GoalNode->Get_DisplayName();

	this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(RightNodeBox, SVerticalBox)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(RoundedBrush)
			.BorderBackgroundColor(FSlateColor(BorderColor))
			.Padding(FMargin(CkDebugStyle::NodeBorderThickness()))
			.Visibility(EVisibility::SelfHitTestInvisible)
			.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, Alpha))
			[
				SNew(SBorder)
				.BorderImage(RoundedBrush)
				.BorderBackgroundColor(FSlateColor(FillColor))
				.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
				[
					SNew(SVerticalBox)

					// Target pill: "● GOAL · priority N"
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("GOAL · PRIORITY %d"), _GoalNode->Get_Priority())))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeMicro()))
						.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent()))
						.TransformPolicy(ETextTransformPolicy::ToUpper)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, CkDebugStyle::SpaceXS, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(GoalDisplayName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH3()))
						.ColorAndOpacity(FSlateColor(TitleColor))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, CkDebugStyle::SpaceM, 0.0f, 0.0f)
					[
						ConditionsList
					]
				]
			]
		]
	];

	CreatePinWidgets();
}

auto
	SGraphNode_GoapGoal::
	CreatePinWidgets()
	-> void
{
	for (auto* Pin : GraphNode->Pins)
	{
		auto PinWidget = SNew(SGraphPin, Pin);
		PinWidget->SetOwner(SharedThis(this));
		PinWidget->SetVisibility(EVisibility::HitTestInvisible);
		RightNodeBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[PinWidget];
		InputPins.Add(PinWidget);
	}
}

auto
	SGraphNode_GoapGoal::
	AddPin(const TSharedRef<SGraphPin>& PinToAdd)
	-> void
{
	PinToAdd->SetOwner(SharedThis(this));
	PinToAdd->SetVisibility(EVisibility::HitTestInvisible);
	RightNodeBox->AddSlot().AutoHeight()[PinToAdd];
	InputPins.Add(PinToAdd);
}

// ====================================================================================================================
