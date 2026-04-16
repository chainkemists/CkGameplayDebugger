#include "SGraphNode_GoapAction.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"

#include "CkDebuggerCommon/Graph/CkDebugNodeTheme.h"
#include "CkDebuggerCommon/Settings/CkDebuggerSettings.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	Construct(const FArguments& InArgs, UCkGoapDebugNode_Action* InNode)
	-> void
{
	GraphNode = InNode;
	_ActionNode = InNode;
	UpdateGraphNode();
}

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	UpdateGraphNode()
	-> void
{
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	const auto Theme = UCkDebuggerSettings::GetTheme();
	const auto InPlan = _ActionNode->Get_InPlan();
	const auto PlanStep = _ActionNode->Get_PlanStepIndex();

	const auto BorderColor = InPlan ? Theme.ActiveBorder : Theme.InactiveBorder;
	const auto FillColor = InPlan ? Theme.ActiveFill : Theme.InactiveFill;
	const auto TextColor = InPlan ? Theme.ActiveText : Theme.InactiveText;
	const auto Alpha = InPlan ? 1.0f : Theme.InactiveAlpha;

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.BorderImage(Theme.GetBodyBrush())
		.Padding(0.0f)
		.BorderBackgroundColor(this, &SGraphNode_GoapAction::GetBorderBackgroundColor)
		.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, Alpha))
		[
			SNew(SOverlay)

			// Hidden pin overlay for connection geometry
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(RightNodeBox, SVerticalBox)
			]

			// Visual content
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(Theme.GetContentBrush())
				.BorderBackgroundColor(FillColor)
				.Padding(FMargin(6.0f, 4.0f))
				.Visibility(EVisibility::SelfHitTestInvisible)
				[
					SNew(SVerticalBox)

					// Header: step badge + name + cost
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						// Step indicator
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SBox)
							.WidthOverride(InPlan ? 8.0f : 0.0f)
							.HeightOverride(InPlan ? 8.0f : 0.0f)
							.Visibility(InPlan ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
								.BorderBackgroundColor(Theme.ActiveBorder)
							]
						]

						// Action name
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(_ActionNode->Get_DisplayName()))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(TextColor)
						]

						// Cost
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("$%.0f"), _ActionNode->Get_Cost())))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(Theme.CostText)
						]
					]

					// Ports
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						_ActionNode->Get_IsPlanChainNode()
							? CreateCompactEffectsSummary()
							: CreatePortRows()
					]
				]
			]
		]
	];

	CreatePinWidgets();
}

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	CreatePortRows()
	-> TSharedRef<SWidget>
{
	const auto Theme = UCkDebuggerSettings::GetTheme();

	auto PortBox = SNew(SHorizontalBox);

	// Preconditions (left)
	auto PreBox = SNew(SVerticalBox);
	for (const auto& [Key, Value] : _ActionNode->Get_Preconditions())
	{
		const auto DotColor = Value ? Theme.PreconditionSatisfied : Theme.PreconditionUnsatisfied;

		PreBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("WhiteBrush"))
				.ColorAndOpacity(DotColor)
				.DesiredSizeOverride(FVector2D(5.0f, 5.0f))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Key.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
				.ColorAndOpacity(Theme.PortLabel)
			]
		];
	}

	// Effects (right)
	auto EffBox = SNew(SVerticalBox);
	for (const auto& [Key, Value] : _ActionNode->Get_Effects())
	{
		EffBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Key.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
				.ColorAndOpacity(Theme.PortLabel)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("WhiteBrush"))
				.ColorAndOpacity(Theme.EffectPort)
				.DesiredSizeOverride(FVector2D(5.0f, 5.0f))
			]
		];
	}

	PortBox->AddSlot().AutoWidth()[PreBox];
	PortBox->AddSlot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(12.0f, 0.0f))];
	PortBox->AddSlot().AutoWidth()[EffBox];

	return PortBox;
}

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	CreateCompactEffectsSummary()
	-> TSharedRef<SWidget>
{
	const auto Theme = UCkDebuggerSettings::GetTheme();

	auto EffectsText = FString{};
	for (const auto& [Key, Value] : _ActionNode->Get_Effects())
	{
		if (EffectsText.Len() > 0) { EffectsText += TEXT(", "); }
		EffectsText += FString::Printf(TEXT("+%s"), *Key.ToString());
	}

	return SNew(STextBlock)
		.Text(FText::FromString(EffectsText))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
		.ColorAndOpacity(Theme.PreconditionSatisfied);
}

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	GetBorderBackgroundColor() const
	-> FSlateColor
{
	const auto Theme = UCkDebuggerSettings::GetTheme();

	if (_ActionNode->Get_InPlan())
	{
		return Theme.ActiveBorder;
	}

	return Theme.InactiveBorder;
}

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	CreatePinWidgets()
	-> void
{
	for (auto* Pin : GraphNode->Pins)
	{
		auto PinWidget = SNew(SGraphPin, Pin);
		PinWidget->SetOwner(SharedThis(this));
		PinWidget->SetVisibility(EVisibility::HitTestInvisible);
		RightNodeBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[PinWidget];

		if (Pin->Direction == EGPD_Output)
		{
			OutputPins.Add(PinWidget);
		}
		else
		{
			InputPins.Add(PinWidget);
		}
	}
}

auto
	SGraphNode_GoapAction::
	AddPin(const TSharedRef<SGraphPin>& PinToAdd)
	-> void
{
	PinToAdd->SetOwner(SharedThis(this));
	PinToAdd->SetVisibility(EVisibility::HitTestInvisible);
	RightNodeBox->AddSlot().AutoHeight()[PinToAdd];

	if (PinToAdd->GetDirection() == EGPD_Input)
	{
		InputPins.Add(PinToAdd);
	}
	else
	{
		OutputPins.Add(PinToAdd);
	}
}

// ====================================================================================================================
