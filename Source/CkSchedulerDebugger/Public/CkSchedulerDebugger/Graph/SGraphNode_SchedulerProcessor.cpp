#include "CkSchedulerDebugger/Graph/SGraphNode_SchedulerProcessor.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugNode_Processor.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

#include "SGraphPin.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
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

	const auto GhostOpacity = _ProcessorNode->IsGhost ? 0.4f : 1.0f;

	RightNodeBox = SNew(SVerticalBox);

	GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
		.BorderBackgroundColor_Raw(this, &SGraphNode_SchedulerProcessor::Get_NodeBorderColor)
		.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, GhostOpacity))
		.Padding(FCkSchedulerDebuggerStyle::Node_BorderThickness)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				RightNodeBox.ToSharedRef()
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor_Raw(this, &SGraphNode_SchedulerProcessor::Get_GroupAccentColor)
					.Padding(FMargin(FCkSchedulerDebuggerStyle::Node_AccentWidth, 0.0f, 0.0f, 0.0f))
					[
						SNew(SSpacer)
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(FCkSchedulerDebuggerStyle::Color_Graph_NodeBody)
					.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 2.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(_ProcessorNode->DisplayName))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
							.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Primary)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text_Raw(this, &SGraphNode_SchedulerProcessor::Get_TimingText)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity_Raw(this, &SGraphNode_SchedulerProcessor::Get_TimingColor)
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(FString::Printf(TEXT("#%d"), _ProcessorNode->ExecutionOrder)))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 3.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(_ProcessorNode->HasDirtyMarker
									? FText::FromString(FString(UTF8TEXT("\u25CF")))
									: FText::GetEmpty())
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_DirtyMarker)
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(_ProcessorNode->IsParallel
									? FText::FromString(FString(UTF8TEXT("\u25C6")))
									: FText::GetEmpty())
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Parallel)
							]
						]
					]
				]
			]
		]
	];

	CreatePinWidgets();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SGraphNode_SchedulerProcessor::
	CreatePinWidgets()
	-> void
{
	for (auto Pin : GraphNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && NOT Pin->bHidden)
		{
			auto PinWidget = SNew(SGraphPin, Pin);
			PinWidget->SetIsEditable(false);
			PinWidget->SetVisibility(EVisibility::HitTestInvisible);
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

	RightNodeBox->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		InPinToAdd
	];
	OutputPins.Add(InPinToAdd);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SGraphNode_SchedulerProcessor::
	Get_GroupAccentColor() const
	-> FSlateColor
{
	return FSlateColor(FCkSchedulerDebuggerStyle::Get_GroupColor(_ProcessorNode->GroupName));
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

auto
	SGraphNode_SchedulerProcessor::
	Get_NodeBorderColor() const
	-> FSlateColor
{
	if (_ProcessorNode->IsGhost)
	{
		return FSlateColor(FCkSchedulerDebuggerStyle::Color_Ghost);
	}

	if (_ProcessorNode->WasDirtyThisFrame)
	{
		return FSlateColor(FCkSchedulerDebuggerStyle::Color_Pumped);
	}

	return FSlateColor(FCkSchedulerDebuggerStyle::Color_Graph_NodeBorder);
}

// --------------------------------------------------------------------------------------------------------------------
