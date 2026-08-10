#include "SCkSchedulerDebugger_ProcessorTree.h"

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"

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
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebugger_Axes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	// --------------------------------------------------------------------------------------------------------------------
	// Processor row — all dynamic values bound via lambdas that read live from
	// the data collector at paint time. Widget is built ONCE per tree structure
	// change; timing + count + badges update automatically without rebuild.
	// --------------------------------------------------------------------------------------------------------------------
	auto DoBuildProcessorRowContent(
		const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode,
		const TSharedPtr<FCkSchedulerDebugger_ViewModel>& InViewModel)
		-> TSharedRef<SWidget>
	{
		const auto WeakVM = TWeakPtr<FCkSchedulerDebugger_ViewModel>(InViewModel);
		const auto WeakNode = TWeakPtr<FCkSchedulerDebugger_TreeNode>(InNode);
		const auto ProcIdx = InNode ? InNode->ProcessorIndex : INDEX_NONE;
		const auto NodeName = InNode ? InNode->DisplayName : FString{};

		// Helper to safely fetch the proc for this row. Callers must null-check.
		const auto LookupProc = [WeakVM, ProcIdx]() -> const FCkSchedulerDebugger_ProcessorInfo*
		{
			if (const auto VM = WeakVM.Pin())
			{
				if (ProcIdx != INDEX_NONE)
				{
					const auto& Procs = VM->Get_DataCollector().Get_Processors();
					if (Procs.IsValidIndex(ProcIdx))
					{
						return &Procs[ProcIdx];
					}
				}
			}
			return nullptr;
		};

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
								.Text_Lambda([LookupProc]()
								{
									const auto* Proc = LookupProc();
									return Proc ? FText::FromString(FString::Printf(TEXT("#%d"), Proc->ExecutionOrder))
									            : FText::GetEmpty();
								})
								.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
								.ColorAndOpacity(CkStyle::TextMute())
								.Justification(ETextJustify::Right)
						]
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(NodeName))
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
						.ColorAndOpacity_Lambda([LookupProc, WeakNode]() -> FSlateColor
						{
							// Highlight-mode dim takes precedence so non-matches read clearly.
							if (const auto Node = WeakNode.Pin();
								Node.IsValid() && NOT Node->IsSearchMatch)
							{ return FSlateColor(CkStyle::TextMute()); }

							const auto* Proc = LookupProc();
							const auto IsGhost = Proc && Proc->IsGhost;
							return FSlateColor(IsGhost
								? CkStyle::None()
								: CkStyle::Text());
						})
				];

		// Dirty marker — always in the tree; visibility controlled via attribute.
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(FString(TEXT("\u25CF"))))
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
					.ColorAndOpacity(CkStyle::Warn())
					.ToolTipText(FText::FromString(TEXT("Has dirty marker")))
					.Visibility_Lambda([LookupProc]()
					{
						const auto* Proc = LookupProc();
						return (Proc && Proc->HasDirtyMarker) ? EVisibility::Visible : EVisibility::Collapsed;
					})
			];

		// Parallel marker — same pattern.
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(FString(TEXT("\u25C6"))))
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
					.ColorAndOpacity(CkStyle::Info())
					.ToolTipText(FText::FromString(TEXT("Parallel processor")))
					.Visibility_Lambda([LookupProc]()
					{
						const auto* Proc = LookupProc();
						return (Proc && Proc->IsParallel) ? EVisibility::Visible : EVisibility::Collapsed;
					})
			];

		// Entity count — bound.
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(SBox)
					.WidthOverride(40.0f)
					[
						SNew(STextBlock)
							.Text_Lambda([LookupProc]()
							{
								const auto* Proc = LookupProc();
								return Proc ? FText::AsNumber(Proc->MainPassEntityCount) : FText::GetEmpty();
							})
							.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
							.ColorAndOpacity(CkStyle::TextMute())
							.Justification(ETextJustify::Right)
							.ToolTipText(FText::FromString(TEXT("Entity count")))
					]
			];

		// Timing — bound text + bound heat color.
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(STextBlock)
					.Text_Lambda([LookupProc]()
					{
						const auto* Proc = LookupProc();
						return Proc ? FText::FromString(FString::Printf(TEXT("%.3f ms"), Proc->MainPassTimeMs))
						            : FText::GetEmpty();
					})
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
					.ColorAndOpacity_Lambda([LookupProc]() -> FSlateColor
					{
						const auto* Proc = LookupProc();
						return FSlateColor(Proc
							? FCkSchedulerDebuggerStyle::Get_TimingColor(Proc->MainPassTimeMs)
							: CkStyle::Ok());
					})
			];

		return Row;
	}

	// --------------------------------------------------------------------------------------------------------------------
	// Group row — accent color + aggregate timing bound via lambdas.
	// --------------------------------------------------------------------------------------------------------------------
	auto DoBuildGroupRowContent(
		const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode,
		const TSharedPtr<FCkSchedulerDebugger_ViewModel>& InViewModel)
		-> TSharedRef<SWidget>
	{
		const auto WeakVM = TWeakPtr<FCkSchedulerDebugger_ViewModel>(InViewModel);
		const auto GroupIdx = InNode ? InNode->GroupIndex : INDEX_NONE;
		const auto NodeName = InNode ? InNode->DisplayName : FString{};
		const auto ChildCount = InNode ? InNode->Children.Num() : 0;

		const auto LookupGroup = [WeakVM, GroupIdx]() -> const FCkSchedulerDebugger_GroupInfo*
		{
			if (const auto VM = WeakVM.Pin())
			{
				if (GroupIdx != INDEX_NONE)
				{
					const auto& Groups = VM->Get_DataCollector().Get_Groups();
					if (Groups.IsValidIndex(GroupIdx))
					{
						return &Groups[GroupIdx];
					}
				}
			}
			return nullptr;
		};

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
								.ColorAndOpacity_Lambda([LookupGroup]() -> FLinearColor
								{
									const auto* Group = LookupGroup();
									return Group ? Group->AccentColor : FCkSchedulerDebuggerStyle::Get_GroupColor_Default();
								})
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 1.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(NodeName))
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
						.ColorAndOpacity_Lambda([LookupGroup]() -> FSlateColor
						{
							const auto* Group = LookupGroup();
							return FSlateColor(Group ? Group->AccentColor : FCkSchedulerDebuggerStyle::Get_GroupColor_Default());
						})
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::AsNumber(ChildCount))
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
						.ColorAndOpacity(CkStyle::TextMute())
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
						.Text_Lambda([LookupGroup]()
						{
							const auto* Group = LookupGroup();
							return Group ? FText::FromString(FString::Printf(TEXT("%.2f ms"), Group->AggregateTimeMs))
							             : FText::GetEmpty();
						})
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
						.ColorAndOpacity(CkStyle::TextDim())
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
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_H3)
						.ColorAndOpacity(CkStyle::Text())
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(CkStyle::Selection().CopyWithNewOpacity(0.3f))
						.Padding(FMargin(4.0f, 1.0f))
						[
							SNew(STextBlock)
								.Text(FText::AsNumber(TotalChildren))
								.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
								.ColorAndOpacity(CkStyle::TextDim())
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

	// Breakdown pane — group header row: accent bar + bold name + running count + aggregate timing.
	// Mirrors DoBuildGroupRowContent exactly but accepts explicit params so pump-pass aggregates
	// (which aren't pre-computed on FCkSchedulerDebugger_GroupInfo) can be passed directly.
	auto DoBuildBreakdownGroupHeader(
		const FString&       InDisplayName,
		const FLinearColor&  InAccentColor,
		int32                InRunningCount,
		double               InAggregateTimeMs)
		-> TSharedRef<SWidget>
	{
		const auto AggregateText = InAggregateTimeMs > 0.0
			? FString::Printf(TEXT("%.2f ms"), InAggregateTimeMs)
			: FString{};

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
								.ColorAndOpacity(InAccentColor)
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 1.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(InDisplayName))
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
						.ColorAndOpacity(InAccentColor)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::AsNumber(InRunningCount))
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
						.ColorAndOpacity(CkStyle::TextMute())
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
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
						.ColorAndOpacity(CkStyle::TextDim())
				];
	}

	// Breakdown pane — processor row: #ExecOrder + name + dirty dot + entity count + heat timing.
	// Mirrors DoBuildProcessorRowContent exactly but accepts explicit time/entity values so the same
	// layout can be reused for both the main pass and each individual pump pass.
	auto DoBuildBreakdownProcessorRow(
		const FCkSchedulerDebugger_ProcessorInfo& InProc,
		double                                    InTimeMs,
		int32                                     InEntityCount)
		-> TSharedRef<SWidget>
	{
		const auto ExecOrderText = FString::Printf(TEXT("#%d"), InProc.ExecutionOrder);
		const auto TimingText    = FString::Printf(TEXT("%.3f ms"), InTimeMs);
		const auto EntityText    = FString::Printf(TEXT("%d"), InEntityCount);
		const auto bIdle         = InEntityCount == 0;
		const auto NameColor     = bIdle ? CkStyle::None() : CkStyle::Text();
		const auto TimingColor   = bIdle ? CkStyle::None() : FCkSchedulerDebuggerStyle::Get_TimingColor(InTimeMs);

		auto Row = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 1.0f, FCkSchedulerDebuggerStyle::Padding_Small, 1.0f)
				[
					SNew(SBox)
						.WidthOverride(30.0f)
						[
							SNew(SCkDebug_SelectableLabel)
								.Text(FText::FromString(ExecOrderText))
								.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
								.ColorAndOpacity(CkStyle::TextMute())
						]
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(SCkDebug_SelectableLabel)
						.Text(FText::FromString(InProc.DisplayName))
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
						.ColorAndOpacity(NameColor)
				];

		if (InProc.HasDirtyMarker)
		{
			Row->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(FString(TEXT("\u25CF"))))
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
						.ColorAndOpacity(CkStyle::Warn())
						.ToolTipText(FText::FromString(TEXT("Has dirty marker")))
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
						SNew(SCkDebug_SelectableLabel)
							.Text(FText::FromString(EntityText))
							.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
							.ColorAndOpacity(CkStyle::TextMute())
					]
			];

		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(SCkDebug_SelectableLabel)
					.Text(FText::FromString(TimingText))
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
					.ColorAndOpacity(TimingColor)
			];

		return Row;
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
						SNew(SCkDebug_DualSearchBar)
							.FilterHintText(FText::FromString(TEXT("Filter processors…")))
							.HighlightHintText(FText::FromString(TEXT("Highlight…")))
							.OnFilterTextChanged_Lambda([this](const FString& InText)
							{
								DoApplyFilter(FText::FromString(InText));
							})
							.OnHighlightTextChanged_Lambda([this](const FString& InText)
							{
								if (_HighlightString == InText) { return; }
								_HighlightString = InText;
								DoApplyFilter(FText::FromString(_FilterString));
							})
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
									.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body);
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
									.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
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
								.OnContextMenuOpening(this, &SCkSchedulerDebugger_ProcessorTree::DoOnContextMenuOpening)
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
														.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
														.ColorAndOpacity(CkStyle::TextDim())
												]

											+ SHorizontalBox::Slot()
												.AutoWidth()
												.VAlign(VAlign_Center)
												.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
												[
													SNew(SCkDebug_IconToggle)
														.IconId(TEXT("Moon"))
														.Label(FText::FromString(TEXT("Hide idle processors")))
														.ToolTip(FText::FromString(TEXT("Hide idle processors (zero entity count).")))
														.IsOn_Lambda([this]()
														{
															return _BreakdownHideIdle;
														})
														.OnStateChanged_Lambda([this](bool InIsOn)
														{
															_BreakdownHideIdle = InIsOn;
															_LastPumpDataHash = 0;
															DoOnDataRefreshed();
														})
												]

											+ SHorizontalBox::Slot()
												.FillWidth(1.0f)
												[
													SNew(SCkDebug_DualSearchBar)
														.FilterHintText(FText::FromString(TEXT("Filter…")))
														.HighlightHintText(FText::FromString(TEXT("Highlight…")))
														.OnFilterTextChanged_Lambda([this](const FString& InText)
														{
															if (_BreakdownFilterString == InText) { return; }
															_BreakdownFilterString = InText;
															_LastPumpDataHash = 0;
															DoOnDataRefreshed();
														})
														.OnHighlightTextChanged_Lambda([this](const FString& InText)
														{
															// Dim binding reads this at paint time; no rebuild needed.
															_BreakdownHighlightString = InText;
														})
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
	// POLICY — see SCkDebugger_WindowBase.h for the full rules. Summary:
	//   - Widget trees built ONCE per true topology change.
	//   - Per-frame dynamic values (timing, counts, dirty flags, pump state)
	//     flow via TAttribute<FText>/<EVisibility> lambdas on pre-allocated rows.
	//   - ZERO structural mutation during Tick → zero scrunch.

	// Tree view: rows are TAttribute-bound, so they self-update. Only rebuild
	// when the flat structure is empty (first call after construct).
	if (_DisplayRoots.Num() == 0)
	{
		DoRebuildFlattenedTree();
	}

	if (NOT _ViewModel.IsValid() || NOT _PumpContainer.IsValid())
	{ return; }

	const auto& Procs = _ViewModel->Get_DataCollector().Get_Processors();
	const auto LivePumpCount = _ViewModel->Get_DataCollector().Get_PumpCount();

	// Grow-only: once we've seen N passes we pre-allocate that many Pass
	// sections and keep them. Per-frame variance in live PumpCount only
	// toggles section Visibility — no structural change, no rebuild.
	if (LivePumpCount > _MaxObservedPumpCount)
	{
		_MaxObservedPumpCount = LivePumpCount;
	}

	// PURE-topology hash. Per-frame state (PumpCountThisFrame, HasDirtyMarker,
	// timing, counts) is INTENTIONALLY EXCLUDED. Those toggle on/off every
	// few frames during gameplay; including them would trigger rebuild storms.
	// They show live via TAttribute bindings on the pre-allocated rows.
	auto StructuralHash = GetTypeHash(Procs.Num());
	for (const auto& Proc : Procs)
	{
		// Stable-per-topology fields only.
		StructuralHash = HashCombine(StructuralHash, GetTypeHash(Proc.ProcessorName));
		StructuralHash = HashCombine(StructuralHash, GetTypeHash(Proc.IsGroupStart));
		StructuralHash = HashCombine(StructuralHash, GetTypeHash(Proc.IsGroupEnd));
		StructuralHash = HashCombine(StructuralHash, GetTypeHash(Proc.IsGhost));
		// DirtyMarkerName is assigned at processor registration and stays put.
		StructuralHash = HashCombine(StructuralHash, GetTypeHash(Proc.DirtyMarkerName));
	}
	StructuralHash = HashCombine(StructuralHash, GetTypeHash(_MaxObservedPumpCount));
	StructuralHash = HashCombine(StructuralHash, GetTypeHash(_ViewModel->Get_SelectedFrameOffset()));
	StructuralHash = HashCombine(StructuralHash, GetTypeHash(_BreakdownFilterString));
	StructuralHash = HashCombine(StructuralHash, GetTypeHash(_BreakdownHideIdle));

	if (StructuralHash == _LastPumpDataHash)
	{ return; }
	_LastPumpDataHash = StructuralHash;

	// Topology changed (or user event): rebuild the pre-allocated stable tree.
	_PumpContainer->SetContent(DoBuildPumpBreakdown_StableTree());
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoBuildPumpBreakdown_StableTree()
	-> TSharedRef<SWidget>
{
	const auto WeakVM = TWeakPtr<FCkSchedulerDebugger_ViewModel>(_ViewModel);
	const auto FilterString = _BreakdownFilterString;
	const auto HideIdle = _BreakdownHideIdle;
	const auto MaxPasses = _MaxObservedPumpCount;

	const auto& Groups = _ViewModel->Get_DataCollector().Get_Groups();
	const auto& Procs  = _ViewModel->Get_DataCollector().Get_Processors();

	auto GroupOrder = TArray<int32>{};
	for (auto I = 0; I < Groups.Num(); ++I) { GroupOrder.Add(I); }
	GroupOrder.Sort([&Groups](int32 A, int32 B)
	{
		return Groups[A].StartNodeIndex < Groups[B].StartNodeIndex;
	});

	// Shared predicate. Captured by value into every row lambda so each row
	// makes its own filter decision at paint time. The Filter input narrows
	// visibility; the Highlight input only drives row-text dimming and is
	// applied separately in the row's colour binding.
	const auto ProcMatchesFilter = [FilterString](const FCkSchedulerDebugger_ProcessorInfo& Proc) -> bool
	{
		if (Proc.IsGroupStart || Proc.IsGroupEnd || Proc.IsGhost) { return false; }
		if (!FilterString.IsEmpty()
			&& !ck::fuzzy::Match(FilterString, Proc.DisplayName, {}).Get_IsMatch())
		{ return false; }
		return true;
	};

	auto NewContent = SNew(SVerticalBox);

	// ----------------------------------------------------------------------
	// Pass-section builder — generates the Main Pass or one Pump Pass N.
	// Every dynamic value binds to a lambda reading live from the collector.
	// ----------------------------------------------------------------------
	const auto AddPassSection = [&](
		const FString& InLabel,
		int32 InPassIdx,
		bool InAlwaysVisible,
		const FLinearColor& InHeaderColor,
		TFunction<double(const FCkSchedulerDebugger_ProcessorInfo&)> InTimeGetter,
		TFunction<int32(const FCkSchedulerDebugger_ProcessorInfo&)> InCountGetter)
	{
		// Pass section visibility: Main Pass is always visible; Pass N only
		// when live PumpCount > N.
		auto SectionVisAttr = InAlwaysVisible
			? TAttribute<EVisibility>(EVisibility::Visible)
			: TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
				[WeakVM, InPassIdx]() -> EVisibility
				{
					const auto VM = WeakVM.Pin();
					return (VM.IsValid() && InPassIdx < VM->Get_DataCollector().Get_PumpCount())
						? EVisibility::Visible : EVisibility::Collapsed;
				}));

		auto SectionBox = SNew(SVerticalBox).Visibility(SectionVisAttr);

		// Section header — label + running count + aggregate ms (both bound live).
		SectionBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 2.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InLabel))
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
				.ColorAndOpacity(InHeaderColor)
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(InHeaderColor.CopyWithNewOpacity(0.3f))
				.Padding(FMargin(4.0f, 1.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([WeakVM, InTimeGetter, InCountGetter, ProcMatchesFilter, HideIdle]()
					{
						const auto VM = WeakVM.Pin();
						if (!VM.IsValid()) { return FText::AsNumber(0); }
						auto Count = 0;
						for (const auto& Proc : VM->Get_DataCollector().Get_Processors())
						{
							if (!ProcMatchesFilter(Proc)) { continue; }
							if (InTimeGetter(Proc) <= 0.0) { continue; }
							if (HideIdle && InCountGetter(Proc) == 0) { continue; }
							++Count;
						}
						return FText::AsNumber(Count);
					})
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
					.ColorAndOpacity(InHeaderColor)
				]
			]

			+ SHorizontalBox::Slot().FillWidth(1.0f) [ SNew(SSpacer) ]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakVM, InTimeGetter, ProcMatchesFilter]()
				{
					const auto VM = WeakVM.Pin();
					if (!VM.IsValid()) { return FText::GetEmpty(); }
					auto Total = 0.0;
					for (const auto& Proc : VM->Get_DataCollector().Get_Processors())
					{
						if (!ProcMatchesFilter(Proc)) { continue; }
						const auto T = InTimeGetter(Proc);
						if (T > 0.0) { Total += T; }
					}
					return Total > 0.0
						? FText::FromString(FString::Printf(TEXT("%.3f ms"), Total))
						: FText::GetEmpty();
				})
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
				.ColorAndOpacity(CkStyle::TextDim())
			]
		];

		// Per-group subsections within this pass.
		for (const auto GroupIdx : GroupOrder)
		{
			if (!Groups.IsValidIndex(GroupIdx)) { continue; }
			const auto& Group = Groups[GroupIdx];
			const auto GroupAccent = Group.AccentColor;
			const auto GroupName = Group.DisplayName;
			const auto GroupMemberIdxs = Group.MemberIndices;

			// Group visibility: at least one member passes filter + has time > 0 this pass.
			auto GroupVisAttr = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
				[WeakVM, GroupMemberIdxs, InTimeGetter, InCountGetter, ProcMatchesFilter, HideIdle]() -> EVisibility
				{
					const auto VM = WeakVM.Pin();
					if (!VM.IsValid()) { return EVisibility::Collapsed; }
					const auto& AllProcs = VM->Get_DataCollector().Get_Processors();
					for (const auto MIdx : GroupMemberIdxs)
					{
						if (!AllProcs.IsValidIndex(MIdx)) { continue; }
						const auto& Proc = AllProcs[MIdx];
						if (!ProcMatchesFilter(Proc)) { continue; }
						if (InTimeGetter(Proc) <= 0.0) { continue; }
						if (HideIdle && InCountGetter(Proc) == 0) { continue; }
						return EVisibility::Visible;
					}
					return EVisibility::Collapsed;
				}));

			auto GroupBox = SNew(SVerticalBox).Visibility(GroupVisAttr);

			// Group header with bound count + aggregate.
			GroupBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(FCkSchedulerDebuggerStyle::Node_AccentWidth)
					.HeightOverride(20.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.ColorAndOpacity(GroupAccent)
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(GroupName))
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
					.ColorAndOpacity(GroupAccent)
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([WeakVM, GroupMemberIdxs, InTimeGetter, InCountGetter, ProcMatchesFilter, HideIdle]()
					{
						const auto VM = WeakVM.Pin();
						if (!VM.IsValid()) { return FText::AsNumber(0); }
						auto Count = 0;
						const auto& AllProcs = VM->Get_DataCollector().Get_Processors();
						for (const auto MIdx : GroupMemberIdxs)
						{
							if (!AllProcs.IsValidIndex(MIdx)) { continue; }
							const auto& Proc = AllProcs[MIdx];
							if (!ProcMatchesFilter(Proc)) { continue; }
							if (InTimeGetter(Proc) <= 0.0) { continue; }
							if (HideIdle && InCountGetter(Proc) == 0) { continue; }
							++Count;
						}
						return FText::AsNumber(Count);
					})
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
					.ColorAndOpacity(CkStyle::TextMute())
				]

				+ SHorizontalBox::Slot().FillWidth(1.0f) [ SNew(SSpacer) ]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([WeakVM, GroupMemberIdxs, InTimeGetter, ProcMatchesFilter]()
					{
						const auto VM = WeakVM.Pin();
						if (!VM.IsValid()) { return FText::GetEmpty(); }
						auto Total = 0.0;
						const auto& AllProcs = VM->Get_DataCollector().Get_Processors();
						for (const auto MIdx : GroupMemberIdxs)
						{
							if (!AllProcs.IsValidIndex(MIdx)) { continue; }
							const auto& Proc = AllProcs[MIdx];
							if (!ProcMatchesFilter(Proc)) { continue; }
							const auto T = InTimeGetter(Proc);
							if (T > 0.0) { Total += T; }
						}
						return Total > 0.0
							? FText::FromString(FString::Printf(TEXT("%.2f ms"), Total))
							: FText::GetEmpty();
					})
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
					.ColorAndOpacity(CkStyle::TextDim())
				]
			];

			// Per-member rows — pre-allocated; visibility + timing bound live.
			for (const auto MemberIdx : GroupMemberIdxs)
			{
				if (!Procs.IsValidIndex(MemberIdx)) { continue; }
				const auto& MemberProc = Procs[MemberIdx];
				if (MemberProc.IsGroupStart || MemberProc.IsGroupEnd || MemberProc.IsGhost)
				{ continue; }

				const auto CapturedMemberIdx = MemberIdx;
				const auto MemberName = MemberProc.DisplayName;

				auto RowVisAttr = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
					[WeakVM, CapturedMemberIdx, InTimeGetter, InCountGetter, ProcMatchesFilter, HideIdle]() -> EVisibility
					{
						const auto VM = WeakVM.Pin();
						if (!VM.IsValid()) { return EVisibility::Collapsed; }
						const auto& AllProcs = VM->Get_DataCollector().Get_Processors();
						if (!AllProcs.IsValidIndex(CapturedMemberIdx)) { return EVisibility::Collapsed; }
						const auto& Proc = AllProcs[CapturedMemberIdx];
						if (!ProcMatchesFilter(Proc)) { return EVisibility::Collapsed; }
						if (InTimeGetter(Proc) <= 0.0) { return EVisibility::Collapsed; }
						if (HideIdle && InCountGetter(Proc) == 0) { return EVisibility::Collapsed; }
						return EVisibility::Visible;
					}));

				GroupBox->AddSlot().AutoHeight()
					.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FCoreStyle::Get(), "NoBorder")
					.ContentPadding(FMargin(0.0f, 1.0f))
					.Visibility(RowVisAttr)
					.OnClicked_Lambda([this, CapturedMemberIdx]() -> FReply
					{
						if (_ViewModel.IsValid())
						{ _ViewModel->Set_SelectedProcessorIndex(CapturedMemberIdx); }
						return FReply::Handled();
					})
					[
						SNew(SHorizontalBox)

						// ExecOrder (bound)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						.Padding(0.0f, 1.0f, FCkSchedulerDebuggerStyle::Padding_Small, 1.0f)
						[
							SNew(SBox).WidthOverride(30.0f)
							[
								SNew(STextBlock)
								.Text_Lambda([WeakVM, CapturedMemberIdx]()
								{
									const auto VM = WeakVM.Pin();
									if (!VM.IsValid()) { return FText::GetEmpty(); }
									const auto& Arr = VM->Get_DataCollector().Get_Processors();
									return Arr.IsValidIndex(CapturedMemberIdx)
										? FText::FromString(FString::Printf(TEXT("#%d"), Arr[CapturedMemberIdx].ExecutionOrder))
										: FText::GetEmpty();
								})
								.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
								.ColorAndOpacity(CkStyle::TextMute())
								.Justification(ETextJustify::Right)
							]
						]

						// Name (static)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(2.0f, 0.0f)
						[
							SNew(SCkDebug_SelectableLabel)
							.Text(FText::FromString(MemberName))
							.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
							.ColorAndOpacity_Lambda([this, MemberName]() -> FSlateColor
							{
								// Dim when the Highlight input is set and this row doesn't match.
								// Otherwise use the primary colour. (The row is only here at all
								// because it already passed the Filter input.)
								if (NOT _BreakdownHighlightString.IsEmpty()
									&& NOT ck::fuzzy::Match(_BreakdownHighlightString, MemberName, {}).Get_IsMatch())
								{ return FSlateColor(CkStyle::TextMute()); }
								return FSlateColor(CkStyle::Text());
							})
						]

						// Entity count (bound)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
						[
							SNew(SBox).WidthOverride(40.0f)
							[
								SNew(STextBlock)
								.Text_Lambda([WeakVM, CapturedMemberIdx, InCountGetter]()
								{
									const auto VM = WeakVM.Pin();
									if (!VM.IsValid()) { return FText::GetEmpty(); }
									const auto& Arr = VM->Get_DataCollector().Get_Processors();
									return Arr.IsValidIndex(CapturedMemberIdx)
										? FText::AsNumber(InCountGetter(Arr[CapturedMemberIdx]))
										: FText::GetEmpty();
								})
								.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
								.ColorAndOpacity(CkStyle::TextMute())
								.Justification(ETextJustify::Right)
							]
						]

						// Timing (bound text + bound heat color)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
						[
							SNew(STextBlock)
							.Text_Lambda([WeakVM, CapturedMemberIdx, InTimeGetter]()
							{
								const auto VM = WeakVM.Pin();
								if (!VM.IsValid()) { return FText::GetEmpty(); }
								const auto& Arr = VM->Get_DataCollector().Get_Processors();
								return Arr.IsValidIndex(CapturedMemberIdx)
									? FText::FromString(FString::Printf(TEXT("%.3f ms"), InTimeGetter(Arr[CapturedMemberIdx])))
									: FText::GetEmpty();
							})
							.ColorAndOpacity_Lambda([WeakVM, CapturedMemberIdx, InTimeGetter]() -> FSlateColor
							{
								const auto VM = WeakVM.Pin();
								if (!VM.IsValid()) { return FSlateColor(CkStyle::TextMute()); }
								const auto& Arr = VM->Get_DataCollector().Get_Processors();
								return Arr.IsValidIndex(CapturedMemberIdx)
									? FSlateColor(FCkSchedulerDebuggerStyle::Get_TimingColor(InTimeGetter(Arr[CapturedMemberIdx])))
									: FSlateColor(CkStyle::TextMute());
							})
							.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
						]
					]
				];
			}

			SectionBox->AddSlot().AutoHeight() [ GroupBox ];
		}

		NewContent->AddSlot().AutoHeight() [ SectionBox ];
	};

	// ----------------------------------------------------------------------
	// MAIN PASS — always visible.
	// ----------------------------------------------------------------------
	AddPassSection(TEXT("Main Pass"), -1, true, CkStyle::Ok(),
		[](const FCkSchedulerDebugger_ProcessorInfo& P) { return P.MainPassTimeMs; },
		[](const FCkSchedulerDebugger_ProcessorInfo& P) { return P.MainPassEntityCount; });

	// ----------------------------------------------------------------------
	// PUMP PASSES 1..MaxObservedPumpCount — each section self-hides when
	// live PumpCount falls below its index.
	// ----------------------------------------------------------------------
	for (auto PassIdx = 0; PassIdx < MaxPasses; ++PassIdx)
	{
		const auto PassIdxCopy = PassIdx;
		AddPassSection(
			FString::Printf(TEXT("Pass %d"), PassIdx + 1),
			PassIdx, false, CkStyle::Warn(),
			[PassIdxCopy](const FCkSchedulerDebugger_ProcessorInfo& P)
			{
				return P.PumpPassTimesMs.IsValidIndex(PassIdxCopy)
					? P.PumpPassTimesMs[PassIdxCopy] : 0.0;
			},
			[PassIdxCopy](const FCkSchedulerDebugger_ProcessorInfo& P)
			{
				return P.PumpPassEntityCounts.IsValidIndex(PassIdxCopy)
					? P.PumpPassEntityCounts[PassIdxCopy] : 0;
			});
	}

	// ----------------------------------------------------------------------
	// CYCLE ANALYSIS — pre-allocated per proc that has a DirtyMarkerName at
	// topology time. Per-proc Visibility binds to live HasDirtyMarker.
	// Procs are grouped by marker name (stable per topology).
	// ----------------------------------------------------------------------

	// Always-visible section header.
	NewContent->AddSlot().AutoHeight()
		.Padding(0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("CYCLE ANALYSIS")))
		.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
		.ColorAndOpacity(CkStyle::Err())
	];

	// "No dirty processors" stub — visible when none currently dirty.
	{
		auto NoneVisibleAttr = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
			[WeakVM]() -> EVisibility
			{
				const auto VM = WeakVM.Pin();
				if (!VM.IsValid()) { return EVisibility::Visible; }
				for (const auto& Proc : VM->Get_DataCollector().Get_Processors())
				{
					if (Proc.HasDirtyMarker) { return EVisibility::Collapsed; }
				}
				return EVisibility::Visible;
			}));

		NewContent->AddSlot().AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No dirty processors")))
			.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_Micro)
			.ColorAndOpacity(CkStyle::TextMute())
			.Visibility(NoneVisibleAttr)
		];
	}

	// Group procs by marker name at BUILD time (stable per topology).
	auto MarkerGroupsByName = TMap<FString, TArray<int32>>{};
	for (auto Idx = 0; Idx < Procs.Num(); ++Idx)
	{
		const auto& Proc = Procs[Idx];
		if (Proc.IsGroupStart || Proc.IsGroupEnd || Proc.IsGhost) { continue; }
		if (Proc.DirtyMarkerName.IsNone()) { continue; }
		if (!FilterString.IsEmpty()
			&& !ck::fuzzy::Match(FilterString, Proc.DisplayName, {}).Get_IsMatch())
		{ continue; }

		auto Pretty = Proc.DirtyMarkerName.ToString();
		Pretty.RemoveFromStart(TEXT("ck::"));
		Pretty.RemoveFromStart(TEXT("FTag_"));
		Pretty.RemoveFromStart(TEXT("F"));
		MarkerGroupsByName.FindOrAdd(Pretty).Add(Idx);
	}

	for (const auto& [MarkerName, MemberIndices] : MarkerGroupsByName)
	{
		const auto CapturedMemberIndices = MemberIndices;
		const auto CapturedMarkerName = MarkerName;

		// Marker-group visibility: any captured member currently dirty.
		auto GroupVisAttr = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
			[WeakVM, CapturedMemberIndices]() -> EVisibility
			{
				const auto VM = WeakVM.Pin();
				if (!VM.IsValid()) { return EVisibility::Collapsed; }
				const auto& AllProcs = VM->Get_DataCollector().Get_Processors();
				for (const auto MIdx : CapturedMemberIndices)
				{
					if (AllProcs.IsValidIndex(MIdx) && AllProcs[MIdx].HasDirtyMarker)
					{ return EVisibility::Visible; }
				}
				return EVisibility::Collapsed;
			}));

		// Accent color: warn when all members pumped every pass, else pumped color.
		auto ColorAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda(
			[WeakVM, CapturedMemberIndices]() -> FSlateColor
			{
				const auto VM = WeakVM.Pin();
				if (!VM.IsValid()) { return FSlateColor(CkStyle::Warn()); }
				const auto PumpCount = VM->Get_DataCollector().Get_PumpCount();
				const auto& AllProcs = VM->Get_DataCollector().Get_Processors();
				auto AllHit = PumpCount > 0;
				for (const auto MIdx : CapturedMemberIndices)
				{
					if (!AllProcs.IsValidIndex(MIdx) || AllProcs[MIdx].PumpCountThisFrame < PumpCount)
					{ AllHit = false; break; }
				}
				return FSlateColor(AllHit
					? CkStyle::Err()
					: CkStyle::Warn());
			}));

		auto MarkerBox = SNew(SVerticalBox).Visibility(GroupVisAttr);

		// Marker header.
		MarkerBox->AddSlot().AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 2.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString(TEXT("\u25CF"))))
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
				.ColorAndOpacity(ColorAttr)
			]

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(CapturedMarkerName))
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Micro)
				.ColorAndOpacity(ColorAttr)
				.AutoWrapText(true)
			]
		];

		// Per-member rows.
		for (const auto MemberIdx : CapturedMemberIndices)
		{
			const auto CapturedMemberIdx = MemberIdx;
			if (!Procs.IsValidIndex(MemberIdx)) { continue; }
			const auto MemberName = Procs[MemberIdx].DisplayName;

			auto MemberVisAttr = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
				[WeakVM, CapturedMemberIdx]() -> EVisibility
				{
					const auto VM = WeakVM.Pin();
					if (!VM.IsValid()) { return EVisibility::Collapsed; }
					const auto& AllProcs = VM->Get_DataCollector().Get_Processors();
					return (AllProcs.IsValidIndex(CapturedMemberIdx) && AllProcs[CapturedMemberIdx].HasDirtyMarker)
						? EVisibility::Visible : EVisibility::Collapsed;
				}));

			MarkerBox->AddSlot().AutoHeight()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Large, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(0.0f, 1.0f))
				.Visibility(MemberVisAttr)
				.OnClicked_Lambda([this, CapturedMemberIdx]() -> FReply
				{
					if (_ViewModel.IsValid())
					{ _ViewModel->Set_SelectedProcessorIndex(CapturedMemberIdx); }
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text_Lambda([WeakVM, CapturedMemberIdx, MemberName]()
					{
						const auto VM = WeakVM.Pin();
						if (!VM.IsValid()) { return FText::FromString(MemberName); }
						const auto& Arr = VM->Get_DataCollector().Get_Processors();
						if (!Arr.IsValidIndex(CapturedMemberIdx)) { return FText::FromString(MemberName); }
						return FText::FromString(FString::Printf(TEXT("%s (x%d)"),
							*MemberName, Arr[CapturedMemberIdx].PumpCountThisFrame));
					})
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
					.ColorAndOpacity(CkStyle::Selection())
					.AutoWrapText(true)
				]
			];
		}

		NewContent->AddSlot().AutoHeight() [ MarkerBox ];
	}

	return NewContent;
}

// --------------------------------------------------------------------------------------------------------------------
// Orphaned legacy rebuild path removed. The DoBuildPumpBreakdown_StableTree
// above replaces everything this block used to do.
// --------------------------------------------------------------------------------------------------------------------
#if 0
	auto NewContent = SNew(SVerticalBox);

	const auto& Groups = _ViewModel->Get_DataCollector().Get_Groups();

	// Sort groups by their pipeline execution order so the breakdown mirrors the tree view.
	auto GroupOrder = TArray<int32>{};
	for (auto GroupIdx = 0; GroupIdx < Groups.Num(); ++GroupIdx)
	{
		GroupOrder.Add(GroupIdx);
	}
	GroupOrder.Sort([&Groups](int32 A, int32 B)
	{
		return Groups[A].StartNodeIndex < Groups[B].StartNodeIndex;
	});

	// Collect member processor indices from a group that have a non-zero value from InGetTime,
	// pass the breakdown filter, and are not ghost/boundary nodes. Result is sorted by ExecutionOrder.
	const auto CollectRunningMembers = [&](
		int32 InGroupIdx,
		TFunctionRef<double(const FCkSchedulerDebugger_ProcessorInfo&)> InGetTime,
		TFunctionRef<int32(const FCkSchedulerDebugger_ProcessorInfo&)>  InGetCount)
		-> TArray<int32>
	{
		auto Result = TArray<int32>{};
		for (const auto MemberProcIdx : Groups[InGroupIdx].MemberIndices)
		{
			if (NOT Procs.IsValidIndex(MemberProcIdx)) { continue; }
			const auto& Proc = Procs[MemberProcIdx];
			if (Proc.IsGroupStart || Proc.IsGroupEnd || Proc.IsGhost) { continue; }
			if (InGetTime(Proc) <= 0.0) { continue; }
			if (_BreakdownHideIdle && InGetCount(Proc) == 0) { continue; }
			if (NOT _BreakdownFilterString.IsEmpty()
				&& NOT ck::fuzzy::Match(_BreakdownFilterString, Proc.DisplayName, {}).Get_IsMatch())
			{ continue; }
			Result.Add(MemberProcIdx);
		}
		Result.Sort([&Procs](int32 A, int32 B)
		{
			return Procs[A].ExecutionOrder < Procs[B].ExecutionOrder;
		});
		return Result;
	};

	// Section header: styled like a tick group row — bold label, count badge, total timing.
	const auto AddSectionHeader = [&](
		const FString&      InLabel,
		int32               InCount,
		double              InTotalMs,
		const FLinearColor& InColor)
	{
		NewContent->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 2.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(FText::FromString(InLabel))
							.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
							.ColorAndOpacity(InColor)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
					[
						SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.BorderBackgroundColor(InColor.CopyWithNewOpacity(0.3f))
							.Padding(FMargin(4.0f, 1.0f))
							[
								SNew(STextBlock)
									.Text(FText::AsNumber(InCount))
									.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
									.ColorAndOpacity(InColor)
							]
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
							.Text(FText::FromString(InTotalMs > 0.0
								? FString::Printf(TEXT("%.3f ms"), InTotalMs)
								: FString{}))
							.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
							.ColorAndOpacity(CkStyle::TextDim())
					]
			];
	};

	// Helper: emit all running processors for a group as clickable tree-view-style rows.
	const auto AddGroupRows = [&](
		int32                   InGroupIdx,
		const TArray<int32>&    InRunningMembers,
		TFunctionRef<double(const FCkSchedulerDebugger_ProcessorInfo&)> InGetTime,
		TFunctionRef<int32(const FCkSchedulerDebugger_ProcessorInfo&)>  InGetCount)
	{
		const auto& Group    = Groups[InGroupIdx];
		auto        AggrMs   = 0.0;
		for (const auto MIdx : InRunningMembers)
		{
			AggrMs += InGetTime(Procs[MIdx]);
		}

		NewContent->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 1.0f, 0.0f, 0.0f)
			[
				DoBuildBreakdownGroupHeader(
					Group.DisplayName, Group.AccentColor,
					InRunningMembers.Num(), AggrMs)
			];

		for (const auto MemberProcIdx : InRunningMembers)
		{
			const auto& Proc       = Procs[MemberProcIdx];
			const auto  TimeMs     = InGetTime(Proc);
			const auto  EntityCnt  = InGetCount(Proc);
			auto        CapturedIdx = MemberProcIdx;

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
							DoBuildBreakdownProcessorRow(Proc, TimeMs, EntityCnt)
						]
				];
		}
	};

	// ---- MAIN PASS section
	{
		auto TotalMs    = 0.0;
		auto TotalCount = 0;
		for (const auto& Proc : Procs)
		{
			if (Proc.MainPassTimeMs > 0.0 && NOT Proc.IsGroupStart && NOT Proc.IsGroupEnd && NOT Proc.IsGhost)
			{
				if (_BreakdownFilterString.IsEmpty()
					|| ck::fuzzy::Match(_BreakdownFilterString, Proc.DisplayName, {}).Get_IsMatch())
				{
					TotalMs += Proc.MainPassTimeMs;
					TotalCount++;
				}
			}
		}
		AddSectionHeader(TEXT("Main Pass"), TotalCount, TotalMs, CkStyle::Ok());
	}

	for (const auto GroupIdx : GroupOrder)
	{
		const auto RunningMembers = CollectRunningMembers(GroupIdx,
			[](const FCkSchedulerDebugger_ProcessorInfo& P) { return P.MainPassTimeMs; },
			[](const FCkSchedulerDebugger_ProcessorInfo& P) { return P.MainPassEntityCount; });
		if (RunningMembers.IsEmpty()) { continue; }

		AddGroupRows(GroupIdx, RunningMembers,
			[](const FCkSchedulerDebugger_ProcessorInfo& P) { return P.MainPassTimeMs; },
			[](const FCkSchedulerDebugger_ProcessorInfo& P) { return P.MainPassEntityCount; });
	}

	if (PumpCount == 0)
	{
		_PumpContainer->SetContent(NewContent);
		return;
	}

	// ---- Per-pass breakdown (collapsed to summary when > 5 passes)

	const auto ShowAllPasses = PumpCount <= 5;
	const auto PassesToShow  = ShowAllPasses ? PumpCount : 3;

	for (auto PassIdx = 0; PassIdx < PassesToShow; ++PassIdx)
	{
		// Section header for this pass
		{
			auto PassTotalMs = 0.0;
			auto PassCount   = 0;
			for (const auto& Proc : Procs)
			{
				if (Proc.PumpPassTimesMs.IsValidIndex(PassIdx)
					&& Proc.PumpPassTimesMs[PassIdx] > 0.0
					&& NOT Proc.IsGroupStart && NOT Proc.IsGroupEnd && NOT Proc.IsGhost)
				{
					if (_BreakdownFilterString.IsEmpty()
						|| ck::fuzzy::Match(_BreakdownFilterString, Proc.DisplayName, {}).Get_IsMatch())
					{
						PassTotalMs += Proc.PumpPassTimesMs[PassIdx];
						PassCount++;
					}
				}
			}
			AddSectionHeader(
				FString::Printf(TEXT("Pass %d"), PassIdx + 1),
				PassCount, PassTotalMs,
				CkStyle::Warn());
		}

		for (const auto GroupIdx : GroupOrder)
		{
			const auto PassIdxCopy    = PassIdx;
			const auto RunningMembers = CollectRunningMembers(GroupIdx,
				[PassIdxCopy](const FCkSchedulerDebugger_ProcessorInfo& P)
				{
					return P.PumpPassTimesMs.IsValidIndex(PassIdxCopy)
						? P.PumpPassTimesMs[PassIdxCopy] : 0.0;
				},
				[PassIdxCopy](const FCkSchedulerDebugger_ProcessorInfo& P)
				{
					return P.PumpPassEntityCounts.IsValidIndex(PassIdxCopy)
						? P.PumpPassEntityCounts[PassIdxCopy] : 0;
				});
			if (RunningMembers.IsEmpty()) { continue; }

			AddGroupRows(GroupIdx, RunningMembers,
				[PassIdxCopy](const FCkSchedulerDebugger_ProcessorInfo& P)
				{
					return P.PumpPassTimesMs.IsValidIndex(PassIdxCopy)
						? P.PumpPassTimesMs[PassIdxCopy] : 0.0;
				},
				[PassIdxCopy](const FCkSchedulerDebugger_ProcessorInfo& P)
				{
					return P.PumpPassEntityCounts.IsValidIndex(PassIdxCopy)
						? P.PumpPassEntityCounts[PassIdxCopy] : 0;
				});
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
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_Micro)
					.ColorAndOpacity(CkStyle::TextMute())
			];
	}

	// ---- PUMP CYCLE ANALYSIS section

	NewContent->AddSlot()
		.AutoHeight()
		.Padding(0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("CYCLE ANALYSIS")))
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
				.ColorAndOpacity(CkStyle::Err())
		];

	// Group pumped processors by dirty marker
	auto MarkerGroups = TMap<uint32, TArray<int32>>{};
	for (auto ProcIdx = 0; ProcIdx < Procs.Num(); ++ProcIdx)
	{
		const auto& Proc = Procs[ProcIdx];
		if (Proc.PumpCountThisFrame > 0 && Proc.HasDirtyMarker)
		{
			if (NOT _BreakdownFilterString.IsEmpty()
				&& NOT ck::fuzzy::Match(_BreakdownFilterString, Proc.DisplayName, {}).Get_IsMatch())
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
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_Micro)
					.ColorAndOpacity(CkStyle::TextMute())
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
				? CkStyle::Err()
				: CkStyle::Warn();

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
								.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
								.ColorAndOpacity(CycleColor)
						]

					+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(FText::FromString(MarkerDisplayName))
								.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Micro)
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
											.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Micro)
											.ColorAndOpacity(CkStyle::Selection())
											.AutoWrapText(true)
									]
							]
					];
			}
		}
	}

	_PumpContainer->SetContent(NewContent);
}
#endif // 0 — legacy rebuild block (replaced by DoBuildPumpBreakdown_StableTree)

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_ProcessorTree::
	Rebuild_ForStyleChange()
	-> void
{
	// RebuildList re-runs DoGenerateRow for every visible item without touching _DisplayRoots, so the
	// TSharedPtr identity the tree tracks selection and expansion by survives (the pointer-identity
	// contract in CkDebuggerCommon/CLAUDE.md).
	if (_TreeView.IsValid())
	{ _TreeView->RebuildList(); }

	// The pump breakdown is a pre-allocated stable tree keyed on a data hash; poisoning the hash makes
	// the next refresh re-emit it under the new selection.
	_LastPumpDataHash = 0;
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

	// Two-pass pipeline driven by the dual search bar:
	//   Pass 1 (visibility) — _FilterString hides non-matches; ancestors stay
	//                         visible if any descendant matches.
	//   Pass 2 (dim flag)   — _HighlightString flags IsSearchMatch on the
	//                         visible set so the row's name colour can dim
	//                         anything that doesn't match.
	const auto FilterStr = _FilterString;
	const auto HighlightStr = _HighlightString;

	TFunction<bool(const TSharedPtr<FCkSchedulerDebugger_TreeNode>&)> ApplyFilterRecursive =
		[&](const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode) -> bool
	{
		// ---- Visibility (Filter input) ----
		if (FilterStr.IsEmpty())
		{
			InNode->IsVisible = true;
			for (const auto& Child : InNode->Children)
			{ ApplyFilterRecursive(Child); }
		}
		else if (InNode->Children.Num() > 0)
		{
			auto AnyChildVisible = false;
			for (const auto& Child : InNode->Children)
			{ AnyChildVisible |= ApplyFilterRecursive(Child); }
			InNode->IsVisible = AnyChildVisible || DoMatchesFilter(*InNode, FilterStr);
		}
		else
		{
			InNode->IsVisible = DoMatchesFilter(*InNode, FilterStr);
		}

		// ---- Dim flag (Highlight input) ----
		InNode->IsSearchMatch = HighlightStr.IsEmpty() || DoMatchesFilter(*InNode, HighlightStr);

		return InNode->IsVisible;
	};

	for (const auto& Root : _DisplayRoots)
	{
		ApplyFilterRecursive(Root);
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
	const auto& Groups = _ViewModel->Get_DataCollector().Get_Groups();

	// ----------------------------------------------------------------------------------------------------------------
	// Sort-key extraction for any tree node (Processor, Group, TickGroup).
	//
	// A Group node's execution key is the ExecutionOrder of its start ghost node (the first entry in the final
	// topological order that belongs to the group). This matches the on-disk SchedulerOrder.txt dump, where the
	// group appears at the position of its start ghost. Groups also carry an AggregateTimeMs for Timing mode.
	//
	// TickGroup nodes fall back to INT32_MIN so Pre/PostPhysics stay in their natural (already-ordered) positions
	// at the root level.
	// ----------------------------------------------------------------------------------------------------------------

	auto GetExecOrderKey = [&](const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode) -> int32
	{
		switch (InNode->Type)
		{
		case ECkSchedulerDebugger_TreeNodeType::Processor:
		{
			const auto Idx = InNode->ProcessorIndex;
			return Procs.IsValidIndex(Idx) ? Procs[Idx].ExecutionOrder : MAX_int32;
		}
		case ECkSchedulerDebugger_TreeNodeType::Group:
		{
			const auto GroupIdx = InNode->GroupIndex;
			if (NOT Groups.IsValidIndex(GroupIdx))
			{ return MAX_int32; }
			const auto StartNodeIdx = Groups[GroupIdx].StartNodeIndex;
			return Procs.IsValidIndex(StartNodeIdx) ? Procs[StartNodeIdx].ExecutionOrder : MAX_int32;
		}
		default:
			return MIN_int32;
		}
	};

	auto GetTimingKey = [&](const TSharedPtr<FCkSchedulerDebugger_TreeNode>& InNode) -> double
	{
		switch (InNode->Type)
		{
		case ECkSchedulerDebugger_TreeNodeType::Processor:
		{
			const auto Idx = InNode->ProcessorIndex;
			return Procs.IsValidIndex(Idx) ? Procs[Idx].MainPassTimeMs : 0.0;
		}
		case ECkSchedulerDebugger_TreeNodeType::Group:
		{
			const auto GroupIdx = InNode->GroupIndex;
			return Groups.IsValidIndex(GroupIdx) ? Groups[GroupIdx].AggregateTimeMs : 0.0;
		}
		default:
			return 0.0;
		}
	};

	auto SortChildren = [&](TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>>& InChildren)
	{
		InChildren.Sort([&](const TSharedPtr<FCkSchedulerDebugger_TreeNode>& A,
			const TSharedPtr<FCkSchedulerDebugger_TreeNode>& B)
		{
			// Groups and Processors can be siblings at the same level only within a valid tree; regardless, we always
			// produce a strict weak ordering by extracting a key per node-type so the comparator never returns
			// (false, false) for distinct nodes (which would leave them in undefined insertion order).

			switch (_SortMode)
			{
			case ECkSchedulerDebugger_SortMode::Name:
				return A->DisplayName < B->DisplayName;

			case ECkSchedulerDebugger_SortMode::Timing:
				return GetTimingKey(A) > GetTimingKey(B);

			case ECkSchedulerDebugger_SortMode::ExecutionOrder:
			default:
				return GetExecOrderKey(A) < GetExecOrderKey(B);
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
	if (InFilter.IsEmpty())
	{ return true; }
	return ck::fuzzy::Match(InFilter, InNode.DisplayName, {}).Get_IsMatch();
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
		.Padding(TAttribute<FMargin>::CreateStatic(&ck::scheduler_debugger_axes::Get_TreeRowPadding))
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

auto
	SCkSchedulerDebugger_ProcessorTree::
	DoOnContextMenuOpening()
	-> TSharedPtr<SWidget>
{
	if (NOT _TreeView.IsValid())
	{ return nullptr; }

	const auto SelectedNodes = _TreeView->GetSelectedItems();
	if (SelectedNodes.Num() == 0)
	{ return nullptr; }

	// Collect per-selected-row projections of the useful fields. For processor
	// nodes we also look up live data from the view model to include timing
	// and execution order; group / tick-group nodes only carry a name.
	auto NameLines = TArray<FString>{};
	auto ExecOrderLines = TArray<FString>{};
	auto TimingLines = TArray<FString>{};
	auto NameWithOrderLines = TArray<FString>{};
	auto AllLines = TArray<FString>{};

	const auto HasViewModel = _ViewModel.IsValid();
	const auto* Procs = HasViewModel ? &_ViewModel->Get_DataCollector().Get_Processors() : nullptr;

	for (const auto& Node : SelectedNodes)
	{
		if (NOT Node.IsValid() || Node->DisplayName.IsEmpty())
		{ continue; }

		NameLines.Add(Node->DisplayName);

		if (Node->Type == ECkSchedulerDebugger_TreeNodeType::Processor
			&& Procs != nullptr
			&& Procs->IsValidIndex(Node->ProcessorIndex))
		{
			const auto& Proc = (*Procs)[Node->ProcessorIndex];
			const auto ExecOrderStr = FString::Printf(TEXT("#%d"), Proc.ExecutionOrder);
			const auto TimingStr    = FString::Printf(TEXT("%.3f ms"), Proc.MainPassTimeMs);

			ExecOrderLines.Add(ExecOrderStr);
			TimingLines.Add(TimingStr);
			NameWithOrderLines.Add(FString::Printf(TEXT("%s %s"), *ExecOrderStr, *Node->DisplayName));
			AllLines.Add(FString::Printf(TEXT("%s %s (%s)"), *ExecOrderStr, *Node->DisplayName, *TimingStr));
		}
		else
		{
			NameWithOrderLines.Add(Node->DisplayName);
			AllLines.Add(Node->DisplayName);
		}
	}

	if (NameLines.Num() == 0)
	{ return nullptr; }

	auto MenuBuilder = FMenuBuilder(true, nullptr);

	ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
		FText::FromString(TEXT("Copy Name")),
		FText::FromString(TEXT("Copy the processor / group name(s) to the clipboard")),
		FString::Join(NameLines, TEXT("\n")));

	if (NameWithOrderLines.Num() > 0)
	{
		ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
			FText::FromString(TEXT("Copy Name with Exec Order")),
			FText::FromString(TEXT("Copy \"#N Name\" line(s) to the clipboard")),
			FString::Join(NameWithOrderLines, TEXT("\n")));
	}

	if (TimingLines.Num() > 0)
	{
		ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
			FText::FromString(TEXT("Copy Timing")),
			FText::FromString(TEXT("Copy last-frame timing for selected processor(s)")),
			FString::Join(TimingLines, TEXT("\n")));
	}

	ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
		FText::FromString(TEXT("Copy All")),
		FText::FromString(TEXT("Copy \"#N Name (t ms)\" line(s) to the clipboard")),
		FString::Join(AllLines, TEXT("\n")));

	return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------
