#include "SCkSchedulerDebuggerWindow.h"

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebugger_Axes.h"
#include "CkSchedulerDebugger/Pages/CkSchedulerDebuggerPage_TreeView.h"

#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_FrameStrip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"

#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

// Named, not anonymous: this module builds with unity on, and a merged TU makes an anonymous
// `FPlaceholderPage` collide with any other file's identically-named local type.
namespace ck_scheduler_debugger_window
{
	class FPlaceholderPage : public ICkSchedulerDebuggerPage
	{
	public:
		explicit FPlaceholderPage(const FText& InName)
			: _Name(InName)
		{
		}

		auto Get_PageName() const -> FText override { return _Name; }

		auto Build_Content(TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel) -> TSharedRef<SWidget> override
		{
			return SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(TEXT("Coming Soon")))
						.ColorAndOpacity(CkStyle::TextMute())
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_PlaceholderBanner)
				];
		}

	private:
		FText _Name;
	};
}

// --------------------------------------------------------------------------------------------------------------------

const FName SCkSchedulerDebuggerWindow::WindowId = FName(TEXT("SchedulerDebugger"));

auto
	SCkSchedulerDebuggerWindow::
	Construct(
		const FArguments& InArgs)
	-> void
{
	Register_WithGate();

	_ViewModel = MakeShared<FCkSchedulerDebugger_ViewModel>();

	_WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();

	_Pages.Add(MakeShared<FCkSchedulerDebuggerPage_TreeView>());

	_ContentContainer = SNew(SBox);

	ChildSlot
	[
		SNew(SCkDebug_WindowChrome)
			.WindowId(WindowId)
			.ToolTabId(TEXT("CkSchedulerDebugger"))
			.ShowRefreshControls(true)
			.CommandGroups(DoBuildCommandGroups())
			.Content()
			[
				SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
			.ColorAndOpacity(FLinearColor::White)
			.Padding(0.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
					[
						DoBuildStatsBar()
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(_FrameStrip, SCkDebug_FrameStrip)
							.DesiredHeight(44.0f)
							// Absolute banding, not relative-to-max: 0.15 ms puts Warn exactly on the
							// scheduler's per-frame budget line and saturates Err at 0.30 ms, which is
							// what the retired four-band Get_TimingColor drew.
							.BudgetMs(FCkSchedulerDebuggerStyle::TimingBudgetMs)
							// Column HEIGHT stays relative to the strip's own range — the old bar
							// normalized against its tallest sample, and that is the spike-spotting read.
							.HeightScale(ECkDebug_FrameStripHeightScale::RelativeToMax)
							.SelectedIndexFromEnd_Lambda([this]() -> int32
							{
								return _ViewModel.IsValid() ? _ViewModel->Get_SelectedFrameOffset() : 0;
							})
							.MarkerMeaning(FString{TEXT("pumped")})
							.CopyText_Lambda([this]() -> FString
							{
								return DoComposeSelectedFrameText();
							})
							.OnScrubbed_Lambda([this](int32 InIndexFromEnd)
							{
								if (NOT _ViewModel.IsValid())
								{ return; }

								// Every navigation path — drag, arrows, Home/End, double-click — funnels
								// here, exactly as the old widget funnelled into Set_SelectedFrameOffset.
								_ViewModel->Set_SelectedFrameOffset(InIndexFromEnd);
							})
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						DoBuildTabBar()
					]

				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						_ContentContainer.ToSharedRef()
					]
			]
			]
	];

	DoSwitchToPage(0);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	Tick(
		const FGeometry& AllottedGeometry,
		double InCurrentTime,
		float InDeltaTime)
	-> void
{
	// MUST be the WindowBase super, not SCompoundWidget — the base Tick drives the gated
	// style-revision watch that routes into OnStyleRevisionChanged.
	SCkDebugger_WindowBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Honour per-window refresh settings. This gate short-circuits the whole
	// data-collection + OnDataRefreshed broadcast chain when the window is
	// hidden, paused, or rate-capped — which is the single biggest lever
	// against editor CPU cost for the Scheduler debugger.
	if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
	{ return; }

	_WorldModel->Ensure_AutoSelect();
	auto* World = _WorldModel->Get_SelectedWorld();
	_CurrentWorld = World;
	_ViewModel->Tick(World, InDeltaTime);

	// Re-apply frame history buffer size every tick so new schedulers (after PIE restart)
	// pick up the user's setting instead of reverting to the default 300.
	if (IsValid(World))
	{
		_ViewModel->Set_FrameHistoryMaxSize(World, _FrameHistoryMaxSize);
	}

	DoPushFrameSamples(false);

	if (_ActivePageIndex >= 0 && _ActivePageIndex < _Pages.Num())
	{
		_Pages[_ActivePageIndex]->Tick(InDeltaTime);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	DoPushFrameSamples(
		bool InForce)
	-> void
{
	if (NOT _FrameStrip.IsValid() || NOT _ViewModel.IsValid())
	{ return; }

	const auto& Collector = _ViewModel->Get_DataCollector();
	const auto& Snapshots = Collector.Get_FrameSnapshots();

	const auto NewestFrame = Snapshots.IsEmpty() ? uint64{0} : Snapshots.Last().FrameNumber;

	// Re-pushing an unchanged history would clear the hovered column (and, with a filter typed,
	// re-run the per-frame processor scan) for no visible gain — which is exactly what a frozen
	// history does every gated tick.
	if (NOT InForce
		&& Snapshots.Num() == _LastPushedSampleCount
		&& NewestFrame == _LastPushedNewestFrame)
	{ return; }

	_LastPushedSampleCount = Snapshots.Num();
	_LastPushedNewestFrame = NewestFrame;

	const auto HasHighlight = NOT _HighlightFilter.IsEmpty();

	auto Samples = TArray<FCkDebug_FrameSample>{};
	Samples.Reserve(Snapshots.Num());

	auto FreshVerdicts = TMap<uint64, bool>{};
	if (HasHighlight)
	{ FreshVerdicts.Reserve(Snapshots.Num()); }

	for (auto Index = 0; Index < Snapshots.Num(); ++Index)
	{
		const auto& Snapshot = Snapshots[Index];

		auto IsHighlighted = false;
		if (HasHighlight)
		{
			if (const auto* Cached = _HighlightVerdictByFrame.Find(Snapshot.FrameNumber))
			{ IsHighlighted = *Cached; }
			else
			{ IsHighlighted = Collector.FrameContainsProcessor(Index, _HighlightFilter); }

			FreshVerdicts.Add(Snapshot.FrameNumber, IsHighlighted);
		}

		auto Sample = FCkDebug_FrameSample{};
		Sample.ValueMs = Snapshot.TotalFrameTimeMs;
		Sample.HasMarker = Snapshot.PumpIterationCount > 0;
		Sample.IsHighlighted = IsHighlighted;

		Samples.Add(MoveTemp(Sample));
	}

	// Dropping the old map here is what evicts verdicts for frames the ring buffer discarded.
	_HighlightVerdictByFrame = MoveTemp(FreshVerdicts);

	_FrameStrip->Set_Samples(MoveTemp(Samples));
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	DoComposeSelectedFrameText() const
	-> FString
{
	if (NOT _ViewModel.IsValid())
	{ return FString{}; }

	const auto& Snapshots = _ViewModel->Get_DataCollector().Get_FrameSnapshots();
	const auto SnapshotIndex = Snapshots.Num() - 1 - _ViewModel->Get_SelectedFrameOffset();

	if (NOT Snapshots.IsValidIndex(SnapshotIndex))
	{ return FString{}; }

	const auto& Snapshot = Snapshots[SnapshotIndex];

	return ck::Format_UE(TEXT("Frame #{}  {} ms  pumps {}  dirty {}"),
		Snapshot.FrameNumber,
		FString::Printf(TEXT("%.3f"), Snapshot.TotalFrameTimeMs),
		Snapshot.PumpIterationCount,
		Snapshot.DirtyProcessorCount);
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkSchedulerDebuggerWindow::DoBuildCommandGroups() -> TArray<FCkDebug_CommandGroup>
{
    const auto Freeze = SNew(SCkDebug_IconToggle)
        .IconId(TEXT("Snowflake")).Label(FText::FromString(TEXT("Freeze capture")))
        .ToolTip(FText::FromString(TEXT("Freeze scheduler capture. Turning this off while inspecting history returns to live capture.")))
        .IsOn_Lambda([this]() { return _ViewModel.IsValid() && (_ViewModel->Get_IsFrozen() || _ViewModel->Get_SelectedFrameOffset() > 0); })
        .OnStateChanged_Lambda([this](bool InIsOn) { if (NOT _ViewModel.IsValid()) { return; } if (InIsOn) { _ViewModel->Set_IsFrozen(true); } else { if (_ViewModel->Get_SelectedFrameOffset() > 0) { _ViewModel->Set_SelectedFrameOffset(0); } _ViewModel->Set_IsFrozen(false); } });
    const auto FrameHistory = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SCkDebug_WorldSelector, _WorldModel).ShowHeaderLabel(false) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SButton).ToolTipText(FText::FromString(TEXT("Previous frame (older) — Left Arrow"))).OnClicked_Lambda([this]() { _ViewModel->CycleSelectedFrame(+1); return FReply::Handled(); })[SNew(STextBlock).Text(FText::FromString(TEXT("<")))] ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SButton).ToolTipText(FText::FromString(TEXT("Next frame (newer) — Right Arrow"))).OnClicked_Lambda([this]() { _ViewModel->CycleSelectedFrame(-1); return FReply::Handled(); })[SNew(STextBlock).Text(FText::FromString(TEXT(">")))] ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(STextBlock).Text(FText::FromString(TEXT("History"))).ColorAndOpacity(CkStyle::TextDim()) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(70.0f)[SNew(SSpinBox<int32>).MinValue(10).MaxValue(10000).Value(this, &SCkSchedulerDebuggerWindow::DoGetFrameHistoryMaxSize).OnValueCommitted_Lambda([this](int32 InValue, ETextCommit::Type) { _FrameHistoryMaxSize = InValue; if (_CurrentWorld.IsValid()) { _ViewModel->Set_FrameHistoryMaxSize(_CurrentWorld.Get(), InValue); } })] ];
    const auto Highlight = SNew(SSearchBox).HintText(FText::FromString(TEXT("Highlight frames with processor..."))).ToolTipText(FText::FromString(TEXT("Outlines history bars whose frames ran a processor matching this text"))).OnTextChanged_Lambda([this](const FText& InText) { auto NewFilter = InText.ToString(); if (_HighlightFilter == NewFilter) { return; } _HighlightFilter = MoveTemp(NewFilter); _HighlightVerdictByFrame.Reset(); DoPushFrameSamples(true); });
    const auto SelectedFrame = SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(DoComposeSelectedFrameText()); }).ColorAndOpacity(CkStyle::TextDim());
    return {FCkDebug_CommandGroup::Primary(TEXT("SchedulerFreeze"), FText::FromString(TEXT("Scheduler freeze capture")), Freeze), FCkDebug_CommandGroup::Context(TEXT("SchedulerFrameHistory"), FText::FromString(TEXT("Scheduler frame navigation and history")), FrameHistory), FCkDebug_CommandGroup::Context(TEXT("SchedulerHighlight"), FText::FromString(TEXT("Scheduler frame highlight filter")), Highlight), FCkDebug_CommandGroup::Context(TEXT("SchedulerSelectedFrame"), FText::FromString(TEXT("Selected scheduler frame status")), SelectedFrame)};
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	OnStyleRevisionChanged()
	-> void
{
	// The chrome, the top bar and the stats strip are attribute-bound and have already moved. Only
	// the pages own imperative sub-trees (tree rows, inspector rows, graph nodes), so the notification
	// goes to EVERY page, not just the active one — an inactive page keeps its widgets alive and would
	// otherwise come back stale when the user switches to it.
	for (const auto& Page : _Pages)
	{
		if (Page.IsValid())
		{ Page->OnStyleRevisionChanged(); }
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	DoBuildStatsBar()
	-> TSharedRef<SWidget>
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding_Lambda([]()
		{
			return ck::debug_axes::Apply_RowDensity(
				FMargin{FCkSchedulerDebuggerStyle::Padding_Small});
		})
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					DoMakeStatItem(
						FText::FromString(TEXT("Frame Time")),
						TAttribute<FText>::CreateLambda([this]()
						{
							return FText::FromString(FString::Printf(TEXT("%.2f ms"),
								_ViewModel->Get_DataCollector().Get_TotalFrameTimeMs()));
						}),
						TAttribute<FSlateColor>::CreateLambda([this]()
						{
							return FSlateColor(FCkSchedulerDebuggerStyle::Get_TimingColor(
								_ViewModel->Get_DataCollector().Get_TotalFrameTimeMs()));
						})
					)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					DoMakeStatItem(
						FText::FromString(TEXT("Pump Count")),
						TAttribute<FText>::CreateLambda([this]()
						{
							return FText::AsNumber(_ViewModel->Get_DataCollector().Get_PumpCount());
						}),
						TAttribute<FSlateColor>::CreateLambda([this]()
						{
							constexpr auto PumpWarningThreshold = 3;
							const auto Count = _ViewModel->Get_DataCollector().Get_PumpCount();
							return FSlateColor(Count >= PumpWarningThreshold
								? CkStyle::Err()
								: CkStyle::Text());
						})
					)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					DoMakeStatItem(
						FText::FromString(TEXT("Processors")),
						TAttribute<FText>::CreateLambda([this]()
						{
							return FText::AsNumber(_ViewModel->Get_DataCollector().Get_ProcessorCount());
						}),
						TAttribute<FSlateColor>(FSlateColor(CkStyle::Text()))
					)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					DoMakeStatItem(
						FText::FromString(TEXT("Ghosts")),
						TAttribute<FText>::CreateLambda([this]()
						{
							return FText::AsNumber(_ViewModel->Get_DataCollector().Get_GhostCount());
						}),
						TAttribute<FSlateColor>(FSlateColor(CkStyle::None()))
					)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					DoMakeStatItem(
						FText::FromString(TEXT("Dirty")),
						TAttribute<FText>::CreateLambda([this]()
						{
							return FText::AsNumber(_ViewModel->Get_DataCollector().Get_DirtyCount());
						}),
						TAttribute<FSlateColor>(FSlateColor(CkStyle::Warn()))
					)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
				[
					DoMakeStatItem(
						FText::FromString(TEXT("Parallel")),
						TAttribute<FText>::CreateLambda([this]()
						{
							return FText::AsNumber(_ViewModel->Get_DataCollector().Get_ParallelCount());
						}),
						TAttribute<FSlateColor>(FSlateColor(CkStyle::Info()))
					)
				]
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	DoBuildTabBar()
	-> TSharedRef<SWidget>
{
	auto TabBar = SNew(SHorizontalBox);

	for (auto PageIdx = 0; PageIdx < _Pages.Num(); ++PageIdx)
	{
		TabBar->AddSlot()
			.AutoWidth()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
			[
				SNew(SBox)
					.Padding(0.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBox)
									.HeightOverride(2.0f)
									[
										SNew(SBorder)
											.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
											.ColorAndOpacity_Lambda([this, PageIdx]()
											{
												return _ActivePageIndex == PageIdx
													? CkStyle::Selection()
													: FLinearColor::Transparent;
											})
									]
							]

						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SButton)
									.ButtonStyle(FCoreStyle::Get(), "NoBorder")
									.ContentPadding(FMargin(
										FCkSchedulerDebuggerStyle::Padding_Medium,
										FCkSchedulerDebuggerStyle::Padding_Small))
									.OnClicked_Lambda([this, PageIdx]() -> FReply
									{
										DoSwitchToPage(PageIdx);
										return FReply::Handled();
									})
									[
										SNew(STextBlock)
											.Text(_Pages[PageIdx]->Get_PageName())
											.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_H3)
											.ColorAndOpacity_Lambda([this, PageIdx]()
											{
												return _ActivePageIndex == PageIdx
													? CkStyle::TextStrong()
													: CkStyle::TextDim();
											})
									]
							]
					]
			];
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f))
		[
			TabBar
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	DoSwitchToPage(
		int32 InPageIndex)
	-> void
{
	if (NOT _Pages.IsValidIndex(InPageIndex))
	{ return; }

	if (_ActivePageIndex >= 0 && _ActivePageIndex < _Pages.Num())
	{
		_Pages[_ActivePageIndex]->Set_IsActive(false);
	}

	_ActivePageIndex = InPageIndex;
	_Pages[_ActivePageIndex]->Set_IsActive(true);

	_ContentContainer->SetContent(
		_Pages[_ActivePageIndex]->Build_Content(_ViewModel));
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	DoMakeStatItem(
		const FText& InLabel,
		TAttribute<FText> InValue,
		TAttribute<FSlateColor> InColor)
	-> TSharedRef<SWidget>
{
	// Delegates to the shared common-widget. Inline_LabelFirst layout matches
	// the original here (label · value) and gives us copy-paste on both halves
	// + consistent typography with every other CkDebugger stat strip.
	return SNew(SCkDebug_StatPair)
		.Layout(ECkDebug_StatPairLayout::Inline_LabelFirst)
		.Label(InLabel)
		.Value(InValue)
		.ValueColor(InColor);
}

// --------------------------------------------------------------------------------------------------------------------
