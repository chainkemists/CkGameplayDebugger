#include "SGraphNode_GoapGoal.h"
#include "CkGoapDebugNode_Goal.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
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

// ====================================================================================================================

auto
	SGraphNode_GoapGoal::
	UpdateGraphNode()
	-> void
{
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	constexpr auto AmberColor = FLinearColor(0.96f, 0.62f, 0.04f);
	const auto IsActive = _GoalNode->Get_IsActiveGoal();
	const auto BorderColor = IsActive ? AmberColor : FLinearColor(0.3f, 0.3f, 0.3f);

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	auto ConditionsList = SNew(SVerticalBox);
	for (const auto& [Key, Value] : _GoalNode->Get_Conditions())
	{
		ConditionsList->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Key.ToString()))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
			.Justification(ETextJustify::Center)
		];
	}

	this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
		.Padding(0.0f)
		.BorderBackgroundColor(BorderColor)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(RightNodeBox, SVerticalBox)
			]

			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Graph.StateNode.ColorSpill"))
				.BorderBackgroundColor(FLinearColor(0.05f, 0.08f, 0.12f))
				.Padding(FMargin(16.0f, 8.0f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("\u2B22 %s"), *_GoalNode->Get_GoalName())))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
						.ColorAndOpacity(AmberColor)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("P:%d"), _GoalNode->Get_Priority())))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						ConditionsList
					]
				]
			]
		]
	];

	CreatePinWidgets();
}

// ====================================================================================================================

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
