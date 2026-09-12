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
#include "CkSchedulerDebugger/Styles/CkSchedulerDebugger_Axes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/CkUiFloatSeries.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_scheduler_debugger_inspector
{
	auto AuthoredTokens() -> FCkUiView::FTokens
	{
		return {
			{TEXT("--space-xs"), FString::SanitizeFloat(CkStyle::SpaceXS)},
			{TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS)},
			{TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM)},
			{TEXT("--space-l"), FString::SanitizeFloat(CkStyle::SpaceL)},
			{TEXT("--space-xl"), FString::SanitizeFloat(CkStyle::SpaceXL)},
		};
	}

	// Section headers go through the SectionHeaderStyle axis instead of each site hand-rolling a
	// bold uppercase STextBlock. Classic keeps the uppercase bold form these sections already used.
	auto Make_Header(const FString& InLabel, ECk_Tone InTone) -> TSharedRef<SWidget>
	{
		return ck::debug_axes::Make_SectionHeader(
			UCkDebuggerStyleSettings::Get_Selection(), FText::FromString(InLabel), InTone);
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
		_ContentBox.ToSharedRef()
	];

	DoBuildAuthoredView();
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
	_AuthoredView.Reset();
	_AuthoredDependencies.Reset();
	_AuthoredDirtyDetails.Reset();
	_AuthoredConflicts.Reset();
	_AuthoredTimingSeries.Reset();
	_ContentBox.Reset();
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
		if (_AuthoredMounted)
		{
			DoPresentAuthored(nullptr);
			return;
		}
		_ContentBox->SetContent(DoBuildNativeScroll(DoBuildEmptyContent()));
		return;
	}
	if (_AuthoredMounted)
	{
		if (NOT DoPresentAuthored(&Procs[SelectedIdx]))
		{
			_AuthoredLoadError = TEXT("Scheduler inspector rejected malformed presentation data.");
			DoClearAuthoredProjection();
		}
		return;
	}

	_ContentBox->SetContent(DoBuildNativeScroll(DoBuildProcessorContent(Procs[SelectedIdx])));
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkSchedulerDebugger_Inspector::
	DoBuildAuthoredView()
	-> void
{
	TSharedPtr<FCkUiCollection> Dependencies;
	TSharedPtr<FCkUiCollection> DirtyDetails;
	TSharedPtr<FCkUiCollection> Conflicts;
	TSharedPtr<FCkUiFloatSeries> TimingSeries;
	TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
	const auto DependenciesResult = FCkUiCollection::TryCreate({
		{TEXT("direction"), ECkUiFieldKind::Text},
		{TEXT("name"), ECkUiFieldKind::Text},
		{TEXT("label"), ECkUiFieldKind::Text},
	}, Dependencies);
	const auto TimingResult = FCkUiFloatSeries::TryCreate({}, TimingSeries);
	const auto DirtyResult = FCkUiCollection::TryCreate({
		{TEXT("key"), ECkUiFieldKind::Text}, {TEXT("value"), ECkUiFieldKind::Text},
	}, DirtyDetails);
	const auto ConflictResult = FCkUiCollection::TryCreate({
		{TEXT("peer"), ECkUiFieldKind::Text}, {TEXT("fragment"), ECkUiFieldKind::Text},
		{TEXT("resolution"), ECkUiFieldKind::Text},
	}, Conflicts);
	const auto RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
	const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
	if (NOT DependenciesResult.Succeeded || NOT TimingResult.Succeeded || NOT DirtyResult.Succeeded
		|| NOT ConflictResult.Succeeded || NOT RegistryResult.Succeeded
		|| NOT Registry.IsValid() || NOT Plugin.IsValid())
	{
		auto Errors = TArray<FString>{};
		Errors.Append(DependenciesResult.Errors);
		Errors.Append(TimingResult.Errors);
		Errors.Append(DirtyResult.Errors);
		Errors.Append(ConflictResult.Errors);
		Errors.Append(RegistryResult.Errors);
		if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
		if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
		_AuthoredLoadError = FString::Join(Errors, TEXT("\n"));
		_AuthoredMounted = false;
		return;
	}

	_AuthoredDependencies = Dependencies;
	_AuthoredDirtyDetails = DirtyDetails;
	_AuthoredConflicts = Conflicts;
	_AuthoredTimingSeries = TimingSeries;
	auto Data = FCkUiView::FDataBindings{};
	const TWeakPtr<SCkSchedulerDebugger_Inspector> WeakInspector{SharedThis(this)};
	Data.SlateUserIndex = 0;
	Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakInspector]() { return WeakInspector.IsValid(); });
	auto BindText = [&Data, WeakInspector](const FString& InName, FString SCkSchedulerDebugger_Inspector::* InMember)
	{
		Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakInspector, InMember]()
		{
			const auto Inspector = WeakInspector.Pin();
			return Inspector.IsValid() ? FText::FromString(Inspector.Get()->*InMember) : FText::GetEmpty();
		}));
	};
	BindText(TEXT("scheduler-inspector-name"), &SCkSchedulerDebugger_Inspector::_AuthoredName);
	BindText(TEXT("scheduler-inspector-status"), &SCkSchedulerDebugger_Inspector::_AuthoredStatus);
	BindText(TEXT("scheduler-inspector-group"), &SCkSchedulerDebugger_Inspector::_AuthoredGroup);
	BindText(TEXT("scheduler-inspector-tick-group"), &SCkSchedulerDebugger_Inspector::_AuthoredTickGroup);
	BindText(TEXT("scheduler-inspector-exec-order"), &SCkSchedulerDebugger_Inspector::_AuthoredExecutionOrder);
	BindText(TEXT("scheduler-inspector-current-timing"), &SCkSchedulerDebugger_Inspector::_AuthoredCurrentTiming);
	BindText(TEXT("scheduler-inspector-peak"), &SCkSchedulerDebugger_Inspector::_AuthoredPeakTiming);
	BindText(TEXT("scheduler-inspector-total-ticks"), &SCkSchedulerDebugger_Inspector::_AuthoredTotalTicks);
	BindText(TEXT("scheduler-inspector-tick-rate"), &SCkSchedulerDebugger_Inspector::_AuthoredTickRate);
	BindText(TEXT("scheduler-inspector-dirty-summary"), &SCkSchedulerDebugger_Inspector::_AuthoredDirtySummary);
	BindText(TEXT("scheduler-inspector-conflict-summary"), &SCkSchedulerDebugger_Inspector::_AuthoredConflictSummary);
	Data.Visibility.Add(TEXT("scheduler-inspector-has-selection"), TAttribute<bool>::CreateLambda([WeakInspector]()
	{
		const auto Inspector = WeakInspector.Pin(); return Inspector.IsValid() && Inspector->_AuthoredHasSelection;
	}));
	Data.Visibility.Add(TEXT("scheduler-inspector-empty"), TAttribute<bool>::CreateLambda([WeakInspector]()
	{
		const auto Inspector = WeakInspector.Pin(); return NOT Inspector.IsValid() || NOT Inspector->_AuthoredHasSelection;
	}));
	Data.Visibility.Add(TEXT("scheduler-inspector-has-dirty"), TAttribute<bool>::CreateLambda([WeakInspector]()
	{
		const auto Inspector = WeakInspector.Pin(); return Inspector.IsValid() && Inspector->_AuthoredHasDirty;
	}));
	Data.Visibility.Add(TEXT("scheduler-inspector-has-conflicts"), TAttribute<bool>::CreateLambda([WeakInspector]()
	{
		const auto Inspector = WeakInspector.Pin(); return Inspector.IsValid() && Inspector->_AuthoredHasConflicts;
	}));
	Data.Visibility.Add(TEXT("scheduler-inspector-has-dependencies"), TAttribute<bool>::CreateLambda([WeakInspector]()
	{
		const auto Inspector = WeakInspector.Pin(); return Inspector.IsValid() && Inspector->_AuthoredHasDependencies;
	}));
	Data.Visibility.Add(TEXT("scheduler-inspector-no-dependencies"), TAttribute<bool>::CreateLambda([WeakInspector]()
	{
		const auto Inspector = WeakInspector.Pin(); return NOT Inspector.IsValid() || NOT Inspector->_AuthoredHasDependencies;
	}));
	Data.Visibility.Add(TEXT("scheduler-inspector-is-parallel"), TAttribute<bool>::CreateLambda([WeakInspector]()
	{
		const auto Inspector = WeakInspector.Pin(); return Inspector.IsValid() && Inspector->_AuthoredIsParallel;
	}));
	Data.Color.Add(TEXT("scheduler-inspector-status-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakInspector]()
	{
		const auto Inspector = WeakInspector.Pin(); return Inspector.IsValid() ? Inspector->_AuthoredStatusForeground : FLinearColor::White;
	}));
	Data.Color.Add(TEXT("scheduler-inspector-status-background"), TAttribute<FLinearColor>::CreateLambda([WeakInspector]()
	{
		const auto Inspector = WeakInspector.Pin(); return Inspector.IsValid() ? Inspector->_AuthoredStatusBackground : FLinearColor::Transparent;
	}));
	Data.Collections.Add(TEXT("scheduler-inspector-dependencies"), _AuthoredDependencies);
	Data.Collections.Add(TEXT("scheduler-inspector-dirty-details"), _AuthoredDirtyDetails);
	Data.Collections.Add(TEXT("scheduler-inspector-conflicts"), _AuthoredConflicts);
	Data.FloatSeries.Add(TEXT("scheduler-inspector-timing-samples"), _AuthoredTimingSeries);
	Data.ItemActions.Add(TEXT("scheduler-inspector-navigate"), FCkUiOnItemAction::CreateLambda([WeakInspector](const FString& InKey)
	{
		if (const auto Inspector = WeakInspector.Pin(); Inspector.IsValid()) { Inspector->DoNavigateDependency(InKey); }
	}));

	const auto Candidate = FCkUiView::Create({}, {}, ck_scheduler_debugger_inspector::AuthoredTokens(), FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
	const TSharedRef<SWidget> Main = Candidate->GetRegion(TEXT("main"));
	const auto ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
	Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("SchedulerInspector.ui.html")),
		FPaths::Combine(ResourceRoot, TEXT("SchedulerInspector.ui.css")));
	Candidate->PollFiles(ck_scheduler_debugger_inspector::AuthoredTokens());
	if (NOT Candidate->GetLastResult().Succeeded)
	{
		_AuthoredLoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
		_AuthoredDependencies.Reset();
		_AuthoredDirtyDetails.Reset();
		_AuthoredConflicts.Reset();
		_AuthoredTimingSeries.Reset();
		return;
	}
	_AuthoredView = Candidate;
	_AuthoredMounted = true;
	_ContentBox->SetContent(Main);
}

auto
	SCkSchedulerDebugger_Inspector::
	DoClearAuthoredProjection()
	-> void
{
	if (_AuthoredDependencies.IsValid()) { _AuthoredDependencies->TrySetRecords({}); }
	if (_AuthoredDirtyDetails.IsValid()) { _AuthoredDirtyDetails->TrySetRecords({}); }
	if (_AuthoredConflicts.IsValid()) { _AuthoredConflicts->TrySetRecords({}); }
	if (_AuthoredTimingSeries.IsValid()) { _AuthoredTimingSeries->TrySetSamples({}); }
	_AuthoredName.Reset(); _AuthoredStatus.Reset(); _AuthoredGroup.Reset(); _AuthoredTickGroup.Reset();
	_AuthoredExecutionOrder.Reset(); _AuthoredCurrentTiming.Reset(); _AuthoredPeakTiming.Reset();
	_AuthoredTotalTicks.Reset(); _AuthoredTickRate.Reset(); _AuthoredDirtySummary.Reset(); _AuthoredConflictSummary.Reset();
	_AuthoredHasSelection = false; _AuthoredHasDirty = false; _AuthoredHasConflicts = false; _AuthoredHasDependencies = false;
	_AuthoredIsParallel = false;
	_AuthoredStatusForeground = FLinearColor::White;
	_AuthoredStatusBackground = FLinearColor::Transparent;
}

auto
	SCkSchedulerDebugger_Inspector::
	DoPresentAuthored(const FCkSchedulerDebugger_ProcessorInfo* InProc)
	-> bool
{
	if (InProc == nullptr) { DoClearAuthoredProjection(); return true; }
	if (NOT _AuthoredDependencies.IsValid() || NOT _AuthoredDirtyDetails.IsValid() || NOT _AuthoredConflicts.IsValid()
		|| NOT _AuthoredTimingSeries.IsValid()) { return false; }
	if (NOT FMath::IsFinite(InProc->MainPassTimeMs) || NOT FMath::IsFinite(InProc->TickRate)) { return false; }
	for (const double PumpPassTimeMs : InProc->PumpPassTimesMs)
	{
		if (NOT FMath::IsFinite(PumpPassTimeMs)) { return false; }
	}
	auto Records = TArray<FCkUiRecordData>{};
	auto AddDependency = [&Records, this](const int32 InNodeIndex, const TCHAR* InDirection)
	{
		auto Name = FString{TEXT("(unknown)")};
		if (_ViewModel.IsValid())
		{
			const auto& Procs = _ViewModel->Get_DataCollector().Get_Processors();
			if (const auto* Target = Procs.FindByPredicate([InNodeIndex](const FCkSchedulerDebugger_ProcessorInfo& InCandidate) { return InCandidate.NodeIndex == InNodeIndex; }))
			{ Name = Target->DisplayName; }
		}
		auto Record = FCkUiRecordData{};
		Record.Key = FString::Printf(TEXT("%s:%d"), InDirection, InNodeIndex);
		Record.Fields.Add(TEXT("direction"), {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InDirection)});
		Record.Fields.Add(TEXT("name"), {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Name)});
		Record.Fields.Add(TEXT("label"), {.Kind = ECkUiFieldKind::Text,
			.Text = FText::FromString(FString::Printf(TEXT("%s · %s"), InDirection, *Name))});
		Records.Add(MoveTemp(Record));
	};
	for (const int32 Edge : InProc->InEdges) { AddDependency(Edge, TEXT("Run After")); }
	for (const int32 Edge : InProc->OutEdges) { AddDependency(Edge, TEXT("Run Before")); }
	const bool HasDependencies = NOT Records.IsEmpty();
	auto DirtyRecords = TArray<FCkUiRecordData>{};
	auto AddDetail = [&DirtyRecords](const FString& InKey, const FString& InValue)
	{
		auto Record = FCkUiRecordData{}; Record.Key = InKey;
		Record.Fields.Add(TEXT("key"), {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InKey)});
		Record.Fields.Add(TEXT("value"), {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)});
		DirtyRecords.Add(MoveTemp(Record));
	};
	if (InProc->HasDirtyMarker)
	{
		AddDetail(TEXT("Marker"), InProc->DirtyMarkerName.IsNone() ? TEXT("(unknown)") : InProc->DirtyMarkerName.ToString());
		AddDetail(TEXT("Dirty This Frame"), InProc->WasDirtyThisFrame ? TEXT("Yes") : TEXT("No"));
		AddDetail(TEXT("Pump Count"), FString::FromInt(InProc->PumpCountThisFrame));
		for (int32 Index = 0; Index < InProc->PumpPassTimesMs.Num(); ++Index)
		{ AddDetail(FString::Printf(TEXT("Pump Pass %d"), Index), FString::Printf(TEXT("%.3f ms"), InProc->PumpPassTimesMs[Index])); }
	}
	auto ConflictRecords = TArray<FCkUiRecordData>{};
	for (int32 Index = 0; Index < InProc->WriteConflicts.Num(); ++Index)
	{
		const auto& Conflict = InProc->WriteConflicts[Index];
		auto Record = FCkUiRecordData{}; Record.Key = FString::Printf(TEXT("conflict:%d"), Index);
		Record.Fields.Add(TEXT("peer"), {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Conflict.PeerProcessorName.IsNone() ? TEXT("(unknown)") : Conflict.PeerProcessorName.ToString())});
		Record.Fields.Add(TEXT("fragment"), {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Conflict.FragmentName.IsNone() ? TEXT("(unknown fragment)") : Conflict.FragmentName.ToString())});
		Record.Fields.Add(TEXT("resolution"), {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(Conflict.WasAutoResolved ? TEXT("auto-edge") : TEXT("UNRESOLVED"))});
		ConflictRecords.Add(MoveTemp(Record));
	}
	auto Samples = TArray<float>{};
	double Peak = 0.0;
	for (const double Sample : InProc->TimingHistory)
	{
		if (NOT FMath::IsFinite(Sample)) { return false; }
		const float SampleFloat = static_cast<float>(Sample);
		if (NOT FMath::IsFinite(SampleFloat)) { return false; }
		Samples.Add(SampleFloat); Peak = FMath::Max(Peak, Sample);
	}
	auto Updates = TArray<FCkUiCollectionUpdate>{};
	Updates.Add({.Collection = _AuthoredDependencies, .Records = MoveTemp(Records)});
	Updates.Add({.Collection = _AuthoredDirtyDetails, .Records = MoveTemp(DirtyRecords)});
	Updates.Add({.Collection = _AuthoredConflicts, .Records = MoveTemp(ConflictRecords)});
	const auto CollectionResult = FCkUiCollection::TrySetRecordsBatch(MoveTemp(Updates));
	const auto SeriesResult = _AuthoredTimingSeries->TrySetSamples(MoveTemp(Samples));
	if (NOT CollectionResult.Succeeded || NOT SeriesResult.Succeeded) { return false; }
	_AuthoredName = InProc->DisplayName;
	_AuthoredStatus = InProc->IsGhost ? TEXT("Ghost") : TEXT("Active");
	_AuthoredStatusForeground = InProc->IsGhost ? CkStyle::None() : CkStyle::Ok();
	_AuthoredStatusBackground = InProc->IsGhost ? CkStyle::Bg2() : CkStyle::GetToneDimColor(ECk_Tone::Ok);
	_AuthoredGroup = InProc->GroupName.IsNone() ? TEXT("(ungrouped)") : InProc->GroupName.ToString();
	_AuthoredTickGroup = DoGetTickGroupName(InProc->TickGroup);
	_AuthoredExecutionOrder = InProc->ExecutionOrder == INDEX_NONE ? TEXT("N/A") : FString::Printf(TEXT("#%d"), InProc->ExecutionOrder);
	_AuthoredCurrentTiming = FString::Printf(TEXT("%.3f ms"), InProc->MainPassTimeMs);
	_AuthoredPeakTiming = FString::Printf(TEXT("%.3f ms"), Peak);
	_AuthoredTotalTicks = FString::FromInt(InProc->TotalTicks);
	_AuthoredTickRate = FString::Printf(TEXT("%.1f%%"), InProc->TickRate * 100.0);
	_AuthoredDirtySummary = FString::Printf(TEXT("%s · %s · %d pump pass%s"),
		InProc->DirtyMarkerName.IsNone() ? TEXT("(unknown)") : *InProc->DirtyMarkerName.ToString(),
		InProc->WasDirtyThisFrame ? TEXT("dirty this frame") : TEXT("not dirty this frame"), InProc->PumpCountThisFrame,
		InProc->PumpCountThisFrame == 1 ? TEXT("") : TEXT("es"));
	bool AnyUnresolved = false;
	for (const FCkSchedulerDebugger_WriteConflictInfo& Conflict : InProc->WriteConflicts)
	{ if (NOT Conflict.WasAutoResolved) { AnyUnresolved = true; break; } }
	_AuthoredConflictSummary = InProc->WriteConflicts.IsEmpty() ? FString{} : (AnyUnresolved
		? TEXT("Unresolved write-write conflicts — add RunAfter/RunBefore to fix.")
		: TEXT("Auto-resolved in declaration order — consider adding explicit ordering."));
	_AuthoredHasSelection = true;
	_AuthoredHasDirty = InProc->HasDirtyMarker;
	_AuthoredHasConflicts = NOT InProc->WriteConflicts.IsEmpty();
	_AuthoredHasDependencies = HasDependencies;
	_AuthoredIsParallel = InProc->IsParallel;
	return true;
}

auto
	SCkSchedulerDebugger_Inspector::
	DoBuildNativeScroll(const TSharedRef<SWidget>& InContent)
	-> TSharedRef<SWidget>
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			InContent
		];
}

auto
	SCkSchedulerDebugger_Inspector::
	DoNavigateDependency(const FString& InRecordKey)
	-> void
{
	int32 TargetNodeIndex = INDEX_NONE;
	FString Prefix;
	FString NodeIndexText;
	if (NOT InRecordKey.Split(TEXT(":"), &Prefix, &NodeIndexText)
		|| NOT LexTryParseString(TargetNodeIndex, *NodeIndexText) || NOT _ViewModel.IsValid()) { return; }
	const auto& Procs = _ViewModel->Get_DataCollector().Get_Processors();
	const int32 TargetProcessorIndex = Procs.IndexOfByPredicate([TargetNodeIndex](const FCkSchedulerDebugger_ProcessorInfo& InCandidate) { return InCandidate.NodeIndex == TargetNodeIndex; });
	if (TargetProcessorIndex != INDEX_NONE) { _ViewModel->Set_SelectedProcessorIndex(TargetProcessorIndex); }
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
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_EmptyState)
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
						.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_H2)
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
										.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Micro)
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
										.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Micro)
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
										.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Micro)
										.ColorAndOpacity(FLinearColor::White)
								]
						]
				]
		];

	Content->AddSlot()
		.AutoHeight()
		[
			ck::debug_axes::Make_AxisSeparator()
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
		[ck::debug_axes::Make_AxisSeparator()];

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
		[ck::debug_axes::Make_AxisSeparator()];

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
		[ck::debug_axes::Make_AxisSeparator()];

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
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_Body)
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
		[ck::debug_axes::Make_AxisSeparator()];

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
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
				.ColorAndOpacity(CkStyle::TextDim())
		];

	if (InProc.InEdges.Num() == 0)
	{
		Body->AddSlot().AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("(none)")))
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_Body)
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
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Bold_Body)
				.ColorAndOpacity(CkStyle::TextDim())
		];

	if (InProc.OutEdges.Num() == 0)
	{
		Body->AddSlot().AutoHeight()
			.Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("(none)")))
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_Body)
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
	// Rows are the RowDensity axis' primary surface in this panel. Attribute form, not value: the
	// panel only rebuilds on selection / frame change, so a sampled margin would freeze the axis
	// whenever the selection is static.
	const auto LabelPadding = TAttribute<FMargin>::CreateStatic(&ck::scheduler_debugger_axes::Get_InspectorLabelPadding);
	const auto ValuePadding = TAttribute<FMargin>::CreateLambda([]()
	{
		return ck::debug_axes::Apply_RowDensity(
			FMargin{FCkSchedulerDebuggerStyle::Padding_Small, 2.0f});
	});

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
							.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
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
					.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
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
				.Font_Static(&ck::scheduler_debugger_axes::Get_Font_Regular_Body)
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
