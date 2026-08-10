#include "CkSchedulerDebuggerPage_TreeView.h"

#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_ProcessorTree.h"
#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_Inspector.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugGraph.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugGraphSchema.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugNode_Processor.h"
#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebugger_Axes.h"

#include "GraphEditor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebuggerPage_TreeView::
	Get_PageName() const
	-> FText
{
	return FText::FromString(TEXT("Tree View"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebuggerPage_TreeView::
	Build_Content(
		TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel)
	-> TSharedRef<SWidget>
{
	_ViewModel = InViewModel;
	_DetailGraphContainer = SNew(SBox);

	_FullGraph = NewObject<UCkSchedulerDebugGraph>();
	_FullGraph->AddToRoot();
	_FullGraph->Schema = UCkSchedulerDebugGraphSchema::StaticClass();

	_DetailGraph = NewObject<UCkSchedulerDebugGraph>();
	_DetailGraph->AddToRoot();
	_DetailGraph->Schema = UCkSchedulerDebugGraphSchema::StaticClass();

	if (_ViewModel.IsValid())
	{
		_SelectionChangedHandle = _ViewModel->OnSelectionChanged.AddRaw(
			this, &FCkSchedulerDebuggerPage_TreeView::DoOnSelectionChanged);
	}

	DoBuildDetailGraph();

	return SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		+ SSplitter::Slot()
			.Value(0.25f)
			.MinSize(240.0f)
			[
				SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						SAssignNew(_ProcessorTree, SCkSchedulerDebugger_ProcessorTree)
							.ViewModel(InViewModel)
					]
			]

		+ SSplitter::Slot()
			.Value(0.50f)
			.MinSize(400.0f)
			[
				SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
								[
									SNew(SButton)
									.Text(FText::FromString(TEXT("Relayout")))
									.OnClicked_Lambda([this]() -> FReply
									{
										if (_DetailGraph)
										{ _DetailGraph->RequestRelayout(); }
										return FReply::Handled();
									})
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("X:")))
									.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
									.ColorAndOpacity(CkStyle::TextDim())
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SBox)
									.WidthOverride(60.0f)
									[
										SNew(SSpinBox<int32>)
										.MinValue(100)
										.MaxValue(600)
										.Value_Lambda([this]() { return _FullGraph ? _FullGraph->LayoutParams.SpacingX : 300; })
										.OnValueChanged_Lambda([this](int32 InValue)
										{
											if (_FullGraph) { _FullGraph->LayoutParams.SpacingX = InValue; }
											if (_DetailGraph) { _DetailGraph->LayoutParams.SpacingX = InValue; }
										})
									]
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("Y:")))
									.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
									.ColorAndOpacity(CkStyle::TextDim())
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SBox)
									.WidthOverride(60.0f)
									[
										SNew(SSpinBox<int32>)
										.MinValue(50)
										.MaxValue(250)
										.Value_Lambda([this]() { return _FullGraph ? _FullGraph->LayoutParams.SpacingY : 100; })
										.OnValueChanged_Lambda([this](int32 InValue)
										{
											if (_FullGraph) { _FullGraph->LayoutParams.SpacingY = InValue; }
											if (_DetailGraph) { _DetailGraph->LayoutParams.SpacingY = InValue; }
										})
									]
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("Passes:")))
									.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
									.ColorAndOpacity(CkStyle::TextDim())
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SBox)
									.WidthOverride(50.0f)
									[
										SNew(SSpinBox<int32>)
										.MinValue(0)
										.MaxValue(8)
										.Value_Lambda([this]() { return _FullGraph ? _FullGraph->LayoutParams.CrossingReductionPasses : 4; })
										.OnValueChanged_Lambda([this](int32 InValue)
										{
											if (_FullGraph) { _FullGraph->LayoutParams.CrossingReductionPasses = InValue; }
											if (_DetailGraph) { _DetailGraph->LayoutParams.CrossingReductionPasses = InValue; }
										})
									]
								]
							]

						+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								_DetailGraphContainer.ToSharedRef()
							]
					]
			]

		+ SSplitter::Slot()
			.Value(0.25f)
			.MinSize(260.0f)
			[
				SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						SAssignNew(_Inspector, SCkSchedulerDebugger_Inspector)
							.ViewModel(InViewModel)
					]
			];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebuggerPage_TreeView::
	Tick(
		float InDeltaTime)
	-> void
{
	if (NOT _ViewModel.IsValid() || NOT _FullGraph)
	{ return; }

	const auto& Collector = _ViewModel->Get_DataCollector();
	const auto& Processors = Collector.Get_Processors();

	if (Processors.Num() > 0)
	{
		_FullGraph->RebuildFromData(Processors);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebuggerPage_TreeView::
	OnSelectionChanged(
		int32 InProcessorIndex)
	-> void
{
	DoOnSelectionChanged(InProcessorIndex);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebuggerPage_TreeView::
	OnStyleRevisionChanged()
	-> void
{
	if (_ProcessorTree.IsValid())
	{ _ProcessorTree->Rebuild_ForStyleChange(); }

	if (_Inspector.IsValid())
	{ _Inspector->Rebuild_ForStyleChange(); }

	// The detail graph's node widgets bake their fill/border at construction (the SGraphNode live-bind
	// invariant only covers the flag-driven visuals), so the graph is re-emitted rather than repainted.
	// DoRebuildDetailGraph already forces past the graph's own topology hash.
	DoRebuildDetailGraph();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebuggerPage_TreeView::
	DoOnSelectionChanged(
		int32 InProcessorIndex)
	-> void
{
	DoRebuildDetailGraph();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebuggerPage_TreeView::
	DoRebuildDetailGraph()
	-> void
{
	if (NOT _ViewModel.IsValid() || NOT _DetailGraph || NOT _DetailGraphContainer.IsValid())
	{ return; }

	const auto SelectedIdx = _ViewModel->Get_SelectedProcessorIndex();

	if (SelectedIdx == INDEX_NONE)
	{
		_DetailGraphContainer->SetContent(
			SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(TEXT("Select a processor to view its dependencies")))
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_EmptyState)
						.ColorAndOpacity(CkStyle::TextMute())
				]
		);
		return;
	}

	const auto& Collector = _ViewModel->Get_DataCollector();
	const auto& AllProcessors = Collector.Get_Processors();

	if (NOT AllProcessors.IsValidIndex(SelectedIdx))
	{ return; }

	const auto& Selected = AllProcessors[SelectedIdx];

	auto NeighborIndices = TSet<int32>{};
	NeighborIndices.Add(SelectedIdx);

	for (const auto InEdge : Selected.InEdges)
	{
		for (auto Idx = 0; Idx < AllProcessors.Num(); ++Idx)
		{
			if (AllProcessors[Idx].NodeIndex == InEdge)
			{
				NeighborIndices.Add(Idx);
				break;
			}
		}
	}

	for (const auto OutEdge : Selected.OutEdges)
	{
		for (auto Idx = 0; Idx < AllProcessors.Num(); ++Idx)
		{
			if (AllProcessors[Idx].NodeIndex == OutEdge)
			{
				NeighborIndices.Add(Idx);
				break;
			}
		}
	}

	auto SubsetProcessors = TArray<FCkSchedulerDebugger_ProcessorInfo>{};
	auto IndexRemapping = TMap<int32, int32>{};

	for (const auto Idx : NeighborIndices)
	{
		if (AllProcessors.IsValidIndex(Idx))
		{
			IndexRemapping.Add(AllProcessors[Idx].NodeIndex, SubsetProcessors.Num());
			SubsetProcessors.Add(AllProcessors[Idx]);
		}
	}

	for (auto& Proc : SubsetProcessors)
	{
		auto RemappedIn = TArray<int32>{};
		for (const auto Edge : Proc.InEdges)
		{
			if (const auto* Found = IndexRemapping.Find(Edge))
			{
				RemappedIn.Add(SubsetProcessors[*Found].NodeIndex);
			}
		}
		Proc.InEdges = RemappedIn;

		auto RemappedOut = TArray<int32>{};
		for (const auto Edge : Proc.OutEdges)
		{
			if (const auto* Found = IndexRemapping.Find(Edge))
			{
				RemappedOut.Add(SubsetProcessors[*Found].NodeIndex);
			}
		}
		Proc.OutEdges = RemappedOut;
	}

	_DetailGraph->ForceRebuild();
	_DetailGraph->RebuildFromData(SubsetProcessors);

	_DetailGraphEditor = SNew(SGraphEditor)
		.GraphToEdit(_DetailGraph)
		.IsEditable(false);

	_DetailGraphContainer->SetContent(_DetailGraphEditor.ToSharedRef());
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebuggerPage_TreeView::
	DoBuildDetailGraph()
	-> TSharedRef<SWidget>
{
	_DetailGraphContainer->SetContent(
		SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("Select a processor to view its dependencies")))
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_EmptyState)
					.ColorAndOpacity(CkStyle::TextMute())
			]
	);
	return _DetailGraphContainer.ToSharedRef();
}

// --------------------------------------------------------------------------------------------------------------------
