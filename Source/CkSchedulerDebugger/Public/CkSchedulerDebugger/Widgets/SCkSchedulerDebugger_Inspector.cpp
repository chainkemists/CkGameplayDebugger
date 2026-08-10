#include "SCkSchedulerDebugger_Inspector.h"

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_scheduler_debugger_inspector
{
	// Section headers go through the SectionHeaderStyle axis instead of each site hand-rolling a
	// bold uppercase STextBlock. Classic keeps the uppercase bold form these sections already used.
	auto Make_Header(const FString& InLabel, ECk_Tone InTone) -> TSharedRef<SWidget>
	{
		return ck::debug_axes::Make_SectionHeader(
			UCkDebuggerStyleSettings::Get_Selection(), FText::FromString(InLabel), InTone);
	}

	// The between-section rule, wired to the SeparatorWeight axis. SSeparator::Thickness is a plain
	// SLATE_ARGUMENT with no setter, so the axis is carried by an SBox height override; a zero-weight
	// option collapses the box, and SBoxPanel then skips the child AND its slot padding.
	// Hairline == 1.0f == what SSeparator drew before, so Classic is unchanged.
	auto Make_SectionSeparator() -> TSharedRef<SWidget>
	{
		const auto Get_Thickness = []()
		{
			return ck::debug_axes::Get_SeparatorThickness(UCkDebuggerStyleSettings::Get_Selection());
		};

		return SNew(SBox)
			.HeightOverride_Lambda([Get_Thickness]() -> FOptionalSize
			{
				return FOptionalSize{Get_Thickness()};
			})
			.Visibility_Lambda([Get_Thickness]()
			{
				return Get_Thickness() > 0.0f ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				SNew(SSeparator)
			];
	}

	// RowDensity as a DELTA on the inspector's own base padding — same contract as the ECS tree.
	auto Apply_RowDensity(const FMargin& InBase) -> FMargin
	{
		const auto Baseline = ck::debug_axes::Get_RowPadding(FCkDebuggerStyleSelection{});
		const auto Current  = ck::debug_axes::Get_RowPadding(UCkDebuggerStyleSettings::Get_Selection());

		const auto DeltaX = Current.Left - Baseline.Left;
		const auto DeltaY = Current.Top  - Baseline.Top;

		return FMargin
		{
			FMath::Max(0.0f, InBase.Left   + DeltaX),
			FMath::Max(0.0f, InBase.Top    + DeltaY),
			FMath::Max(0.0f, InBase.Right  + DeltaX),
			FMath::Max(0.0f, InBase.Bottom + DeltaY)
		};
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	Construct(
		const FArguments& InArgs)
	-> void
{
	_ViewModel = InArgs._ViewModel;

	_ContentBox = SNew(SBox);

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
			[
				_ContentBox.ToSharedRef()
			]
	];

	DoRebuildContent();

	if (_ViewModel.IsValid())
	{
		_SelectionChangedHandle = _ViewModel->OnSelectionChanged.AddRaw(
			this, &SCkSchedulerDebugger_Inspector::DoOnSelectionChanged);
		_DataRefreshedHandle = _ViewModel->OnDataRefreshed.AddRaw(
			this, &SCkSchedulerDebugger_Inspector::DoOnDataRefreshed);
	}
}

// --------------------------------------------------------------------------------------------------------------------

SCkSchedulerDebugger_Inspector::~SCkSchedulerDebugger_Inspector()
{
	if (_ViewModel.IsValid())
	{
		_ViewModel->OnSelectionChanged.Remove(_SelectionChangedHandle);
		_ViewModel->OnDataRefreshed.Remove(_DataRefreshedHandle);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoOnSelectionChanged(
		int32 InProcessorIndex)
	-> void
{
	DoRebuildContent();
}

auto
	SCkSchedulerDebugger_Inspector::
	DoOnDataRefreshed()
	-> void
{
	// Don't rebuild every frame — only selection changes trigger a full rebuild.
	// Per-frame rebuilds cause SScrollBox layout measurement failures (overlapping text).
	// EXCEPTION: when the frame offset changes (user scrubbing the history bar), we need
	// to rebuild to show the historical frame's timing data.
	if (NOT _ViewModel.IsValid())
	{ return; }

	const auto CurrentFrameOffset = _ViewModel->Get_SelectedFrameOffset();
	if (CurrentFrameOffset != _LastSeenFrameOffset)
	{
		_LastSeenFrameOffset = CurrentFrameOffset;
		DoRebuildContent();
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoRebuildContent()
	-> void
{
	if (NOT _ContentBox.IsValid() || NOT _ViewModel.IsValid())
	{ return; }

	const auto SelectedIdx = _ViewModel->Get_SelectedProcessorIndex();
	const auto& Procs = _ViewModel->Get_DataCollector().Get_Processors();

	if (SelectedIdx == INDEX_NONE || NOT Procs.IsValidIndex(SelectedIdx))
	{
		_ContentBox->SetContent(DoBuildEmptyContent());
		return;
	}

	_ContentBox->SetContent(DoBuildProcessorContent(Procs[SelectedIdx]));
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoBuildEmptyContent()
	-> TSharedRef<SWidget>
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FCkSchedulerDebuggerStyle::Padding_Large)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("No processor selected")))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
				.ColorAndOpacity(CkStyle::TextMute())
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoBuildProcessorContent(
		const FCkSchedulerDebugger_ProcessorInfo& InProc)
	-> TSharedRef<SWidget>
{
	auto Content = SNew(SVerticalBox);

	Content->AddSlot()
		.AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SCkDebug_SelectableLabel)
						.Text(FText::FromName(InProc.ProcessorName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(CkStyle::Text())
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
						[
							SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.BorderBackgroundColor(InProc.IsGhost
									? CkStyle::None()
									: CkStyle::Ok())
								.Padding(FMargin(6.0f, 2.0f))
								[
									SNew(STextBlock)
										.Text(FText::FromString(InProc.IsGhost ? TEXT("Ghost") : TEXT("Active")))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
										.ColorAndOpacity(FLinearColor::White)
								]
						]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
						[
							SNew(SBorder)
								.Visibility(InProc.HasDirtyMarker ? EVisibility::Visible : EVisibility::Collapsed)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.BorderBackgroundColor(CkStyle::Warn())
								.Padding(FMargin(6.0f, 2.0f))
								[
									SNew(STextBlock)
										.Text(FText::FromString(TEXT("Dirty")))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
										.ColorAndOpacity(FLinearColor::White)
								]
						]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBorder)
								.Visibility(InProc.IsParallel ? EVisibility::Visible : EVisibility::Collapsed)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.BorderBackgroundColor(CkStyle::Info())
								.Padding(FMargin(6.0f, 2.0f))
								[
									SNew(STextBlock)
										.Text(FText::FromString(TEXT("Parallel")))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
										.ColorAndOpacity(FLinearColor::White)
								]
						]
				]
		];

	Content->AddSlot()
		.AutoHeight()
		[
			ck_scheduler_debugger_inspector::Make_SectionSeparator()
		];

	Content->AddSlot()
		.AutoHeight()
		[
			DoBuildInfoSection(InProc)
		];

	Content->AddSlot()
		.AutoHeight()
		[
			DoBuildTimingSection(InProc)
		];

	if (InProc.HasDirtyMarker)
	{
		Content->AddSlot()
			.AutoHeight()
			[
				DoBuildDirtySection(InProc)
			];
	}

	if (InProc.WriteConflicts.Num() > 0)
	{
		Content->AddSlot()
			.AutoHeight()
			[
				DoBuildWriteConflictSection(InProc)
			];
	}

	Content->AddSlot()
		.AutoHeight()
		[
			DoBuildDependenciesSection(InProc)
		];

	return Content;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoBuildInfoSection(
		const FCkSchedulerDebugger_ProcessorInfo& InProc)
	-> TSharedRef<SWidget>
{
	auto Body = SNew(SVerticalBox);

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Group"), InProc.GroupName.IsNone()
			? TEXT("(ungrouped)")
			: InProc.GroupName.ToString())];

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Tick Group"), DoGetTickGroupName(InProc.TickGroup))];

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Exec Order"),
			InProc.ExecutionOrder != INDEX_NONE
				? FString::Printf(TEXT("#%d"), InProc.ExecutionOrder)
				: TEXT("N/A"))];

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Timing (Current)"),
			FString::Printf(TEXT("%.3f ms"), InProc.MainPassTimeMs))];

	auto Section = SNew(SVerticalBox);

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			ck_scheduler_debugger_inspector::Make_Header(TEXT("INFO"), ECk_Tone::Neutral)
		];

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			Body
		];

	Section->AddSlot().AutoHeight()
		[ck_scheduler_debugger_inspector::Make_SectionSeparator()];

	return Section;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoBuildTimingSection(
		const FCkSchedulerDebugger_ProcessorInfo& InProc)
	-> TSharedRef<SWidget>
{
	auto Body = SNew(SVerticalBox);

	// The retired module-local sparkline took a TArray<double> by value; the common widget takes a
	// shared ring of floats and holds it alive, which is why the copy is built here and handed over.
	auto Samples = MakeShared<TArray<float>>();
	Samples->Reserve(InProc.TimingHistory.Num());

	auto PeakTimeMs = 0.0;
	for (const auto TimeMs : InProc.TimingHistory)
	{
		Samples->Add(static_cast<float>(TimeMs));
		PeakTimeMs = FMath::Max(PeakTimeMs, TimeMs);
	}

	const auto LatestTimeMs = InProc.TimingHistory.IsEmpty() ? 0.0 : InProc.TimingHistory.Last();

	Body->AddSlot().AutoHeight()
		[
			SNew(SBox)
				.HeightOverride(48.0f)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
				[
					SNew(SCkDebug_Sparkline)
						.Samples(Samples)
						.Color(FCkSchedulerDebuggerStyle::Get_TimingColor(LatestTimeMs))
						.FillOpacity(0.15f)
						.DesiredSize(FVector2D{200.0f, 40.0f})
				]
		];

	// The old sparkline painted its own peak / current labels into the corners. The common widget
	// draws no text, so those two numbers become inspector rows rather than being lost.
	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Peak (history)"), FString::Printf(TEXT("%.3f ms"), PeakTimeMs))];

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Total Ticks"), FString::FromInt(InProc.TotalTicks))];

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Tick Rate"),
			FString::Printf(TEXT("%.1f%%"), InProc.TickRate * 100.0))];

	auto Section = SNew(SVerticalBox);

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			ck_scheduler_debugger_inspector::Make_Header(TEXT("TIMING"), ECk_Tone::Neutral)
		];

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			Body
		];

	Section->AddSlot().AutoHeight()
		[ck_scheduler_debugger_inspector::Make_SectionSeparator()];

	return Section;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoBuildDirtySection(
		const FCkSchedulerDebugger_ProcessorInfo& InProc)
	-> TSharedRef<SWidget>
{
	auto Body = SNew(SVerticalBox);

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Marker"),
			InProc.DirtyMarkerName.IsNone()
				? TEXT("(unknown)")
				: InProc.DirtyMarkerName.ToString())];

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Dirty This Frame"),
			InProc.WasDirtyThisFrame ? TEXT("Yes") : TEXT("No"))];

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Pump Count"),
			FString::FromInt(InProc.PumpCountThisFrame))];

	for (auto PassIdx = 0; PassIdx < InProc.PumpPassTimesMs.Num(); ++PassIdx)
	{
		Body->AddSlot().AutoHeight()
			[DoMakeInfoRow(
				FString::Printf(TEXT("Pump Pass %d"), PassIdx),
				FString::Printf(TEXT("%.3f ms"), InProc.PumpPassTimesMs[PassIdx]))];
	}

	auto Section = SNew(SVerticalBox);

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			ck_scheduler_debugger_inspector::Make_Header(TEXT("DIRTY MARKER"), ECk_Tone::Neutral)
		];

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			Body
		];

	Section->AddSlot().AutoHeight()
		[ck_scheduler_debugger_inspector::Make_SectionSeparator()];

	return Section;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoBuildWriteConflictSection(
		const FCkSchedulerDebugger_ProcessorInfo& InProc)
	-> TSharedRef<SWidget>
{
	// Red accent to signal these are errors/warnings, not ordinary metadata.
	const auto ErrorColorRGBA = CkStyle::Err();

	auto Body = SNew(SVerticalBox);

	auto AnyUnresolved = false;
	for (const auto& Conflict : InProc.WriteConflicts)
	{
		if (NOT Conflict.WasAutoResolved)
		{
			AnyUnresolved = true;
			break;
		}
	}

	Body->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			SNew(STextBlock)
				.Text(FText::FromString(AnyUnresolved
					? TEXT("Unresolved write-write conflicts — add RunAfter/RunBefore to fix.")
					: TEXT("Auto-resolved in declaration order — consider adding explicit ordering.")))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
				.ColorAndOpacity(ErrorColorRGBA)
				.AutoWrapText(true)
		];

	for (const auto& Conflict : InProc.WriteConflicts)
	{
		const auto PeerName = Conflict.PeerProcessorName.IsNone()
			? FString(TEXT("(unknown)"))
			: Conflict.PeerProcessorName.ToString();
		const auto FragName = Conflict.FragmentName.IsNone()
			? FString(TEXT("(unknown fragment)"))
			: Conflict.FragmentName.ToString();
		const auto Resolution = Conflict.WasAutoResolved
			? FString(TEXT("auto-edge"))
			: FString(TEXT("UNRESOLVED"));

		Body->AddSlot().AutoHeight()
			[DoMakeInfoRow(
				FString::Printf(TEXT("%s (%s)"), *PeerName, *Resolution),
				FragName)];
	}

	auto Section = SNew(SVerticalBox);

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			ck_scheduler_debugger_inspector::Make_Header(TEXT("WRITE CONFLICTS"), ECk_Tone::Err)
		];

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			Body
		];

	Section->AddSlot().AutoHeight()
		[ck_scheduler_debugger_inspector::Make_SectionSeparator()];

	return Section;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoBuildDependenciesSection(
		const FCkSchedulerDebugger_ProcessorInfo& InProc)
	-> TSharedRef<SWidget>
{
	auto Body = SNew(SVerticalBox);

	Body->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("Run After:")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(CkStyle::TextDim())
		];

	if (InProc.InEdges.Num() == 0)
	{
		Body->AddSlot().AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("(none)")))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
					.ColorAndOpacity(CkStyle::TextMute())
			];
	}
	else
	{
		for (const auto EdgeIdx : InProc.InEdges)
		{
			Body->AddSlot().AutoHeight()
				[DoMakeDependencyButton(EdgeIdx)];
		}
	}

	Body->AddSlot()
		.AutoHeight()
		.Padding(0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("Run Before:")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(CkStyle::TextDim())
		];

	if (InProc.OutEdges.Num() == 0)
	{
		Body->AddSlot().AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("(none)")))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
					.ColorAndOpacity(CkStyle::TextMute())
			];
	}
	else
	{
		for (const auto EdgeIdx : InProc.OutEdges)
		{
			Body->AddSlot().AutoHeight()
				[DoMakeDependencyButton(EdgeIdx)];
		}
	}

	auto Section = SNew(SVerticalBox);

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			ck_scheduler_debugger_inspector::Make_Header(TEXT("DEPENDENCIES"), ECk_Tone::Neutral)
		];

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			Body
		];

	return Section;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoMakeInfoRow(
		const FString& InLabel,
		const FString& InValue)
	-> TSharedRef<SWidget>
{
	// Rows are the RowDensity axis' primary surface in this panel. Rebuilt on selection / frame
	// change, so the density is sampled here rather than bound per-paint.
	const auto LabelPadding = ck_scheduler_debugger_inspector::Apply_RowDensity(FMargin{0.0f, 2.0f});
	const auto ValuePadding = ck_scheduler_debugger_inspector::Apply_RowDensity(
		FMargin{FCkSchedulerDebuggerStyle::Padding_Small, 2.0f});

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(LabelPadding)
			[
				SNew(SBox)
					.MinDesiredWidth(80.0f)
					.MaxDesiredWidth(100.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(InLabel))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							.ColorAndOpacity(CkStyle::TextDim())
							.AutoWrapText(true)
					]
			]

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Top)
			.Padding(ValuePadding)
			[
				SNew(SCkDebug_SelectableLabel)
					.Text(FText::FromString(InValue))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(CkStyle::Text())
			];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoMakeDependencyButton(
		int32 InNodeIndex)
	-> TSharedRef<SWidget>
{
	auto DisplayName = FString{TEXT("Unknown")};

	if (_ViewModel.IsValid())
	{
		const auto& Procs = _ViewModel->Get_DataCollector().Get_Processors();
		for (auto Idx = 0; Idx < Procs.Num(); ++Idx)
		{
			if (Procs[Idx].NodeIndex == InNodeIndex)
			{
				DisplayName = Procs[Idx].DisplayName;
				break;
			}
		}
	}

	auto TargetProcessorIdx = static_cast<int32>(INDEX_NONE);
	if (_ViewModel.IsValid())
	{
		const auto& Procs = _ViewModel->Get_DataCollector().Get_Processors();
		for (auto Idx = 0; Idx < Procs.Num(); ++Idx)
		{
			if (Procs[Idx].NodeIndex == InNodeIndex)
			{
				TargetProcessorIdx = Idx;
				break;
			}
		}
	}

	return SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(FCkSchedulerDebuggerStyle::Padding_Medium, 2.0f))
		.OnClicked_Lambda([this, TargetProcessorIdx]() -> FReply
		{
			if (_ViewModel.IsValid() && TargetProcessorIdx != INDEX_NONE)
			{
				_ViewModel->Set_SelectedProcessorIndex(TargetProcessorIdx);
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
				.Text(FText::FromString(DisplayName))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(CkStyle::Selection())
				.AutoWrapText(true)
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoGetTickGroupName(
		ETickingGroup InTickGroup)
	-> FString
{
	switch (InTickGroup)
	{
	case TG_PrePhysics:     return TEXT("Pre Physics");
	case TG_DuringPhysics:  return TEXT("During Physics");
	case TG_PostPhysics:    return TEXT("Post Physics");
	case TG_PostUpdateWork: return TEXT("Post Update Work");
	default:                return TEXT("Unknown");
	}
}

// --------------------------------------------------------------------------------------------------------------------
