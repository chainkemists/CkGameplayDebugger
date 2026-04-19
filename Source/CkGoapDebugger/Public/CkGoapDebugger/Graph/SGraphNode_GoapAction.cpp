#include "SGraphNode_GoapAction.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
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

	const auto BorderColor = InPlan ? CkDebugStyle::NodeBorder_InPlan() : CkDebugStyle::NodeBorder_Inactive();
	const auto FillColor   = InPlan ? CkDebugStyle::NodeFill_InPlan()   : CkDebugStyle::NodeFill_Inactive();
	const auto TextColor   = InPlan ? CkDebugStyle::Text()              : CkDebugStyle::TextDim();
	const auto Alpha       = InPlan ? 1.0f                              : CkDebugStyle::NodeInactiveOpacity();

	const auto RoundedBrush = CkDebugStyle::GetRoundedBrush();

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	// Header row — optional numbered step badge, action name, cost right-aligned.
	auto HeaderRow = SNew(SHorizontalBox);

	if (InPlan && PlanStep >= 0)
	{
		HeaderRow->AddSlot()
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
					.BorderBackgroundColor(FSlateColor(CkDebugStyle::Info()))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f))
					[
						SNew(STextBlock)
						.Text(FText::AsNumber(PlanStep + 1))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::NodeMetaFontSize()))
						.ColorAndOpacity(FSlateColor(CkDebugStyle::BgRoot()))
					]
				]
			];
	}

	HeaderRow->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(_ActionNode->Get_DisplayName()))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::NodeTitleFontSize()))
			.ColorAndOpacity(FSlateColor(TextColor))
		];

	HeaderRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("$%.0f"), _ActionNode->Get_Cost())))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::NodeCostFontSize()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent()))
		];

	const auto Body = _ActionNode->Get_IsPlanChainNode()
		? CreateCompactEffectsSummary()
		: CreatePortRows();

	this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SOverlay)

		// Hidden pin overlay for connection geometry.
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(RightNodeBox, SVerticalBox)
		]

		// Visual frame: rounded border color + rounded fill with padded content.
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

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						HeaderRow
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, CkDebugStyle::SpaceS, 0.0f, 0.0f)
					[
						Body
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
	const auto MonoFont = FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::NodeMetaFontSize());

	// Preconditions column (left-aligned, red marker).
	auto PreBox = SNew(SVerticalBox);
	for (const auto& Pre : _ActionNode->Get_Preconditions())
	{
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
				SNew(SBox)
				.WidthOverride(6.0f)
				.HeightOverride(6.0f)
				[
					SNew(SImage)
					.Image(CkDebugStyle::GetFilledBrush())
					.ColorAndOpacity(FSlateColor(CkDebugStyle::Err()))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Pre.AsString()))
				.Font(MonoFont)
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
			]
		];
	}

	// Effects column (right-aligned, green marker).
	auto EffBox = SNew(SVerticalBox);
	for (const auto& Eff : _ActionNode->Get_Effects())
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
				.Text(FText::FromString(Eff.AsString()))
				.Font(MonoFont)
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(6.0f)
				.HeightOverride(6.0f)
				[
					SNew(SImage)
					.Image(CkDebugStyle::GetFilledBrush())
					.ColorAndOpacity(FSlateColor(CkDebugStyle::Ok()))
				]
			]
		];
	}

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()           [ PreBox ]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkDebugStyle::SpaceL, 0.0f)[ SNew(SBox) ]
		+ SHorizontalBox::Slot().AutoWidth()           [ EffBox ];
}

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	CreateCompactEffectsSummary()
	-> TSharedRef<SWidget>
{
	auto EffectsText = FString{};
	for (const auto& Eff : _ActionNode->Get_Effects())
	{
		if (EffectsText.Len() > 0) { EffectsText += TEXT(", "); }
		EffectsText += Eff.AsString();
	}

	return SNew(STextBlock)
		.Text(FText::FromString(EffectsText))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::NodeMetaFontSize()))
		.ColorAndOpacity(FSlateColor(CkDebugStyle::Ok()));
}

// ====================================================================================================================

auto
	SGraphNode_GoapAction::
	GetBorderBackgroundColor() const
	-> FSlateColor
{
	return FSlateColor(_ActionNode->Get_InPlan() ? CkDebugStyle::NodeBorder_InPlan() : CkDebugStyle::NodeBorder_Inactive());
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
