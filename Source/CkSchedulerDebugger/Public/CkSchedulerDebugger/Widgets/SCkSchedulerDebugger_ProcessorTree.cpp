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
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Selection)
						.Padding(FMargin(4.0f, 1.0f))
						[
							SNew(STextBlock)
								.Text(FText::AsNumber(TotalChildren))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Highlight)
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

	_PumpContainer = SNew(SVerticalBox);

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
											SNew(STextBlock)
												.Text(FText::FromString(TEXT("Pump This Frame")))
												.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
												.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
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
		_TreeView->RequestTreeRefresh();
	}

	if (NOT _ViewModel.IsValid())
	{ return; }

	const auto PumpCount = _ViewModel->Get_DataCollector().Get_PumpCount();

	if (PumpCount == _LastPumpCount)
	{ return; }

	_LastPumpCount = PumpCount;
	_PumpContainer->ClearChildren();

	if (PumpCount == 0)
	{
		_PumpContainer->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("No pumps this frame")))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
					.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
			];
		return;
	}

	const auto& Procs = _ViewModel->Get_DataCollector().Get_Processors();
	for (auto PassIdx = 0; PassIdx < PumpCount; ++PassIdx)
	{
		_PumpContainer->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("Pass %d:"), PassIdx + 1)))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
					.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Pumped)
			];

		for (const auto& Proc : Procs)
		{
			if (Proc.PumpPassTimesMs.IsValidIndex(PassIdx) && Proc.PumpPassTimesMs[PassIdx] > 0.0)
			{
				_PumpContainer->AddSlot()
					.AutoHeight()
					.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 1.0f, 0.0f, 1.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Proc.DisplayName))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
									.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
									.AutoWrapText(true)
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
					];
			}
		}
	}
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
		for (const auto& Root : _DisplayRoots)
		{
			_TreeView->SetItemExpansion(Root, Root->IsExpanded);
			for (const auto& Child : Root->Children)
			{
				_TreeView->SetItemExpansion(Child, Child->IsExpanded);
			}
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

	if (_FilterString.IsEmpty())
	{
		for (const auto& Root : _DisplayRoots)
		{
			Root->IsVisible = true;
			for (const auto& Child : Root->Children)
			{
				Child->IsVisible = true;
				for (const auto& Grandchild : Child->Children)
				{
					Grandchild->IsVisible = true;
				}
			}
		}
	}
	else
	{
		for (const auto& Root : _DisplayRoots)
		{
			auto AnyChildVisible = false;
			for (const auto& Child : Root->Children)
			{
				if (Child->Type == ECkSchedulerDebugger_TreeNodeType::Group)
				{
					auto AnyGrandchildVisible = false;
					for (const auto& Grandchild : Child->Children)
					{
						Grandchild->IsVisible = DoMatchesFilter(*Grandchild, _FilterString);
						AnyGrandchildVisible |= Grandchild->IsVisible;
					}
					Child->IsVisible = AnyGrandchildVisible
						|| DoMatchesFilter(*Child, _FilterString);
					AnyChildVisible |= Child->IsVisible;
				}
				else
				{
					Child->IsVisible = DoMatchesFilter(*Child, _FilterString);
					AnyChildVisible |= Child->IsVisible;
				}
			}
			Root->IsVisible = AnyChildVisible;
		}
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

	for (auto& Root : _DisplayRoots)
	{
		for (auto& Child : Root->Children)
		{
			if (Child->Type == ECkSchedulerDebugger_TreeNodeType::Group)
			{
				SortChildren(Child->Children);
			}
		}
		SortChildren(Root->Children);
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
