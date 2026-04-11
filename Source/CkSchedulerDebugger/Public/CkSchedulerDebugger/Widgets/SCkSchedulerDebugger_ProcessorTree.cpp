#include "SCkSchedulerDebugger_ProcessorTree.h"

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SButton.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	auto DoBuildProcessorRowContent(
		const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode,
		const TSharedPtr<FCkSchedulerDebugger_ViewModel>& InViewModel)
		-> TSharedRef<SWidget>
	{
		auto ExecOrderText = FString{};
		auto TimingText = FString{};
		auto EntityCountText = FString{};
		auto TimingColor = FCkSchedulerDebuggerStyle::Color_Heat_Fast;
		auto IsGhost = false;
		auto IsDirty = false;
		auto IsParallel = false;

		if (InViewModel.IsValid() && InNode->ProcessorIndex != INDEX_NONE)
		{
			const auto& Procs = InViewModel->Get_DataCollector().Get_Processors();
			if (Procs.IsValidIndex(InNode->ProcessorIndex))
			{
				const auto& Proc = Procs[InNode->ProcessorIndex];
				ExecOrderText = FString::Printf(TEXT("#%d"), Proc.ExecutionOrder);
				TimingText = FString::Printf(TEXT("%.3f ms"), Proc.MainPassTimeMs);
				TimingColor = FCkSchedulerDebuggerStyle::Get_TimingColor(Proc.MainPassTimeMs);
				IsGhost = Proc.IsGhost;
				IsDirty = Proc.HasDirtyMarker;
				IsParallel = Proc.IsParallel;
				EntityCountText = FString::Printf(TEXT("%d"), Proc.MainPassEntityCount);
			}
		}

		const auto NameColor = IsGhost
			? FCkSchedulerDebuggerStyle::Color_Ghost
			: FCkSchedulerDebuggerStyle::Color_Text_Primary;

		auto Row = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 1.0f, FCkSchedulerDebuggerStyle::Padding_Small, 1.0f)
				[
					SNew(SBox)
						.WidthOverride(30.0f)
						[
							SNew(STextBlock)
								.Text(FText::FromString(ExecOrderText))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
								.Justification(ETextJustify::Right)
						]
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(InNode->DisplayName))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(NameColor)
				];

		if (IsDirty)
		{
			Row->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(FString(TEXT("\u25CF"))))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_DirtyMarker)
						.ToolTipText(FText::FromString(TEXT("Has dirty marker")))
				];
		}

		if (IsParallel)
		{
			Row->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(FString(TEXT("\u25C6"))))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Parallel)
						.ToolTipText(FText::FromString(TEXT("Parallel processor")))
				];
		}

		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(SBox)
					.WidthOverride(40.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(EntityCountText))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
							.Justification(ETextJustify::Right)
							.ToolTipText(FText::FromString(TEXT("Entity count")))
					]
			];

		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TimingText))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(TimingColor)
			];

		return Row;
	}

	auto DoBuildGroupRowContent(
		const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode,
		const TSharedPtr<FCkSchedulerDebugger_ViewModel>& InViewModel)
		-> TSharedRef<SWidget>
	{
		auto AccentColor = FCkSchedulerDebuggerStyle::Color_Group_Default;
		auto AggregateText = FString{};
		auto ChildCount = InNode->Children.Num();

		if (InViewModel.IsValid() && InNode->GroupIndex != INDEX_NONE)
		{
			const auto& Groups = InViewModel->Get_DataCollector().Get_Groups();
			if (Groups.IsValidIndex(InNode->GroupIndex))
			{
				AccentColor = Groups[InNode->GroupIndex].AccentColor;
				AggregateText = FString::Printf(TEXT("%.2f ms"),
					Groups[InNode->GroupIndex].AggregateTimeMs);
			}
		}

		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
						.WidthOverride(FCkSchedulerDebuggerStyle::Node_AccentWidth)
						.HeightOverride(20.0f)
						[
							SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.ColorAndOpacity(AccentColor)
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 1.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(InNode->DisplayName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity(AccentColor)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::AsNumber(ChildCount))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(AggregateText))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
				];
	}

	auto DoBuildTickGroupRowContent(
		const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode)
		-> TSharedRef<SWidget>
	{
		auto TotalChildren = 0;
		for (const auto& Group : InNode->Children)
		{
			TotalChildren += Group->Children.Num();
		}

		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(InNode->DisplayName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Primary)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.235f, 0.510f, 0.863f, 0.3f))
						.Padding(FMargin(4.0f, 1.0f))
						[
							SNew(STextBlock)
								.Text(FText::AsNumber(TotalChildren))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
						]
				];
	}

	auto DoBuildRowContent(
		const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode,
		const TSharedPtr<FCkSchedulerDebugger_ViewModel>& InViewModel)
		-> TSharedRef<SWidget>
	{
		if (NOT InNode.IsValid())
		{
			return SNew(STextBlock).Text(FText::FromString(TEXT("---")));
		}

		switch (InNode->Type)
		{
		case ECkSchedulerDebugger_TreeNodeType::TickGroup:
			return DoBuildTickGroupRowContent(InNode);
		case ECkSchedulerDebugger_TreeNodeType::Group:
			return DoBuildGroupRowContent(InNode, InViewModel);
		case ECkSchedulerDebugger_TreeNodeType::Processor:
			return DoBuildProcessorRowContent(InNode, InViewModel);
		default:
			return SNew(STextBlock).Text(FText::FromString(InNode->DisplayName));
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	Construct(
		const FArguments& InArgs)
	-> void
{
	_ViewModel = InArgs._ViewModel;

	_PumpContainer = SNew(SBox);

	static TArray<TSharedPtr<FString>> SortOptions = {
		MakeShared<FString>(TEXT("Exec Order")),
		MakeShared<FString>(TEXT("Name")),
		MakeShared<FString>(TEXT("Timing"))
	};

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
					[
						SNew(SSearchBox)
							.OnTextChanged_Lambda([this](const FText& InText)
							{
								DoApplyFilter(InText);
							})
							.HintText(FText::FromString(TEXT("Search processors...")))
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&SortOptions)
							.InitiallySelectedItem(SortOptions[0])
							.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) -> TSharedRef<SWidget>
							{
								return SNew(STextBlock)
									.Text(FText::FromString(*InItem))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
							})
							.OnSelectionChanged_Lambda([this](TSharedPtr<FString> InItem, ESelectInfo::Type)
							{
								if (*InItem == TEXT("Name"))
								{
									_SortMode = ECkSchedulerDebugger_SortMode::Name;
								}
								else if (*InItem == TEXT("Timing"))
								{
									_SortMode = ECkSchedulerDebugger_SortMode::Timing;
								}
								else
								{
									_SortMode = ECkSchedulerDebugger_SortMode::ExecutionOrder;
								}
								DoRebuildFlattenedTree();
							})
							[
								SNew(STextBlock)
									.Text_Lambda([this]() -> FText
									{
										switch (_SortMode)
										{
										case ECkSchedulerDebugger_SortMode::Name: return FText::FromString(TEXT("Name"));
										case ECkSchedulerDebugger_SortMode::Timing: return FText::FromString(TEXT("Timing"));
										default: return FText::FromString(TEXT("Exec Order"));
										}
									})
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							]
					]
			]

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SSplitter)
					.Orientation(Orient_Vertical)

					+ SSplitter::Slot()
						.Value(0.7f)
						[
							SAssignNew(_TreeView, STreeView<TSharedPtr<FCkSchedulerDebugger_TreeNode>>)
								.TreeItemsSource(&_DisplayRoots)
								.OnGenerateRow(this, &SCkSchedulerDebugger_ProcessorTree::DoGenerateRow)
								.OnGetChildren(this, &SCkSchedulerDebugger_ProcessorTree::DoGetChildren)
								.OnSelectionChanged(this, &SCkSchedulerDebugger_ProcessorTree::DoOnSelectionChanged)
								.SelectionMode(ESelectionMode::Single)
						]

					+ SSplitter::Slot()
						.Value(0.3f)
						[
							SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
								[
									SNew(SVerticalBox)

									+ SVerticalBox::Slot()
										.AutoHeight()
										.Padding(0.0f, 0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
										[
											SNew(SHorizontalBox)

											+ SHorizontalBox::Slot()
												.AutoWidth()
												.VAlign(VAlign_Center)
												.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
												[
													SNew(STextBlock)
														.Text(FText::FromString(TEXT("Frame Breakdown")))
														.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
														.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
												]

											+ SHorizontalBox::Slot()
												.FillWidth(1.0f)
												[
													SNew(SSearchBox)
														.OnTextChanged_Lambda([this](const FText& InText)
														{
															_BreakdownFilterString = InText.ToString();
															_LastPumpDataHash = 0;
															DoOnDataRefreshed();
														})
														.HintText(FText::FromString(TEXT("Filter...")))
												]
										]

									+ SVerticalBox::Slot()
										.FillHeight(1.0f)
										[
											SNew(SScrollBox)
											+ SScrollBox::Slot()
												[
													_PumpContainer.ToSharedRef()
												]
										]
								]
						]
			]
	];

	if (_ViewModel.IsValid())
	{
		_DataRefreshedHandle = _ViewModel->OnDataRefreshed.AddRaw(
			this, &SCkSchedulerDebugger_ProcessorTree::DoOnDataRefreshed);
	}
}

// --------------------------------------------------------------------------------------------------------------------

SCkSchedulerDebugger_ProcessorTree::~SCkSchedulerDebugger_ProcessorTree()
{
	if (_ViewModel.IsValid())
	{
		_ViewModel->OnDataRefreshed.Remove(_DataRefreshedHandle);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoOnDataRefreshed()
	-> void
{
	if (_DisplayRoots.Num() == 0)
	{
		DoRebuildFlattenedTree();
	}
	else if (_TreeView.IsValid())
	{
		// RebuildList forces STreeView to discard cached row widgets and re-call DoGenerateRow
		// for all visible items. This is necessary because row content (timing text, badges) is
		// baked at creation time, not bound via lambdas. Without this, scrubbing to a historical
		// frame via the frame history bar would show stale timing values.
		_TreeView->RebuildList();
	}

	if (NOT _ViewModel.IsValid() || NOT _PumpContainer.IsValid())
	{ return; }

	const auto& Procs = _ViewModel->Get_DataCollector().Get_Processors();
	const auto PumpCount = _ViewModel->Get_DataCollector().Get_PumpCount();

	// Compute a hash of the pump data so we only rebuild when content actually changes.
	// This avoids Slate layout measurement issues (overlapping text) from rebuilding every frame,
	// while still catching changes the old _LastPumpCount guard missed (same count, different processors).
	// Hash includes both main-pass and pump data so the pane updates when either changes
	// (e.g., when scrubbing to a historical frame with different timing).
	auto PumpDataHash = GetTypeHash(PumpCount);
	for (const auto& Proc : Procs)
	{
		PumpDataHash = HashCombine(PumpDataHash, GetTypeHash(static_cast<int32>(Proc.MainPassTimeMs * 10000.0)));
		if (Proc.PumpCountThisFrame > 0)
		{
			PumpDataHash = HashCombine(PumpDataHash, GetTypeHash(Proc.ProcessorName));
			PumpDataHash = HashCombine(PumpDataHash, GetTypeHash(Proc.PumpCountThisFrame));
			for (const auto PumpTimeMs : Proc.PumpPassTimesMs)
			{
				PumpDataHash = HashCombine(PumpDataHash, GetTypeHash(static_cast<int32>(PumpTimeMs * 10000.0)));
			}
		}
	}
	// Also hash the selected frame offset and filter so scrubbing/searching triggers a rebuild
	if (_ViewModel.IsValid())
	{
		PumpDataHash = HashCombine(PumpDataHash, GetTypeHash(_ViewModel->Get_SelectedFrameOffset()));
	}
	PumpDataHash = HashCombine(PumpDataHash, GetTypeHash(_BreakdownFilterString));

	if (PumpDataHash == _LastPumpDataHash)
	{ return; }
	_LastPumpDataHash = PumpDataHash;

	// Build a fresh widget tree and swap it in atomically via SetContent().
	// This avoids the SVerticalBox slot mutation layout measurement bug that causes overlapping text.

	auto NewContent = SNew(SVerticalBox);

	// ---- MAIN PASS section: show all processors that ticked, sorted by timing (descending)
	{
		auto MainPassIndices = TArray<int32>{};
		auto MainPassTotalMs = 0.0;
		for (auto ProcIdx = 0; ProcIdx < Procs.Num(); ++ProcIdx)
		{
			const auto& Proc = Procs[ProcIdx];
			if (Proc.MainPassTimeMs > 0.0 && NOT Proc.IsGroupStart && NOT Proc.IsGroupEnd && NOT Proc.IsGhost)
			{
				if (NOT _BreakdownFilterString.IsEmpty()
					&& NOT Proc.DisplayName.Contains(_BreakdownFilterString, ESearchCase::IgnoreCase))
				{ continue; }

				MainPassIndices.Add(ProcIdx);
				MainPassTotalMs += Proc.MainPassTimeMs;
			}
		}

		MainPassIndices.Sort([&Procs](int32 A, int32 B)
		{
			return Procs[A].MainPassTimeMs > Procs[B].MainPassTimeMs;
		});

		NewContent->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("Main Pass (%d) %.3f ms"),
						MainPassIndices.Num(), MainPassTotalMs)))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
					.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Active)
			];

		for (const auto ProcIdx : MainPassIndices)
		{
			const auto& Proc = Procs[ProcIdx];
			auto CapturedIdx = ProcIdx;

			NewContent->AddSlot()
				.AutoHeight()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
						.ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.ContentPadding(FMargin(0.0f, 1.0f))
						.OnClicked_Lambda([this, CapturedIdx]() -> FReply
						{
							if (_ViewModel.IsValid())
							{
								_ViewModel->Set_SelectedProcessorIndex(CapturedIdx);
							}
							return FReply::Handled();
						})
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(FText::FromString(Proc.DisplayName))
										.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
										.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Selection)
										.AutoWrapText(true)
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
								[
									SNew(SBox)
										.WidthOverride(35.0f)
										[
											SNew(STextBlock)
												.Text(FText::FromString(FString::Printf(TEXT("%d"),
													Proc.MainPassEntityCount)))
												.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
												.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
												.Justification(ETextJustify::Right)
										]
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
										.Text(FText::FromString(FString::Printf(TEXT("%.3f ms"),
											Proc.MainPassTimeMs)))
										.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
										.ColorAndOpacity(FCkSchedulerDebuggerStyle::Get_TimingColor(
											Proc.MainPassTimeMs))
								]
						]
				];
		}
	}

	if (PumpCount == 0)
	{
		_PumpContainer->SetContent(NewContent);
		return;
	}

	// ---- Per-pass breakdown (collapsed to summary when > 5 passes)

	const auto ShowAllPasses = PumpCount <= 5;
	const auto PassesToShow = ShowAllPasses ? PumpCount : 3;

	for (auto PassIdx = 0; PassIdx < PassesToShow; ++PassIdx)
	{
		NewContent->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("Pass %d:"), PassIdx + 1)))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
					.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Pumped)
			];

		for (auto ProcIdx = 0; ProcIdx < Procs.Num(); ++ProcIdx)
		{
			const auto& Proc = Procs[ProcIdx];
			if (NOT Proc.PumpPassTimesMs.IsValidIndex(PassIdx) || Proc.PumpPassTimesMs[PassIdx] <= 0.0)
			{ continue; }
			if (NOT _BreakdownFilterString.IsEmpty()
				&& NOT Proc.DisplayName.Contains(_BreakdownFilterString, ESearchCase::IgnoreCase))
			{ continue; }

			auto CapturedProcIdx = ProcIdx;

			NewContent->AddSlot()
				.AutoHeight()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
						.ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.ContentPadding(FMargin(0.0f, 1.0f))
						.OnClicked_Lambda([this, CapturedProcIdx]() -> FReply
						{
							if (_ViewModel.IsValid())
							{
								_ViewModel->Set_SelectedProcessorIndex(CapturedProcIdx);
							}
							return FReply::Handled();
						})
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(FText::FromString(Proc.DisplayName))
										.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
										.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Selection)
										.AutoWrapText(true)
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
								[
									SNew(SBox)
										.WidthOverride(35.0f)
										[
											SNew(STextBlock)
												.Text(FText::FromString(FString::Printf(TEXT("%d"),
													Proc.PumpPassEntityCounts.IsValidIndex(PassIdx)
														? Proc.PumpPassEntityCounts[PassIdx] : 0)))
												.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
												.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
												.Justification(ETextJustify::Right)
										]
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
										.Text(FText::FromString(FString::Printf(TEXT("%.3f ms"),
											Proc.PumpPassTimesMs[PassIdx])))
										.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
										.ColorAndOpacity(FCkSchedulerDebuggerStyle::Get_TimingColor(
											Proc.PumpPassTimesMs[PassIdx]))
								]
						]
				];
		}
	}

	if (NOT ShowAllPasses)
	{
		NewContent->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("... %d more passes (same processors) ..."), PumpCount - PassesToShow)))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
					.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
			];
	}

	// ---- PUMP CYCLE ANALYSIS section

	NewContent->AddSlot()
		.AutoHeight()
		.Padding(0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("CYCLE ANALYSIS")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Warning)
		];

	// Group pumped processors by dirty marker
	auto MarkerGroups = TMap<uint32, TArray<int32>>{};
	for (auto ProcIdx = 0; ProcIdx < Procs.Num(); ++ProcIdx)
	{
		const auto& Proc = Procs[ProcIdx];
		if (Proc.PumpCountThisFrame > 0 && Proc.HasDirtyMarker)
		{
			if (NOT _BreakdownFilterString.IsEmpty()
				&& NOT Proc.DisplayName.Contains(_BreakdownFilterString, ESearchCase::IgnoreCase))
			{ continue; }

			MarkerGroups.FindOrAdd(Proc.DirtyMarkerHash).Add(ProcIdx);
		}
	}

	if (MarkerGroups.IsEmpty())
	{
		NewContent->AddSlot()
			.AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("No dirty processors")))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
					.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
			];
	}
	else
	{
		for (const auto& [MarkerHash, MemberIndices] : MarkerGroups)
		{
			// Determine the dirty marker name from the first member
			auto MarkerDisplayName = FString{TEXT("Unknown")};
			if (MemberIndices.Num() > 0 && Procs.IsValidIndex(MemberIndices[0]))
			{
				const auto& MarkerName = Procs[MemberIndices[0]].DirtyMarkerName;
				if (NOT MarkerName.IsNone())
				{
					MarkerDisplayName = MarkerName.ToString();
					// Strip common prefixes for readability
					MarkerDisplayName.RemoveFromStart(TEXT("ck::"));
					MarkerDisplayName.RemoveFromStart(TEXT("FTag_"));
					MarkerDisplayName.RemoveFromStart(TEXT("F"));
				}
			}

			const auto AllMembersHitAllPasses = [&]()
			{
				for (const auto MemberIdx : MemberIndices)
				{
					if (Procs[MemberIdx].PumpCountThisFrame < PumpCount)
					{ return false; }
				}
				return true;
			}();

			auto CycleColor = AllMembersHitAllPasses
				? FCkSchedulerDebuggerStyle::Color_Warning
				: FCkSchedulerDebuggerStyle::Color_Pumped;

			NewContent->AddSlot()
				.AutoHeight()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 2.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
						[
							SNew(STextBlock)
								.Text(FText::FromString(FString(TEXT("\u25CF"))))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(CycleColor)
						]

					+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(MarkerDisplayName))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
								.ColorAndOpacity(CycleColor)
								.AutoWrapText(true)
						]
				];

			for (const auto MemberIdx : MemberIndices)
			{
				const auto& Member = Procs[MemberIdx];
				auto CapturedIdx = MemberIdx;

				NewContent->AddSlot()
					.AutoHeight()
					.Padding(FCkSchedulerDebuggerStyle::Padding_Large, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
							.ButtonStyle(FCoreStyle::Get(), "NoBorder")
							.ContentPadding(FMargin(0.0f, 1.0f))
							.OnClicked_Lambda([this, CapturedIdx]() -> FReply
							{
								if (_ViewModel.IsValid())
								{
									_ViewModel->Set_SelectedProcessorIndex(CapturedIdx);
								}
								return FReply::Handled();
							})
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
											.Text(FText::FromString(FString::Printf(TEXT("%s (x%d)"),
												*Member.DisplayName, Member.PumpCountThisFrame)))
											.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
											.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Selection)
											.AutoWrapText(true)
									]
							]
					];
			}
		}
	}

	_PumpContainer->SetContent(NewContent);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoRebuildFlattenedTree()
	-> void
{
	if (NOT _ViewModel.IsValid())
	{ return; }

	_DisplayRoots = _ViewModel->Get_DataCollector().Get_TreeRoots();

	DoApplySort();

	if (NOT _FilterString.IsEmpty())
	{
		DoApplyFilter(FText::FromString(_FilterString));
	}

	if (_TreeView.IsValid())
	{
		TFunction<void(const TSharedPtr<FCkSchedulerDebugger_TreeNode>&)> ExpandRecursive =
			[&](const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode)
		{
			_TreeView->SetItemExpansion(InNode, InNode->IsExpanded);
			for (const auto& Child : InNode->Children)
			{
				ExpandRecursive(Child);
			}
		};

		for (const auto& Root : _DisplayRoots)
		{
			ExpandRecursive(Root);
		}
		_TreeView->RequestTreeRefresh();
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoApplyFilter(
		const FText& InFilterText)
	-> void
{
	_FilterString = InFilterText.ToString();

	TFunction<bool(const TSharedPtr<FCkSchedulerDebugger_TreeNode>&, const FString&)> ApplyFilterRecursive =
		[&](const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode, const FString& InFilter) -> bool
	{
		if (InFilter.IsEmpty())
		{
			InNode->IsVisible = true;
			for (const auto& Child : InNode->Children)
			{
				ApplyFilterRecursive(Child, InFilter);
			}
			return true;
		}

		if (InNode->Children.Num() > 0)
		{
			auto AnyChildVisible = false;
			for (const auto& Child : InNode->Children)
			{
				AnyChildVisible |= ApplyFilterRecursive(Child, InFilter);
			}
			InNode->IsVisible = AnyChildVisible || DoMatchesFilter(*InNode, InFilter);
			return InNode->IsVisible;
		}

		InNode->IsVisible = DoMatchesFilter(*InNode, InFilter);
		return InNode->IsVisible;
	};

	for (const auto& Root : _DisplayRoots)
	{
		ApplyFilterRecursive(Root, _FilterString);
	}

	if (_TreeView.IsValid())
	{
		_TreeView->RequestTreeRefresh();
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoApplySort()
	-> void
{
	if (NOT _ViewModel.IsValid())
	{ return; }

	const auto& Procs = _ViewModel->Get_DataCollector().Get_Processors();

	auto SortChildren = [&](TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>>& InChildren)
	{
		InChildren.Sort([&](const TSharedPtr<FCkSchedulerDebugger_TreeNode>& A,
			const TSharedPtr<FCkSchedulerDebugger_TreeNode>& B)
		{
			if (A->Type != ECkSchedulerDebugger_TreeNodeType::Processor
				|| B->Type != ECkSchedulerDebugger_TreeNodeType::Processor)
			{
				return false;
			}

			const auto IdxA = A->ProcessorIndex;
			const auto IdxB = B->ProcessorIndex;
			if (NOT Procs.IsValidIndex(IdxA) || NOT Procs.IsValidIndex(IdxB))
			{ return false; }

			switch (_SortMode)
			{
			case ECkSchedulerDebugger_SortMode::Name:
				return Procs[IdxA].DisplayName < Procs[IdxB].DisplayName;

			case ECkSchedulerDebugger_SortMode::Timing:
				return Procs[IdxA].MainPassTimeMs > Procs[IdxB].MainPassTimeMs;

			case ECkSchedulerDebugger_SortMode::ExecutionOrder:
			default:
				return Procs[IdxA].ExecutionOrder < Procs[IdxB].ExecutionOrder;
			}
		});
	};

	TFunction<void(TSharedPtr<FCkSchedulerDebugger_TreeNode>&)> SortRecursive =
		[&](TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode)
	{
		for (auto& Child : InNode->Children)
		{
			if (Child->Type == ECkSchedulerDebugger_TreeNodeType::Group)
			{
				SortRecursive(Child);
			}
		}
		SortChildren(InNode->Children);
	};

	for (auto& Root : _DisplayRoots)
	{
		SortRecursive(Root);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoMatchesFilter(
		const FCkSchedulerDebugger_TreeNode& InNode,
		const FString& InFilter) const
	-> bool
{
	return InNode.DisplayName.Contains(InFilter, ESearchCase::IgnoreCase);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoGenerateRow(
		TSharedPtr<FCkSchedulerDebugger_TreeNode> InItem,
		const TSharedRef<STableViewBase>& InOwnerTable)
	-> TSharedRef<ITableRow>
{
	return SNew(STableRow<TSharedPtr<FCkSchedulerDebugger_TreeNode>>, InOwnerTable)
		.Padding(FMargin(0.0f, 1.0f))
		.ShowSelection(true)
		.Content()
		[
			DoBuildRowContent(InItem, _ViewModel)
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoGetChildren(
		TSharedPtr<FCkSchedulerDebugger_TreeNode> InItem,
		TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>>& OutChildren)
	-> void
{
	if (NOT InItem.IsValid())
	{ return; }

	for (const auto& Child : InItem->Children)
	{
		if (Child->IsVisible)
		{
			OutChildren.Add(Child);
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoOnSelectionChanged(
		TSharedPtr<FCkSchedulerDebugger_TreeNode> InItem,
		ESelectInfo::Type InSelectInfo)
	-> void
{
	if (NOT _ViewModel.IsValid())
	{ return; }

	if (InItem.IsValid() && InItem->Type == ECkSchedulerDebugger_TreeNodeType::Processor)
	{
		_ViewModel->Set_SelectedProcessorIndex(InItem->ProcessorIndex);
	}
}

// --------------------------------------------------------------------------------------------------------------------
