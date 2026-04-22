#include "CkSchedulerDebugger/Graph/SGraphNode_SchedulerProcessor.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugNode_Processor.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NodePill.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	SGraphNode_SchedulerProcessor::
	Construct(
		const FArguments& InArgs,
		UCkSchedulerDebugNode_Processor* InNode)
	-> void
{
	_ProcessorNode = InNode;
	GraphNode = InNode;
	UpdateGraphNode();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SGraphNode_SchedulerProcessor::
	UpdateGraphNode()
	-> void
{
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	const auto Variant = _ProcessorNode->IsGhost
		? ECkDebug_NodePillVariant::Inactive
		: ECkDebug_NodePillVariant::InPlan;

	// Pumped-this-frame highlight rides on the pill's border-color override so
	// the rest of the variant palette (fill / badge) keeps flowing from tokens.
	const auto BorderOverride = _ProcessorNode->WasDirtyThisFrame
		? CkDebugStyle::Warn()
		: FLinearColor::Transparent;

	const auto AccentColor = FCkSchedulerDebuggerStyle::Get_GroupColor(_ProcessorNode->GroupName);

	GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SOverlay)

		// Hidden pin overlay for connection geometry — pins are HitTestInvisible
		// so they don't eat clicks targeted at the pill.
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(RightNodeBox, SVerticalBox)
			.Visibility(EVisibility::HitTestInvisible)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SCkDebug_NodePill)
			.Variant(Variant)
			.StepIndex(-1)
			.ShowCost(false)
			.Title(FText::FromString(_ProcessorNode->DisplayName))
			.AccentColor(AccentColor)
			.BorderColorOverride(BorderOverride)
			.BodyContent() [ CreateMetaRow() ]
		]
	];

	CreatePinWidgets();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SGraphNode_SchedulerProcessor::
	CreateMetaRow()
	-> TSharedRef<SWidget>
{
	const auto MetaFont = FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::NodeMetaFontSize());

	auto Row = SNew(SHorizontalBox);

	Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
		[
			SNew(STextBlock)
			.Text_Raw(this, &SGraphNode_SchedulerProcessor::Get_TimingText)
			.Font(MetaFont)
			.ColorAndOpacity_Raw(this, &SGraphNode_SchedulerProcessor::Get_TimingColor)
		];

	Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("#%d"), _ProcessorNode->ExecutionOrder)))
			.Font(MetaFont)
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
		];

	if (_ProcessorNode->HasDirtyMarker)
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, CkDebugStyle::SpaceS, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString(UTF8TEXT("\u25CF"))))
				.Font(MetaFont)
				.ColorAndOpacity(FSlateColor(CkDebugStyle::Warn()))
			];
	}

	if (_ProcessorNode->IsParallel)
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString(UTF8TEXT("\u25C6"))))
				.Font(MetaFont)
				.ColorAndOpacity(FSlateColor(CkDebugStyle::Info()))
			];
	}

	return Row;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SGraphNode_SchedulerProcessor::
	CreatePinWidgets()
	-> void
{
	for (auto Pin : GraphNode->Pins)
	{
		if (Pin && NOT Pin->bHidden)
		{
			auto PinWidget = SNew(SGraphPin, Pin);
			AddPin(PinWidget);
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SGraphNode_SchedulerProcessor::
	AddPin(
		const TSharedRef<SGraphPin>& InPinToAdd)
	-> void
{
	InPinToAdd->SetOwner(SharedThis(this));
	InPinToAdd->SetRenderOpacity(0.0f);
	InPinToAdd->SetVisibility(EVisibility::HitTestInvisible);

	if (RightNodeBox.IsValid())
	{
		RightNodeBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			[
				InPinToAdd
			];
	}

	if (InPinToAdd->GetDirection() == EGPD_Input)
	{
		InputPins.Add(InPinToAdd);
	}
	else
	{
		OutputPins.Add(InPinToAdd);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SGraphNode_SchedulerProcessor::
	Get_TimingText() const
	-> FText
{
	return FText::FromString(FString::Printf(TEXT("%.3f ms"), _ProcessorNode->LastFrameTimeMs));
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SGraphNode_SchedulerProcessor::
	Get_TimingColor() const
	-> FSlateColor
{
	return FSlateColor(FCkSchedulerDebuggerStyle::Get_TimingColor(_ProcessorNode->LastFrameTimeMs));
}

// --------------------------------------------------------------------------------------------------------------------
