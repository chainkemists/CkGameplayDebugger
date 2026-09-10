#include "CkInsightsDebugger/Window/SCkInsightsAnalyzerTab.h"

#include "HAL/Event.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkInsightsAnalyzerTabTestAccess
{
    static auto MakeTimingView(uint64 InFrameIndex, uint64 InSelectedFrameCount)
        -> TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe>
    {
        auto Result = FCk_FrameAnalysisResult{};
        Result.FrameIndex = InFrameIndex;
        Result.FrameStartTime = 1.0;
        Result.FrameEndTime = 1.016;
        Result.FrameDurationMs = 16.0;
        Result.HasValidTimeRange = true;
        Result.Events.Add(FCk_TimingEvent{1, 1.0, 1.016, 0});
        Result.Events.Add(FCk_TimingEvent{2, 1.002, 1.006, 1});
        Result.Events.Add(FCk_TimingEvent{3, 1.003, 1.004, 2});
        return FCkInsightsFrameTimingView::Build(
            Result,
            TMap<uint32, FString>{
                {1, TEXT("Synthetic Root")},
                {2, TEXT("Synthetic Child")},
                {3, TEXT("Synthetic Grandchild")}},
            InSelectedFrameCount,
            false);
    }

    static auto QueueSynthetic(
        SCkInsightsAnalyzerTab& InTab,
        uint64 InFrameIndex,
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> InGate = {}) -> void
    {
        InTab._SelectedLiveFrame = InFrameIndex;
        InTab._FrameRequests.Request(InFrameIndex, [InFrameIndex, Gate = MoveTemp(InGate)](const FCancelToken&)
        {
            if (Gate.IsValid())
            {
                Gate->Get()->Wait();
            }

            auto Details = FCkInsightsFrameDetails{};
            Details.Result.FrameIndex = InFrameIndex;
            Details.Result.FrameDurationMs = 16.0;
            auto Event = FCk_TimingEvent{};
            Event.TimerIndex = 1;
            Event.EndTime = 0.016;
            Details.Result.Events.Add(Event);
            Details.Report = FString::Printf(TEXT("synthetic frame %llu"), InFrameIndex);
            return Details;
        });
    }

    static auto QueueSyntheticMulti(
        SCkInsightsAnalyzerTab& InTab,
        uint64 InFrameIndex,
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> InGate = {}) -> void
    {
        InTab._SelectedLiveFrame.Reset();
        InTab._FrameRequests.Request(InFrameIndex, [InFrameIndex, Gate = MoveTemp(InGate)](const FCancelToken&)
        {
            if (Gate.IsValid())
            {
                Gate->Get()->Wait();
            }

            auto Details = FCkInsightsFrameDetails{};
            auto Stats = FCk_MultiFrameStats{};
            Stats.FrameCount = 2;
            Stats.AvgFrameMs = 12.0;
            Stats.SelectedRuns.Add(FCk_FrameRun{InFrameIndex, InFrameIndex + 1});
            Stats.AnalysedFrameIndices = {InFrameIndex, InFrameIndex + 1};

            auto Averaged = FCk_FrameAnalysisResult{};
            Averaged.FrameDurationMs = 12.0;
            Averaged.IsSynthesizedAverage = true;
            Averaged.Events.Add(FCk_TimingEvent{1, 0.0, 0.012, 0});
            Stats.AveragedFrame = MoveTemp(Averaged);
            Details.MultiStats = MoveTemp(Stats);
            Details.Categories.Add(FCk_CategorySummaryEntry{TEXT("Prepared Category"), 4.0, 33.0});
            Details.TopTimers.Add(FCk_TopTimerEntry{TEXT("Prepared Timer"), 3.0, 4.0, 2, 25.0});
            Details.WaitRows.Add(FCk_WaitThreadSummary{7, TEXT("Prepared Wait Thread"), true, 2.0, 12.0});
            Details.Report = FString::Printf(TEXT("synthetic multi %llu"), InFrameIndex);
            return Details;
        });
    }

    static auto ApplySyntheticSingle(SCkInsightsAnalyzerTab& InTab, uint64 InFrameIndex) -> void
    {
        auto Details = FCkInsightsFrameDetails{};
        Details.Result.FrameIndex = InFrameIndex;
        Details.Result.FrameDurationMs = 16.0;
        Details.Result.Events.Add(FCk_TimingEvent{1, 0.0, 0.016, 0});
        Details.TimingView = MakeTimingView(InFrameIndex, 1);
        auto Root = MakeShared<FCk_HotPathNode>();
        Root->TimerIndex = 1;
        Root->RawName = TEXT("Synthetic Root");
        Root->DisplayName = Root->RawName;
        auto Child = MakeShared<FCk_HotPathNode>();
        Child->TimerIndex = 2;
        Child->RawName = TEXT("Synthetic Child");
        Child->DisplayName = Child->RawName;
        auto Grandchild = MakeShared<FCk_HotPathNode>();
        Grandchild->TimerIndex = 3;
        Grandchild->RawName = TEXT("Synthetic Grandchild");
        Grandchild->DisplayName = Grandchild->RawName;
        Child->Children.Add(MoveTemp(Grandchild));
        Root->Children.Add(MoveTemp(Child));
        auto Aggregate = MakeShared<FCk_HotPathNode>();
        Aggregate->bIsAggregate = true;
        Aggregate->RawName = TEXT("Synthetic Aggregate");
        Aggregate->DisplayName = Aggregate->RawName;
        Root->Children.Add(MoveTemp(Aggregate));
        Details.HotPaths.Add(MoveTemp(Root));
        Details.Report = FString::Printf(TEXT("synthetic frame %llu"), InFrameIndex);
        InTab.DoApplyFrameDetails(MoveTemp(Details));
    }

    static auto ApplySyntheticMulti(SCkInsightsAnalyzerTab& InTab, uint64 InFrameIndex) -> void
    {
        auto Details = FCkInsightsFrameDetails{};
        auto Stats = FCk_MultiFrameStats{};
        Stats.FrameCount = 2;
        Stats.AvgFrameMs = 12.0;
        Stats.P95FrameMs = 14.0;
        Stats.P99FrameMs = 15.0;
        Stats.MaxFrameMs = 16.0;
        Stats.SelectedRuns.Add(FCk_FrameRun{InFrameIndex, InFrameIndex + 1});
        Stats.AnalysedFrameIndices = {InFrameIndex, InFrameIndex + 1};
        auto Averaged = FCk_FrameAnalysisResult{};
        Averaged.FrameDurationMs = 12.0;
        Averaged.IsSynthesizedAverage = true;
        Averaged.Events.Add(FCk_TimingEvent{1, 0.0, 0.012, 0});
        Stats.AveragedFrame = MoveTemp(Averaged);
        auto Root = MakeShared<FCk_MergedHotPathNode>();
        Root->TimerIndex = 1;
        Root->RawName = TEXT("Synthetic Root");
        Root->DisplayName = Root->RawName;
        auto Child = MakeShared<FCk_MergedHotPathNode>();
        Child->TimerIndex = 2;
        Child->RawName = TEXT("Synthetic Child");
        Child->DisplayName = Child->RawName;
        auto Grandchild = MakeShared<FCk_MergedHotPathNode>();
        Grandchild->TimerIndex = 3;
        Grandchild->RawName = TEXT("Synthetic Grandchild");
        Grandchild->DisplayName = Grandchild->RawName;
        Child->Children.Add(MoveTemp(Grandchild));
        Root->Children.Add(MoveTemp(Child));
        Stats.MergedHotPaths.Add(MoveTemp(Root));
        Details.MultiStats = MoveTemp(Stats);
        Details.TimingView = MakeTimingView(InFrameIndex, 2);
        Details.Report = FString::Printf(TEXT("synthetic multi %llu"), InFrameIndex);
        InTab.DoApplyFrameDetails(MoveTemp(Details));
    }

    static auto GetSummaryStripDesiredHeight(SCkInsightsAnalyzerTab& InTab) -> float
    {
        InTab._SummaryStrip->SlatePrepass();
        return InTab._SummaryStrip->GetDesiredSize().Y;
    }
    static auto ApplyProgressiveMulti(
        SCkInsightsAnalyzerTab& InTab,
        uint64 InProcessedFrames,
        uint64 InRequestedFrames,
        bool InIntermediate) -> void
    {
        auto Details = FCkInsightsFrameDetails{};
        auto Stats = FCk_MultiFrameStats{};
        Stats.FrameCount = InProcessedFrames;
        Stats.AvgFrameMs = 12.0;
        Stats.SelectedRuns.Add(FCk_FrameRun{100, 109});
        Stats.AnalysedFrameIndices.Reserve(static_cast<int32>(InProcessedFrames));
        for (uint64 Frame = 0; Frame < InProcessedFrames; ++Frame)
        {
            Stats.AnalysedFrameIndices.Add(100 + Frame);
        }
        auto Averaged = FCk_FrameAnalysisResult{};
        Averaged.FrameDurationMs = 12.0;
        Averaged.IsSynthesizedAverage = true;
        Averaged.Events.Add(FCk_TimingEvent{1, 0.0, 0.012, 0});
        Stats.AveragedFrame = MoveTemp(Averaged);

        auto Root = MakeShared<FCk_MergedHotPathNode>();
        Root->RawName = TEXT("Stable Root");
        Root->DisplayName = Root->RawName;
        Root->AvgInclusiveMs = 12.0;
        auto Child = MakeShared<FCk_MergedHotPathNode>();
        Child->RawName = TEXT("Stable Child");
        Child->DisplayName = Child->RawName;
        Child->AvgInclusiveMs = 6.0;
        Root->Children.Add(MoveTemp(Child));
        Stats.MergedHotPaths.Add(MoveTemp(Root));

        Details.MultiStats = MoveTemp(Stats);
        Details.IsIntermediate = InIntermediate;
        Details.ProcessedFrames = InProcessedFrames;
        Details.RequestedFrames = InRequestedFrames;
        Details.TimingView = MakeTimingView(100, InRequestedFrames);
        Details.Report = TEXT("synthetic progressive multi");
        InTab.DoApplyFrameDetails(MoveTemp(Details));
    }

    static auto IsIntermediateMulti(const SCkInsightsAnalyzerTab& InTab) -> bool
    {
        return InTab._MultiDetailsIntermediate;
    }

    static auto GetAveragedScopeLabel(const SCkInsightsAnalyzerTab& InTab) -> const FString&
    {
        return InTab._AveragedScopeLabel;
    }

    static auto GetStatusText(const SCkInsightsAnalyzerTab& InTab) -> FString
    {
        return InTab._StatusText->GetText().ToString();
    }

    static auto SetMergedRootExpanded(SCkInsightsAnalyzerTab& InTab, bool InExpanded) -> void
    {
        InTab._MergedHotPathTree->SetItemExpansion(InTab._MergedHotPathRoots[0], InExpanded);
    }

    static auto IsMergedRootExpanded(const SCkInsightsAnalyzerTab& InTab) -> bool
    {
        return InTab._MergedHotPathTree->IsItemExpanded(InTab._MergedHotPathRoots[0]);
    }
    static auto SetMergedChildSelected(SCkInsightsAnalyzerTab& InTab) -> void
    {
        InTab._MergedHotPathTree->SetItemSelection(InTab._MergedHotPathRoots[0]->Children[0], true);
    }

    static auto IsMergedChildSelected(const SCkInsightsAnalyzerTab& InTab) -> bool
    {
        return InTab._MergedHotPathTree->IsItemSelected(InTab._MergedHotPathRoots[0]->Children[0]);
    }
    static auto ClearResults(SCkInsightsAnalyzerTab& InTab) -> void
    {
        InTab.DoClearResults();
    }

    static auto HasNoSingleFrameResult(const SCkInsightsAnalyzerTab& InTab) -> bool
    {
        return NOT InTab._LastSingleResult.IsSet()
            && NOT InTab._LastMultiStats.IsSet()
            && InTab._ResultsMode == SCkInsightsAnalyzerTab::EResultsMode::None
            && NOT InTab._SelectedLiveFrame.IsSet();
    }

    static auto HasSingleFrameResult(const SCkInsightsAnalyzerTab& InTab, uint64 InFrameIndex) -> bool
    {
        return InTab._LastSingleResult.IsSet()
            && InTab._LastSingleResult->FrameIndex == InFrameIndex
            && InTab._ResultsMode == SCkInsightsAnalyzerTab::EResultsMode::SingleFrame
            && InTab._SelectedLiveFrame.Get(INDEX_NONE) == InFrameIndex;
    }

    static auto HasPreparedMultiFrameResult(const SCkInsightsAnalyzerTab& InTab, uint64 InFrameIndex) -> bool
    {
        return InTab._LastMultiStats.IsSet()
            && InTab._LastMultiStats->SelectedRuns.Num() == 1
            && InTab._LastMultiStats->SelectedRuns[0].FirstFrame == InFrameIndex
            && InTab._ResultsMode == SCkInsightsAnalyzerTab::EResultsMode::MultiFrame
            && InTab._LastSingleResult.IsSet() == false
            && InTab._Categories.Num() == 1 && InTab._Categories[0].Name == TEXT("Prepared Category")
            && InTab._TopTimers.Num() == 1 && InTab._TopTimers[0].Name == TEXT("Prepared Timer")
            && InTab._WaitRows.Num() == 1 && InTab._WaitRows[0].ThreadName == TEXT("Prepared Wait Thread");
    }

    static auto IsFrameRequestBusy(const SCkInsightsAnalyzerTab& InTab) -> bool
    {
        return InTab._FrameRequests.IsBusy();
    }

    static auto GetCurrentReport(const SCkInsightsAnalyzerTab& InTab) -> const FString&
    {
        return InTab._CurrentReport;
    }

    static auto HasTimingView(
        const SCkInsightsAnalyzerTab& InTab,
        uint64 InFrameIndex,
        uint64 InSelectedFrameCount) -> bool
    {
        if (ck::Is_NOT_Valid(InTab._FrameTimingGraph))
        {
            return false;
        }

        const auto View = InTab._FrameTimingGraph->GetView();
        return View.IsValid()
            && View->FrameIndex == InFrameIndex
            && View->SelectedFrameCount == InSelectedFrameCount;
    }

    static auto SelectTimingTimer(SCkInsightsAnalyzerTab& InTab, TOptional<uint32> InTimerIndex) -> void
    {
        if (ck::IsValid(InTab._FrameTimingGraph))
        {
            InTab._FrameTimingGraph->SetTimerSelection(InTimerIndex);
        }
        InTab.DoOnFrameTimingTimerSelectionChanged(InTimerIndex);
    }

    static auto SelectSingleHotPathRow(SCkInsightsAnalyzerTab& InTab, ESelectInfo::Type InSelectInfo) -> void
    {
        auto Node = TSharedPtr<FCk_HotPathNode>{};
        if (InTab._HotPathRoots.IsValidIndex(0)
            && ck::IsValid(InTab._HotPathRoots[0])
            && InTab._HotPathRoots[0]->Children.IsValidIndex(0))
        {
            Node = InTab._HotPathRoots[0]->Children[0];
        }
        if (ck::IsValid(InTab._HotPathTree) && ck::IsValid(Node))
        {
            InTab._HotPathTree->SetSelection(Node, InSelectInfo);
        }
    }

    static auto SelectMergedHotPathRow(SCkInsightsAnalyzerTab& InTab, ESelectInfo::Type InSelectInfo) -> void
    {
        auto Node = TSharedPtr<FCk_MergedHotPathNode>{};
        if (InTab._MergedHotPathRoots.IsValidIndex(0)
            && ck::IsValid(InTab._MergedHotPathRoots[0])
            && InTab._MergedHotPathRoots[0]->Children.IsValidIndex(0))
        {
            Node = InTab._MergedHotPathRoots[0]->Children[0];
        }
        if (ck::IsValid(InTab._MergedHotPathTree) && ck::IsValid(Node))
        {
            InTab._MergedHotPathTree->SetSelection(Node, InSelectInfo);
        }
    }

    static auto SelectSingleAggregateRow(SCkInsightsAnalyzerTab& InTab) -> void
    {
        auto Node = TSharedPtr<FCk_HotPathNode>{};
        if (InTab._HotPathRoots.IsValidIndex(0)
            && ck::IsValid(InTab._HotPathRoots[0])
            && InTab._HotPathRoots[0]->Children.IsValidIndex(1))
        {
            Node = InTab._HotPathRoots[0]->Children[1];
        }
        if (ck::IsValid(InTab._HotPathTree) && ck::IsValid(Node))
        {
            InTab._HotPathTree->SetSelection(Node, ESelectInfo::OnMouseClick);
        }
    }

    static auto ClickEmptyTimingGraph(SCkInsightsAnalyzerTab& InTab) -> void
    {
        if (ck::Is_NOT_Valid(InTab._FrameTimingGraph))
        {
            return;
        }

        const auto Geometry = FGeometry::MakeRoot(FVector2D{400.0f, 150.0f}, FSlateLayoutTransform{});
        const auto Position = FVector2D{300.0f, 100.0f};
        const auto PressedButtons = TSet<FKey>{EKeys::LeftMouseButton};
        const auto Event = FPointerEvent(0, Position, Position, PressedButtons,
            EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InTab._FrameTimingGraph->OnMouseButtonDown(Geometry, Event);
        InTab._FrameTimingGraph->OnMouseButtonUp(Geometry, Event);
    }

    static auto GetTimingSelection(const SCkInsightsAnalyzerTab& InTab) -> TOptional<uint32>
    {
        return InTab._FrameTimingGraph->GetSelectedTimerIndex();
    }

    static auto TickBetweenSlatePasses(SCkInsightsAnalyzerTab& InTab) -> void
    {
        InTab.DoOnCaptureUiTick(0.0f);
    }

    static auto HasSelectedSingleChild(const SCkInsightsAnalyzerTab& InTab, uint32 InTimerIndex) -> bool
    {
        if (ck::Is_NOT_Valid(InTab._HotPathTree)
            || NOT InTab._HotPathRoots.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._HotPathRoots[0])
            || NOT InTab._HotPathRoots[0]->Children.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._HotPathRoots[0]->Children[0]))
        {
            return false;
        }

        const auto& Root = InTab._HotPathRoots[0];
        const auto& Child = Root->Children[0];
        return Child->TimerIndex == InTimerIndex
            && InTab._HotPathTree->IsItemExpanded(Root)
            && InTab._HotPathTree->IsItemSelected(Child);
    }

    static auto HasSelectedMergedChild(const SCkInsightsAnalyzerTab& InTab, uint32 InTimerIndex) -> bool
    {
        if (ck::Is_NOT_Valid(InTab._MergedHotPathTree)
            || NOT InTab._MergedHotPathRoots.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._MergedHotPathRoots[0])
            || NOT InTab._MergedHotPathRoots[0]->Children.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._MergedHotPathRoots[0]->Children[0]))
        {
            return false;
        }

        const auto& Root = InTab._MergedHotPathRoots[0];
        const auto& Child = Root->Children[0];
        return Child->TimerIndex == InTimerIndex
            && InTab._MergedHotPathTree->IsItemExpanded(Root)
            && InTab._MergedHotPathTree->IsItemSelected(Child);
    }

    static auto HasSelectedSingleGrandchild(const SCkInsightsAnalyzerTab& InTab, uint32 InTimerIndex) -> bool
    {
        if (ck::Is_NOT_Valid(InTab._HotPathTree)
            || NOT InTab._HotPathRoots.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._HotPathRoots[0])
            || NOT InTab._HotPathRoots[0]->Children.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._HotPathRoots[0]->Children[0])
            || NOT InTab._HotPathRoots[0]->Children[0]->Children.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._HotPathRoots[0]->Children[0]->Children[0]))
        {
            return false;
        }

        const auto& Root = InTab._HotPathRoots[0];
        const auto& Child = Root->Children[0];
        const auto& Grandchild = Child->Children[0];
        return Grandchild->TimerIndex == InTimerIndex
            && InTab._HotPathTree->IsItemExpanded(Root)
            && InTab._HotPathTree->IsItemExpanded(Child)
            && InTab._HotPathTree->IsItemSelected(Grandchild);
    }

    static auto HasSelectedMergedGrandchild(const SCkInsightsAnalyzerTab& InTab, uint32 InTimerIndex) -> bool
    {
        if (ck::Is_NOT_Valid(InTab._MergedHotPathTree)
            || NOT InTab._MergedHotPathRoots.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._MergedHotPathRoots[0])
            || NOT InTab._MergedHotPathRoots[0]->Children.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._MergedHotPathRoots[0]->Children[0])
            || NOT InTab._MergedHotPathRoots[0]->Children[0]->Children.IsValidIndex(0)
            || ck::Is_NOT_Valid(InTab._MergedHotPathRoots[0]->Children[0]->Children[0]))
        {
            return false;
        }

        const auto& Root = InTab._MergedHotPathRoots[0];
        const auto& Child = Root->Children[0];
        const auto& Grandchild = Child->Children[0];
        return Grandchild->TimerIndex == InTimerIndex
            && InTab._MergedHotPathTree->IsItemExpanded(Root)
            && InTab._MergedHotPathTree->IsItemExpanded(Child)
            && InTab._MergedHotPathTree->IsItemSelected(Grandchild);
    }

    static auto GetSingleTreeSelectionCount(const SCkInsightsAnalyzerTab& InTab) -> int32
    {
        return ck::IsValid(InTab._HotPathTree)
            ? InTab._HotPathTree->GetNumItemsSelected()
            : 0;
    }

    static auto MakeScreenshotResourceName(
        SCkInsightsAnalyzerTab& InTab,
        uint32 InScreenshotId,
        const TCHAR* InSuffix) -> FName
    {
        return InTab.DoMakeScreenshotResourceName(InScreenshotId, InSuffix);
    }

    static auto FinishLoadedTraceWithAutomatedReport(
        SCkInsightsAnalyzerTab& InTab,
        const FString& InTracePath) -> bool
    {
        if (NOT InTab._Session->PrepareAnalysisService() || NOT InTab._Session->StartAnalysis(InTracePath))
        {
            return false;
        }

        const double Deadline = FPlatformTime::Seconds() + 30.0;
        while (FPlatformTime::Seconds() < Deadline && NOT InTab._Session->IsAnalysisComplete())
        {
            FPlatformProcess::Sleep(0.001f);
        }
        if (NOT InTab._Session->IsAnalysisComplete())
        {
            return false;
        }

        const auto Frames = InTab._Session->ReadAvailableFrames(0);
        InTab._PendingTracePath = InTracePath;
        InTab._PendingAutoReportTracePath = InTracePath;
        InTab._LoadingState = SCkInsightsAnalyzerTab::ELoadingState::ReadingFrames;
        InTab._LoadedFrameCount = Frames.Durations.Num();
        InTab._TotalFrameCount = Frames.FrameCount;
        InTab.DoFinishLoading();
        return true;
    }

    static auto IsAutomatedReportQueued(const SCkInsightsAnalyzerTab& InTab) -> bool
    {
        return InTab._FrameRequests.IsBusy()
            && InTab._AutoOpenReportPending
            && NOT InTab._AutoOpenReportGenerated;
    }

    static auto WaitForAutomatedReport(SCkInsightsAnalyzerTab& InTab) -> bool
    {
        const double Deadline = FPlatformTime::Seconds() + 30.0;
        while (FPlatformTime::Seconds() < Deadline && InTab._FrameRequests.IsBusy())
        {
            InTab.DoPollFrameDetails();
            FPlatformProcess::Sleep(0.001f);
        }
        InTab.DoPollFrameDetails();
        return NOT InTab._AutoOpenReportPending && InTab._AutoOpenReportGenerated;
    }

    static auto AnalyzeFirstTwoFrames(SCkInsightsAnalyzerTab& InTab) -> bool
    {
        if (InTab._TotalFrameCount < 2)
        {
            return false;
        }

        InTab.DoAnalyzeFrameSet(TArray<FCk_FrameRun>{FCk_FrameRun{0, 1}});
        const auto Deadline = FPlatformTime::Seconds() + 30.0;
        while (FPlatformTime::Seconds() < Deadline && InTab._FrameRequests.IsBusy())
        {
            InTab.DoPollFrameDetails();
            FPlatformProcess::Sleep(0.001f);
        }
        InTab.DoPollFrameDetails();
        return NOT InTab._FrameRequests.IsBusy() && HasTimingView(InTab, 0, 2);
    }

    static auto CanOpenLoadedTraceInUnrealInsights(const SCkInsightsAnalyzerTab& InTab) -> bool
    {
        return InTab.DoCanOpenLoadedTraceInUnrealInsights();
    }

    static auto AutoOpenWaitsForAutomatedReport(SCkInsightsAnalyzerTab& InTab) -> bool
    {
        InTab._AutoOpenTraceOpeningStarted = true;
        return InTab.DoOnAutoOpenTraceTick(0.0f);
    }

    static auto InteractiveAnalysisDoesNotCancelAutomatedReport(SCkInsightsAnalyzerTab& InTab) -> bool
    {
        InTab.DoOnAnalyzeWorstClicked();
        return IsAutomatedReportQueued(InTab);
    }

    static auto OpeningAnotherTraceDoesNotCancelAutomatedReport(
        SCkInsightsAnalyzerTab& InTab,
        const FString& InTracePath) -> bool
    {
        InTab.DoOpenTracePath(InTracePath);
        return IsAutomatedReportQueued(InTab);
    }

    static auto ManualOpenSupersedesPendingAutomatedCapture(
        SCkInsightsAnalyzerTab& InTab,
        FGuid InPendingCaptureGuid,
        FGuid InRetainedCaptureGuid) -> bool
    {
        const auto AutomatedTracePath = FPaths::ProjectSavedDir() / TEXT("Profiling/PendingCapture.utrace");
        InTab._PendingAutoOpenTraceGuid = InPendingCaptureGuid;
        InTab._PendingAutoOpenTracePath = AutomatedTracePath;
        InTab._PendingAutoReportTracePath = AutomatedTracePath;

        InTab.DoSupersedePendingAutoOpenForManualTrace(InRetainedCaptureGuid);

        const auto ExpectedSuppressedGuid = InRetainedCaptureGuid.IsValid()
            ? InRetainedCaptureGuid
            : InPendingCaptureGuid;

        return NOT InTab._PendingAutoOpenTraceGuid.IsValid()
            && InTab._PendingAutoOpenTracePath.IsEmpty()
            && InTab._PendingAutoReportTracePath.IsEmpty()
            && InTab._SuppressedAutoOpenTraceGuid == ExpectedSuppressedGuid;
    }

    static auto GetUnrealInsightsExecutablePath() -> FString
    {
        return SCkInsightsAnalyzerTab::DoGet_UnrealInsightsExecutablePath();
    }

    static auto GetUnrealInsightsOpenTraceArguments(const FString& InTracePath) -> FString
    {
        return SCkInsightsAnalyzerTab::DoGet_UnrealInsightsOpenTraceArguments(InTracePath);
    }
};

namespace ck_insights_analyzer_tab_spec
{
    class FCk_Latent_TabLatestRequest final : public IAutomationLatentCommand
    {
    public:
        explicit FCk_Latent_TabLatestRequest(FAutomationTestBase* InTest)
            : _Test(InTest)
            , _Tab(SNew(SCkInsightsAnalyzerTab))
            , _Gate(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
        {}

        virtual auto Update() -> bool override
        {
            if (FPlatformTime::Seconds() > _Deadline)
            {
                _Gate->Get()->Trigger();
                _Test->AddError(TEXT("Insights tab request integration test timed out."));
                return true;
            }

            switch (_Phase)
            {
            case EPhase::Start:
                FCkInsightsAnalyzerTabTestAccess::QueueSynthetic(*_Tab, 10, _Gate);
                FCkInsightsAnalyzerTabTestAccess::ClearResults(*_Tab);
                _Gate->Get()->Trigger();
                _Phase = EPhase::AwaitClearedRequest;
                return false;

            case EPhase::AwaitClearedRequest:
                TickTab();
                if (NOT FCkInsightsAnalyzerTabTestAccess::IsFrameRequestBusy(*_Tab))
                {
                    _Test->TestTrue(
                        TEXT("A completion released after ClearResults cannot repopulate the tab"),
                        FCkInsightsAnalyzerTabTestAccess::HasNoSingleFrameResult(*_Tab));
                    _Gate->Get()->Reset();
                    FCkInsightsAnalyzerTabTestAccess::QueueSynthetic(*_Tab, 20, _Gate);
                    FCkInsightsAnalyzerTabTestAccess::QueueSynthetic(*_Tab, 30);
                    _Gate->Get()->Trigger();
                    _Phase = EPhase::AwaitLatestRequest;
                }
                return false;

            case EPhase::AwaitLatestRequest:
                TickTab();
                if (FCkInsightsAnalyzerTabTestAccess::HasSingleFrameResult(*_Tab, 30))
                {
                    _Test->TestTrue(
                        TEXT("The newest queued frame is the only result applied to the tab"),
                        FCkInsightsAnalyzerTabTestAccess::HasSingleFrameResult(*_Tab, 30));
                    _Test->TestEqual(TEXT("The applied details preserve the newest report"),
                        FCkInsightsAnalyzerTabTestAccess::GetCurrentReport(*_Tab), FString{TEXT("synthetic frame 30")});
                    return true;
                }
                return false;
            }

            return true;
        }

    private:
        enum class EPhase : uint8
        {
            Start,
            AwaitClearedRequest,
            AwaitLatestRequest,
        };

        auto TickTab() -> void
        {
            FCkInsightsAnalyzerTabTestAccess::TickBetweenSlatePasses(*_Tab);
        }

        FAutomationTestBase* _Test = nullptr;
        TSharedRef<SCkInsightsAnalyzerTab> _Tab;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _Gate;
        EPhase _Phase = EPhase::Start;
        double _Deadline = FPlatformTime::Seconds() + 5.0;
    };

    class FCk_Latent_TabMultiFrameRequest final : public IAutomationLatentCommand
    {
    public:
        explicit FCk_Latent_TabMultiFrameRequest(FAutomationTestBase* InTest)
            : _Test(InTest)
            , _Tab(SNew(SCkInsightsAnalyzerTab))
            , _Gate(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
        {}

        virtual auto Update() -> bool override
        {
            if (FPlatformTime::Seconds() > _Deadline)
            {
                _Gate->Get()->Trigger();
                _Test->AddError(TEXT("Insights tab multi-frame request integration test timed out."));
                return true;
            }

            switch (_Phase)
            {
            case EPhase::Start:
                FCkInsightsAnalyzerTabTestAccess::QueueSyntheticMulti(*_Tab, 100);
                _Phase = EPhase::AwaitPreparedMulti;
                return false;

            case EPhase::AwaitPreparedMulti:
                TickTab();
                if (FCkInsightsAnalyzerTabTestAccess::HasPreparedMultiFrameResult(*_Tab, 100))
                {
                    _Test->TestTrue(TEXT("Prepared multi-frame payload applies without an open trace session"), true);
                    _Test->TestEqual(TEXT("Prepared multi-frame report reaches the tab"),
                        FCkInsightsAnalyzerTabTestAccess::GetCurrentReport(*_Tab), FString{TEXT("synthetic multi 100")});
                    _Gate->Get()->Reset();
                    FCkInsightsAnalyzerTabTestAccess::QueueSyntheticMulti(*_Tab, 200, _Gate);
                    FCkInsightsAnalyzerTabTestAccess::QueueSynthetic(*_Tab, 300);
                    _Gate->Get()->Trigger();
                    _Phase = EPhase::AwaitSingleSupersedingMulti;
                }
                return false;

            case EPhase::AwaitSingleSupersedingMulti:
                TickTab();
                if (FCkInsightsAnalyzerTabTestAccess::HasSingleFrameResult(*_Tab, 300))
                {
                    _Test->TestTrue(TEXT("A newer single-frame request drops the pending multi-frame result"), true);
                    _Gate->Get()->Reset();
                    FCkInsightsAnalyzerTabTestAccess::QueueSyntheticMulti(*_Tab, 400, _Gate);
                    FCkInsightsAnalyzerTabTestAccess::ClearResults(*_Tab);
                    _Gate->Get()->Trigger();
                    _Phase = EPhase::AwaitClearedMulti;
                }
                return false;

            case EPhase::AwaitClearedMulti:
                TickTab();
                if (NOT FCkInsightsAnalyzerTabTestAccess::IsFrameRequestBusy(*_Tab))
                {
                    _Test->TestTrue(TEXT("Clearing drops an old multi-frame completion"),
                        FCkInsightsAnalyzerTabTestAccess::HasNoSingleFrameResult(*_Tab));
                    return true;
                }
                return false;
            }

            return true;
        }

    private:
        enum class EPhase : uint8
        {
            Start,
            AwaitPreparedMulti,
            AwaitSingleSupersedingMulti,
            AwaitClearedMulti,
        };

        auto TickTab() -> void
        {
            FCkInsightsAnalyzerTabTestAccess::TickBetweenSlatePasses(*_Tab);
        }

        FAutomationTestBase* _Test = nullptr;
        TSharedRef<SCkInsightsAnalyzerTab> _Tab;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _Gate;
        EPhase _Phase = EPhase::Start;
        double _Deadline = FPlatformTime::Seconds() + 5.0;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_DropsLateFrameDetails,
    "Ck.InsightsDebugger.AnalyzerTab.DropsLateFrameDetails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_DropsLateFrameDetails::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(
        ck_insights_analyzer_tab_spec::FCk_Latent_TabLatestRequest(this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_PreservesProgressiveMultiFrameUiState,
    "Ck.InsightsDebugger.AnalyzerTab.PreservesProgressiveMultiFrameUiState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_PreservesProgressiveMultiFrameUiState::RunTest(const FString&)
{
    auto Tab = SNew(SCkInsightsAnalyzerTab);
    FCkInsightsAnalyzerTabTestAccess::ApplyProgressiveMulti(*Tab, 2, 10, true);

    TestTrue(TEXT("The first progressive payload marks multi-frame details as intermediate"),
        FCkInsightsAnalyzerTabTestAccess::IsIntermediateMulti(*Tab));
    TestTrue(TEXT("The partial scope label uses its analyzed frame count, not the full selection"),
        FCkInsightsAnalyzerTabTestAccess::GetAveragedScopeLabel(*Tab).Contains(TEXT("Average of 2 frames")));
    TestTrue(TEXT("The partial status labels processed frames against the requested denominator"),
        FCkInsightsAnalyzerTabTestAccess::GetStatusText(*Tab).Contains(TEXT("2 / 10")));

    FCkInsightsAnalyzerTabTestAccess::SetMergedRootExpanded(*Tab, false);
    FCkInsightsAnalyzerTabTestAccess::SetMergedChildSelected(*Tab);
    FCkInsightsAnalyzerTabTestAccess::ApplyProgressiveMulti(*Tab, 4, 10, true);
    TestTrue(TEXT("A later partial payload keeps a user-collapsed merged root collapsed"),
        NOT FCkInsightsAnalyzerTabTestAccess::IsMergedRootExpanded(*Tab));
    TestTrue(TEXT("Later partial details remain marked intermediate"),
        FCkInsightsAnalyzerTabTestAccess::IsIntermediateMulti(*Tab));
    TestTrue(TEXT("A later partial payload preserves the selected merged row by path"),
        FCkInsightsAnalyzerTabTestAccess::IsMergedChildSelected(*Tab));
    FCkInsightsAnalyzerTabTestAccess::ApplyProgressiveMulti(*Tab, 10, 10, false);
    TestTrue(TEXT("The final payload clears the intermediate multi-frame state"),
        NOT FCkInsightsAnalyzerTabTestAccess::IsIntermediateMulti(*Tab));
    TestTrue(TEXT("The final payload preserves the user-collapsed merged root"),
        NOT FCkInsightsAnalyzerTabTestAccess::IsMergedRootExpanded(*Tab));
    TestTrue(TEXT("The final payload preserves the selected merged row by path"),
        FCkInsightsAnalyzerTabTestAccess::IsMergedChildSelected(*Tab));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_SummaryStripRetainsDesiredHeight,
    "Ck.InsightsDebugger.AnalyzerTab.SummaryStripRetainsDesiredHeight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_SummaryStripRetainsDesiredHeight::RunTest(const FString&)
{
    auto Tab = SNew(SCkInsightsAnalyzerTab);
    const float EmptyHeight = FCkInsightsAnalyzerTabTestAccess::GetSummaryStripDesiredHeight(*Tab);

    FCkInsightsAnalyzerTabTestAccess::ClearResults(*Tab);
    const float ClearedHeight = FCkInsightsAnalyzerTabTestAccess::GetSummaryStripDesiredHeight(*Tab);

    FCkInsightsAnalyzerTabTestAccess::ApplySyntheticSingle(*Tab, 101);
    const float SingleHeight = FCkInsightsAnalyzerTabTestAccess::GetSummaryStripDesiredHeight(*Tab);

    FCkInsightsAnalyzerTabTestAccess::ApplySyntheticMulti(*Tab, 201);
    const float MultiHeight = FCkInsightsAnalyzerTabTestAccess::GetSummaryStripDesiredHeight(*Tab);

    TestTrue(TEXT("The empty summary strip reserves a visible desired height"), EmptyHeight > 0.0f);
    TestTrue(TEXT("Clearing results retains the summary strip desired height"),
        FMath::IsNearlyEqual(ClearedHeight, EmptyHeight));
    TestTrue(TEXT("A single-frame summary retains the reserved desired height"),
        FMath::IsNearlyEqual(SingleHeight, EmptyHeight));
    TestTrue(TEXT("A multi-frame summary retains the reserved desired height"),
        FMath::IsNearlyEqual(MultiHeight, EmptyHeight));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_PresentsExactSelectedFrameTiming,
    "Ck.InsightsDebugger.AnalyzerTab.PresentsExactSelectedFrameTiming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_PresentsExactSelectedFrameTiming::RunTest(const FString&)
{
    auto Tab = SNew(SCkInsightsAnalyzerTab);
    FCkInsightsAnalyzerTabTestAccess::ApplySyntheticSingle(*Tab, 101);
    TestTrue(TEXT("Single-frame analysis presents that exact frame's timing hierarchy"),
        FCkInsightsAnalyzerTabTestAccess::HasTimingView(*Tab, 101, 1));

    FCkInsightsAnalyzerTabTestAccess::ApplySyntheticMulti(*Tab, 201);
    TestTrue(TEXT("Multi-frame analysis presents the first selected frame, not an averaged topology"),
        FCkInsightsAnalyzerTabTestAccess::HasTimingView(*Tab, 201, 2));

    FCkInsightsAnalyzerTabTestAccess::ClearResults(*Tab);
    TestFalse(TEXT("Clearing analysis removes the timing hierarchy"),
        FCkInsightsAnalyzerTabTestAccess::HasTimingView(*Tab, 201, 2));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_LinksTimingSelectionToHotPathTree,
    "Ck.InsightsDebugger.AnalyzerTab.LinksTimingSelectionToHotPathTree",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_LinksTimingSelectionToHotPathTree::RunTest(const FString&)
{
    auto Tab = SNew(SCkInsightsAnalyzerTab);
    FCkInsightsAnalyzerTabTestAccess::ApplySyntheticSingle(*Tab, 101);
    FCkInsightsAnalyzerTabTestAccess::SelectTimingTimer(*Tab, 3);
    TestTrue(TEXT("A timing selection expands every ancestor and selects its exact single-frame timer row"),
        FCkInsightsAnalyzerTabTestAccess::HasSelectedSingleGrandchild(*Tab, 3));

    FCkInsightsAnalyzerTabTestAccess::SelectTimingTimer(*Tab, {});
    TestEqual(TEXT("An empty timing selection clears the single-frame tree selection"),
        FCkInsightsAnalyzerTabTestAccess::GetSingleTreeSelectionCount(*Tab), 0);

    FCkInsightsAnalyzerTabTestAccess::ApplySyntheticMulti(*Tab, 201);
    FCkInsightsAnalyzerTabTestAccess::SelectTimingTimer(*Tab, 3);
    TestTrue(TEXT("A first-frame timing selection expands every ancestor and selects its averaged timer row"),
        FCkInsightsAnalyzerTabTestAccess::HasSelectedMergedGrandchild(*Tab, 3));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_LinksHotPathTreeSelectionToTiming,
    "Ck.InsightsDebugger.AnalyzerTab.LinksHotPathTreeSelectionToTiming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_LinksHotPathTreeSelectionToTiming::RunTest(const FString&)
{
    auto Tab = SNew(SCkInsightsAnalyzerTab);
    FCkInsightsAnalyzerTabTestAccess::ApplySyntheticSingle(*Tab, 101);
    FCkInsightsAnalyzerTabTestAccess::SelectSingleHotPathRow(*Tab, ESelectInfo::OnMouseClick);
    const auto SingleSelection = FCkInsightsAnalyzerTabTestAccess::GetTimingSelection(*Tab);
    TestTrue(TEXT("A clicked single-frame row selects its timer in the timing graph"),
        SingleSelection.IsSet() && SingleSelection.GetValue() == 2);

    FCkInsightsAnalyzerTabTestAccess::SelectTimingTimer(*Tab, 1);
    FCkInsightsAnalyzerTabTestAccess::SelectSingleHotPathRow(*Tab, ESelectInfo::Direct);
    const auto DirectSelection = FCkInsightsAnalyzerTabTestAccess::GetTimingSelection(*Tab);
    TestTrue(TEXT("A direct tree selection does not echo back into the timing graph"),
        DirectSelection.IsSet() && DirectSelection.GetValue() == 1);

    FCkInsightsAnalyzerTabTestAccess::SelectSingleAggregateRow(*Tab);
    TestFalse(TEXT("An aggregate row has no graph timer identity"),
        FCkInsightsAnalyzerTabTestAccess::GetTimingSelection(*Tab).IsSet());
    TestEqual(TEXT("The aggregate row remains selected until the user clears it"),
        FCkInsightsAnalyzerTabTestAccess::GetSingleTreeSelectionCount(*Tab), 1);
    FCkInsightsAnalyzerTabTestAccess::ClickEmptyTimingGraph(*Tab);
    TestEqual(TEXT("Empty graph space clears an aggregate tree selection even when graph selection was already empty"),
        FCkInsightsAnalyzerTabTestAccess::GetSingleTreeSelectionCount(*Tab), 0);

    FCkInsightsAnalyzerTabTestAccess::ApplySyntheticMulti(*Tab, 201);
    FCkInsightsAnalyzerTabTestAccess::SelectMergedHotPathRow(*Tab, ESelectInfo::OnMouseClick);
    const auto MergedSelection = FCkInsightsAnalyzerTabTestAccess::GetTimingSelection(*Tab);
    TestTrue(TEXT("A clicked averaged row selects all matching first-frame occurrences"),
        MergedSelection.IsSet() && MergedSelection.GetValue() == 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_UsesProcessUniqueScreenshotResourceNames,
    "Ck.InsightsDebugger.AnalyzerTab.UsesProcessUniqueScreenshotResourceNames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_UsesProcessUniqueScreenshotResourceNames::RunTest(const FString&)
{
    auto FirstTab = SNew(SCkInsightsAnalyzerTab);
    auto SecondTab = SNew(SCkInsightsAnalyzerTab);
    const auto FirstName = FCkInsightsAnalyzerTabTestAccess::MakeScreenshotResourceName(
        *FirstTab, 7, TEXT("Thumbnail"));
    const auto FirstNextName = FCkInsightsAnalyzerTabTestAccess::MakeScreenshotResourceName(
        *FirstTab, 7, TEXT("Thumbnail"));
    const auto SecondName = FCkInsightsAnalyzerTabTestAccess::MakeScreenshotResourceName(
        *SecondTab, 7, TEXT("Thumbnail"));

    TestNotEqual(TEXT("Repeated resources within one tab receive distinct names"), FirstName, FirstNextName);
    TestNotEqual(TEXT("The same screenshot in a replacement tab cannot collide globally"), FirstName, SecondName);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_AppliesPreparedMultiFrameDetails,
    "Ck.InsightsDebugger.AnalyzerTab.AppliesPreparedMultiFrameDetails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_AppliesPreparedMultiFrameDetails::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(
        ck_insights_analyzer_tab_spec::FCk_Latent_TabMultiFrameRequest(this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_ManualOpenSupersedesPendingAutomatedCapture,
    "Ck.InsightsDebugger.AnalyzerTab.ManualOpenSupersedesPendingAutomatedCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_ManualOpenSupersedesPendingAutomatedCapture::RunTest(const FString&)
{
    const auto PendingCaptureGuid = FGuid::NewGuid();
    auto PendingTab = SNew(SCkInsightsAnalyzerTab);
    TestTrue(TEXT("A manual trace clears and suppresses the locally pending automatic capture"),
        FCkInsightsAnalyzerTabTestAccess::ManualOpenSupersedesPendingAutomatedCapture(
            *PendingTab,
            PendingCaptureGuid,
            {}));

    const auto NewRetainedCaptureGuid = FGuid::NewGuid();
    auto ChangedCompletionTab = SNew(SCkInsightsAnalyzerTab);
    TestTrue(TEXT("A manual trace suppresses a newer retained completion not yet discovered by the tab ticker"),
        FCkInsightsAnalyzerTabTestAccess::ManualOpenSupersedesPendingAutomatedCapture(
            *ChangedCompletionTab,
            PendingCaptureGuid,
            NewRetainedCaptureGuid));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsAnalyzerTab_QueuesAutomatedReportAfterLoading,
    "Ck.InsightsDebugger.AnalyzerTab.QueuesAutomatedReportAfterLoading",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_QueuesAutomatedReportAfterLoading::RunTest(const FString&)
{
    auto TracePath = FPlatformMisc::GetEnvironmentVariable(TEXT("CK_INSIGHTS_TEST_TRACE"));
    const bool UsesEnvironmentTrace = NOT TracePath.IsEmpty();
    if (NOT UsesEnvironmentTrace)
    {
        TracePath = FPaths::ProjectSavedDir() / TEXT("Profiling/20260807_192547_5F8520.utrace");
    }
    if (NOT IFileManager::Get().FileExists(*TracePath))
    {
        if (UsesEnvironmentTrace)
        {
            AddError(FString::Printf(TEXT("CK_INSIGHTS_TEST_TRACE does not exist: %s"), *TracePath));
            return false;
        }

        AddWarning(TEXT("SKIPPED: automated-report fixture is not present in Saved/Profiling."));
        return true;
    }

    auto Tab = SNew(SCkInsightsAnalyzerTab);
    if (NOT TestTrue(TEXT("The real trace completes loading"),
        FCkInsightsAnalyzerTabTestAccess::FinishLoadedTraceWithAutomatedReport(*Tab, TracePath)))
    {
        return false;
    }

    TestTrue(TEXT("Finishing trace loading queues automated reporting without completing it inline"),
        FCkInsightsAnalyzerTabTestAccess::IsAutomatedReportQueued(*Tab));
    TestTrue(TEXT("Interactive analysis cannot cancel the automatic capture report"),
        FCkInsightsAnalyzerTabTestAccess::InteractiveAnalysisDoesNotCancelAutomatedReport(*Tab));
    TestTrue(TEXT("Opening another trace cannot cancel the automatic capture report"),
        FCkInsightsAnalyzerTabTestAccess::OpeningAnotherTraceDoesNotCancelAutomatedReport(*Tab, TracePath));
    TestTrue(TEXT("Capture completion remains pending while the automatic report is running"),
        FCkInsightsAnalyzerTabTestAccess::AutoOpenWaitsForAutomatedReport(*Tab));
    TestTrue(TEXT("The background automated report completes and is applied"),
        FCkInsightsAnalyzerTabTestAccess::WaitForAutomatedReport(*Tab));
    TestTrue(TEXT("A real two-frame selection presents its first exact frame timing hierarchy"),
        FCkInsightsAnalyzerTabTestAccess::AnalyzeFirstTwoFrames(*Tab));
    TestTrue(TEXT("The loaded trace is eligible for the Unreal Insights launch action"),
        FCkInsightsAnalyzerTabTestAccess::CanOpenLoadedTraceInUnrealInsights(*Tab));
    TestTrue(TEXT("The Unreal Insights executable resolves through the platform application path"),
        FCkInsightsAnalyzerTabTestAccess::GetUnrealInsightsExecutablePath().EndsWith(TEXT("UnrealInsights.exe")));
    TestEqual(TEXT("The Unreal Insights action passes the exact loaded trace path"),
        FCkInsightsAnalyzerTabTestAccess::GetUnrealInsightsOpenTraceArguments(TracePath),
        FString::Printf(TEXT("-OpenTraceFile=\"%s\""), *FPaths::ConvertRelativePathToFull(TracePath)));
    FCkInsightsAnalyzerTabTestAccess::ClearResults(*Tab);
    return true;
}

#endif
