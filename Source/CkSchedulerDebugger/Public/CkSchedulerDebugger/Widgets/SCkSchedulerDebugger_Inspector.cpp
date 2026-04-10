#include "SCkSchedulerDebugger_Inspector.h"

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

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
				.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
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
					SNew(STextBlock)
						.Text(FText::FromName(InProc.ProcessorName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Primary)
						.AutoWrapText(true)
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
								.ColorAndOpacity(InProc.IsGhost
									? FCkSchedulerDebuggerStyle::Color_Ghost
									: FCkSchedulerDebuggerStyle::Color_Active)
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
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_DirtyMarker)
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
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Parallel)
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
			SNew(SSeparator)
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
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("INFO")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
		];

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			Body
		];

	Section->AddSlot().AutoHeight()
		[SNew(SSeparator)];

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

	Body->AddSlot().AutoHeight()
		[
			SNew(SBox)
				.HeightOverride(40.0f)
				.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
				[
					SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Background_Dark)
						.Padding(FCkSchedulerDebuggerStyle::Padding_Small)
						[
							SNew(STextBlock)
								.Text(FText::FromString(TEXT("Sparkline placeholder")))
								.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
								.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
						]
				]
		];

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Total Ticks"), FString::FromInt(InProc.TotalTicks))];

	Body->AddSlot().AutoHeight()
		[DoMakeInfoRow(TEXT("Tick Rate"),
			FString::Printf(TEXT("%.1f%%"), InProc.TickRate * 100.0))];

	auto Section = SNew(SVerticalBox);

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("TIMING")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
		];

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			Body
		];

	Section->AddSlot().AutoHeight()
		[SNew(SSeparator)];

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
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("DIRTY MARKER")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
		];

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			Body
		];

	Section->AddSlot().AutoHeight()
		[SNew(SSeparator)];

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
	constexpr auto ErrorColorRGBA = FLinearColor{0.92f, 0.30f, 0.30f, 1.0f};

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
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("WRITE CONFLICTS")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(ErrorColorRGBA)
		];

	Section->AddSlot().AutoHeight()
		.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, FCkSchedulerDebuggerStyle::Padding_Small)
		[
			Body
		];

	Section->AddSlot().AutoHeight()
		[SNew(SSeparator)];

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
				.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
		];

	if (InProc.InEdges.Num() == 0)
	{
		Body->AddSlot().AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("(none)")))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
					.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
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
				.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
		];

	if (InProc.OutEdges.Num() == 0)
	{
		Body->AddSlot().AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("(none)")))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
					.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
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
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("DEPENDENCIES")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
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
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(0.0f, 2.0f)
			[
				SNew(SBox)
					.MinDesiredWidth(80.0f)
					.MaxDesiredWidth(100.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(InLabel))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
							.AutoWrapText(true)
					]
			]

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Top)
			.Padding(FCkSchedulerDebuggerStyle::Padding_Small, 2.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(InValue))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Primary)
					.AutoWrapText(true)
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
				.ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Selection)
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
