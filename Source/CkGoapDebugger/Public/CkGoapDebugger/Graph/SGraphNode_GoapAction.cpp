#include "SGraphNode_GoapAction.h"
#include "CkGoapDebugNode_Action.h"

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
	const auto NodeOpacity = InPlan ? 1.0f : 0.25f;

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
		.Padding(0.0f)
		.BorderBackgroundColor(this, &SGraphNode_GoapAction::GetBorderBackgroundColor)
		.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, NodeOpacity))
		[
			SNew(SOverlay)

			// Hidden pin area for connection geometry
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(RightNodeBox, SVerticalBox)
			]

			// Visual content
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)

				// Header: name + cost
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Graph.StateNode.ColorSpill"))
					.BorderBackgroundColor(FLinearColor(0.05f, 0.08f, 0.12f))
					.Padding(FMargin(8.0f, 4.0f))
					[
						SNew(SHorizontalBox)

						// Plan step badge
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SBox)
							.WidthOverride(InPlan ? 20.0f : 0.0f)
							.HeightOverride(InPlan ? 20.0f : 0.0f)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("GenericWhiteBox"))
								.BorderBackgroundColor(FLinearColor(0.23f, 0.51f, 0.96f))
								.Padding(FMargin(0.0f))
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.Visibility(InPlan ? EVisibility::Visible : EVisibility::Collapsed)
								[
									SNew(STextBlock)
									.Text(FText::AsNumber(PlanStep + 1))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
									.ColorAndOpacity(FLinearColor::White)
								]
							]
						]

						// Action name
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(_ActionNode->Get_DisplayName()))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
							.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
						]

						// Cost badge
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("$%.0f"), _ActionNode->Get_Cost())))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(FLinearColor(0.96f, 0.62f, 0.04f))
						]
					]
				]

				// Ports or compact effects summary
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(8.0f, 4.0f, 8.0f, 6.0f))
				[
					_ActionNode->Get_IsPlanChainNode()
						? CreateCompactEffectsSummary()
						: CreatePortRows()
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

	// Preconditions (left column)
	auto PreBox = SNew(SVerticalBox);
	for (const auto& [Key, Value] : _ActionNode->Get_Preconditions())
	{
		const auto Satisfied = Value;
		const auto DotColor = Satisfied ? FLinearColor(0.13f, 0.77f, 0.37f) : FLinearColor(0.94f, 0.27f, 0.27f);

		PreBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("GenericWhiteBox"))
				.ColorAndOpacity(DotColor)
				.DesiredSizeOverride(FVector2D(7.0f, 7.0f))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Key.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
			]
		];
	}

	// Spacer
	auto Spacer = SNew(SSpacer).Size(FVector2D(16.0f, 0.0f));

	// Effects (right column)
	auto EffBox = SNew(SVerticalBox);
	for (const auto& [Key, Value] : _ActionNode->Get_Effects())
	{
		EffBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Key.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("GenericWhiteBox"))
				.ColorAndOpacity(FLinearColor(0.23f, 0.51f, 0.96f))
				.DesiredSizeOverride(FVector2D(7.0f, 7.0f))
			]
		];
	}

	PortBox->AddSlot().AutoWidth()[PreBox];
	PortBox->AddSlot().FillWidth(1.0f)[Spacer];
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
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
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
		return FLinearColor(0.23f, 0.51f, 0.96f);
	}

	return FLinearColor(0.1f, 0.14f, 0.2f);
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
			PinWidget->SetVisibility(EVisibility::HitTestInvisible);
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
