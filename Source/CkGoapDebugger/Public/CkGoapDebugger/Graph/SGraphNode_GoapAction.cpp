#include "SGraphNode_GoapAction.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"

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

	const auto InPlan = _ActionNode->Get_InPlan();
	const auto PlanStep = _ActionNode->Get_PlanStepIndex();

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.Body")))
		.Padding(0.0f)
		.BorderBackgroundColor(this, &SGraphNode_GoapAction::GetBorderBackgroundColor)
		[
			SNew(SOverlay)

			// Hidden pin overlay — fills entire node for connection geometry
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
				.BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.ColorSpill")))
				.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f))
				.Padding(FMargin(6.0f, 4.0f))
				.Visibility(EVisibility::SelfHitTestInvisible)
				[
					SNew(SVerticalBox)

					// Header: step badge + name + cost
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						// Step badge (only for plan nodes)
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
								.BorderBackgroundColor(InPlan
									? FLinearColor(0.15f, 0.45f, 0.85f)
									: FLinearColor(0.3f, 0.3f, 0.3f))
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
							.ColorAndOpacity_Lambda([this, InPlan]()
							{
								auto Color = FLinearColor(0.9f, 0.9f, 0.9f);
								if (NOT InPlan) { Color.A = 0.4f; }
								return FSlateColor(Color);
							})
						]

						// Cost badge
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("$%.0f"), _ActionNode->Get_Cost())))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(FLinearColor(0.96f, 0.62f, 0.04f))
						]
					]

					// Ports: preconditions (left) + effects (right)
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
	auto PortBox = SNew(SHorizontalBox);

	// Preconditions (left)
	auto PreBox = SNew(SVerticalBox);
	for (const auto& [Key, Value] : _ActionNode->Get_Preconditions())
	{
		const auto DotColor = Value
			? FLinearColor(0.13f, 0.77f, 0.37f)
			: FLinearColor(0.94f, 0.27f, 0.27f);

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
				.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
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
				.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("WhiteBrush"))
				.ColorAndOpacity(FLinearColor(0.15f, 0.45f, 0.85f))
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
	auto EffectsText = FString{};
	for (const auto& [Key, Value] : _ActionNode->Get_Effects())
	{
		if (EffectsText.Len() > 0) { EffectsText += TEXT(", "); }
		EffectsText += FString::Printf(TEXT("+%s"), *Key.ToString());
	}

	return SNew(STextBlock)
		.Text(FText::FromString(EffectsText))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
		.ColorAndOpacity(FLinearColor(0.13f, 0.77f, 0.37f));
}

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	GetBorderBackgroundColor() const
	-> FSlateColor
{
	if (_ActionNode->Get_InPlan())
	{
		return FLinearColor(0.15f, 0.45f, 0.85f);
	}

	return FLinearColor(0.25f, 0.25f, 0.28f);
}

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	CreatePinWidgets()
	-> void
{
	for (auto* Pin : GraphNode->Pins)
	{
		if (Pin->Direction == EGPD_Output)
		{
			auto PinWidget = SNew(SGraphPin, Pin);
			PinWidget->SetOwner(SharedThis(this));
			PinWidget->SetVisibility(EVisibility::Collapsed);
			RightNodeBox->AddSlot().AutoHeight().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[PinWidget];
			OutputPins.Add(PinWidget);
		}
	}
}

auto
	SGraphNode_GoapAction::
	AddPin(const TSharedRef<SGraphPin>& PinToAdd)
	-> void
{
	PinToAdd->SetOwner(SharedThis(this));
	PinToAdd->SetVisibility(EVisibility::Collapsed);
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
