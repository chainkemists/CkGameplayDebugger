#include "SCkSchedulerDebuggerWindow.h"

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"
#include "CkSchedulerDebugger/Pages/CkSchedulerDebuggerPage_TreeView.h"
#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_FrameHistoryBar.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"

#include "Engine/World.h"
#include "Editor.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
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
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
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
		SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
			.ColorAndOpacity(FLinearColor::White)
			.Padding(0.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						DoBuildTopBar()
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
					[
						DoBuildStatsBar()
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 2.0f, FCkSchedulerDebuggerStyle::Padding_Small, 2.0f)
							[
								SNew(SButton)
									.ToolTipText(FText::FromString(TEXT("Previous frame (older) — Left Arrow")))
									.OnClicked_Lambda([this]() -> FReply
									{
										_ViewModel->CycleSelectedFrame(+1);
										return FReply::Handled();
									})
									[
										SNew(STextBlock)
											.Text(FText::FromString(TEXT("<")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
									]
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 2.0f, FCkSchedulerDebuggerStyle::Padding_Small, 2.0f)
							[
								SNew(SButton)
									.ToolTipText(FText::FromString(TEXT("Next frame (newer) — Right Arrow")))
									.OnClicked_Lambda([this]() -> FReply
									{
										_ViewModel->CycleSelectedFrame(-1);
										return FReply::Handled();
									})
									[
										SNew(STextBlock)
											.Text(FText::FromString(TEXT(">")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
									]
							]

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 2.0f)
							[
								SNew(SSearchBox)
									.HintText(FText::FromString(TEXT("Highlight frames with processor...")))
									.ToolTipText(FText::FromString(TEXT("Outlines history bars whose frames ran a processor matching this text")))
									.OnTextChanged_Lambda([this](const FText& InText)
									{
										if (_FrameHistoryBar.IsValid())
										{
											_FrameHistoryBar->Set_HighlightFilter(InText.ToString());
										}
									})
							]
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(_FrameHistoryBar, SCkSchedulerDebugger_FrameHistoryBar)
							.ViewModel(_ViewModel)
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
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

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

	if (_ActivePageIndex >= 0 && _ActivePageIndex < _Pages.Num())
	{
		_Pages[_ActivePageIndex]->Tick(InDeltaTime);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	DoBuildTopBar()
	-> TSharedRef<SWidget>
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(FMargin(FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Large, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::FromString(TEXT("CkScheduler Debugger")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Primary)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
				[
					SNew(SCkDebug_WorldSelector, _WorldModel)
						.ShowHeaderLabel(false)
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
						[
							SNew(STextBlock)
								.Text(FText::FromString(TEXT("History")))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
						]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
								.WidthOverride(70.0f)
								[
									SNew(SSpinBox<int32>)
										.MinValue(10)
										.MaxValue(10000)
										.Value(this, &SCkSchedulerDebuggerWindow::DoGetFrameHistoryMaxSize)
										.OnValueCommitted_Lambda([this](int32 InValue, ETextCommit::Type)
										{
											_FrameHistoryMaxSize = InValue;
											if (_CurrentWorld.IsValid())
											{
												_ViewModel->Set_FrameHistoryMaxSize(_CurrentWorld.Get(), InValue);
											}
										})
										.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								]
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
						.OnClicked_Lambda([this]() -> FReply
						{
							if (_ViewModel->Get_SelectedFrameOffset() > 0)
							{
								// If on a historical frame, resume = return to live
								_ViewModel->Set_SelectedFrameOffset(0);
							}
							else
							{
								_ViewModel->Set_IsFrozen(NOT _ViewModel->Get_IsFrozen());
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
								.Text_Lambda([this]() -> FText
								{
									return _ViewModel->Get_IsFrozen()
										? FText::FromString(TEXT("Resume"))
										: FText::FromString(TEXT("Freeze"));
								})
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Large, 0.0f, 0.0f, 0.0f)
				[
					SNew(SCkDebugger_RefreshControls)
						.WindowId(SCkSchedulerDebuggerWindow::WindowId)
				]
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebuggerWindow::
	DoBuildStatsBar()
	-> TSharedRef<SWidget>
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(FCkSchedulerDebuggerStyle::Padding_Small))
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
								? FCkSchedulerDebuggerStyle::Color_Warning
								: FCkSchedulerDebuggerStyle::Color_Text_Primary);
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
						TAttribute<FSlateColor>(FSlateColor(FCkSchedulerDebuggerStyle::Color_Text_Primary))
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
						TAttribute<FSlateColor>(FSlateColor(FCkSchedulerDebuggerStyle::Color_Ghost))
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
						TAttribute<FSlateColor>(FSlateColor(FCkSchedulerDebuggerStyle::Color_DirtyMarker))
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
						TAttribute<FSlateColor>(FSlateColor(FCkSchedulerDebuggerStyle::Color_Parallel))
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
													? FCkSchedulerDebuggerStyle::Color_Selection
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
											.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
											.ColorAndOpacity_Lambda([this, PageIdx]()
											{
												return _ActivePageIndex == PageIdx
													? FCkSchedulerDebuggerStyle::Color_Text_Highlight
													: FCkSchedulerDebuggerStyle::Color_Text_Secondary;
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
