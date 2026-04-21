#include "SGraphNode_GoapAction.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NodePill.h"

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

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	const auto Variant = InPlan
		? ECkDebug_NodePillVariant::InPlan
		: ECkDebug_NodePillVariant::Inactive;

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

		// Shared action-pill — identical visual core to plan-strip step pills.
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SCkDebug_NodePill)
			.Variant(Variant)
			.StepIndex(InPlan ? PlanStep : -1)
			.Title(FText::FromString(_ActionNode->Get_DisplayName()))
			.CostValue(_ActionNode->Get_Cost())
			.BodyContent() [ Body ]
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
