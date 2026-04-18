#include "SGraphNode_GoapGoal.h"
#include "CkGoapDebugNode_Goal.h"

#include "CkDebuggerCommon/Graph/CkDebugNodeTheme.h"
#include "CkDebuggerCommon/Settings/CkDebuggerSettings.h"

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

auto
	SGraphNode_GoapGoal::
	UpdateGraphNode()
	-> void
{
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	const auto Theme = UCkDebuggerSettings::GetTheme();
	const auto IsActive = _GoalNode->Get_IsActiveGoal();
	const auto BorderColor = IsActive ? Theme.GoalBorder : Theme.InactiveBorder;

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	auto ConditionsList = SNew(SVerticalBox);
	for (const auto& C : _GoalNode->Get_Conditions())
	{
		const auto Label = C.AsString();
		ConditionsList->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Label))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(Theme.PortLabel)
			.Justification(ETextJustify::Center)
		];
	}

	this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.BorderImage(Theme.GetBodyBrush())
		.Padding(Theme.GetBodyPadding())
		.BorderBackgroundColor(BorderColor)
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
			[ SAssignNew(RightNodeBox, SVerticalBox) ]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(Theme.GetContentBrush())
				.BorderBackgroundColor(Theme.ActiveFill)
				.Padding(FMargin(12.0f, 6.0f))
				.Visibility(EVisibility::SelfHitTestInvisible)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("[GOAL] %s"),
							*(_GoalNode->Get_DisplayName().IsEmpty()
								? _GoalNode->Get_GoalName()
								: _GoalNode->Get_DisplayName()))))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
						.ColorAndOpacity(Theme.GoalText)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("P:%d"), _GoalNode->Get_Priority())))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(Theme.InactiveText)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[ ConditionsList ]
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
