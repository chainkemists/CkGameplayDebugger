#include "CkInsightsDebugger/Window/SCkInsightsAnalyzerTab.h"

#include "HAL/Event.h"
#include "HAL/PlatformTime.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

struct FCkInsightsAnalyzerTabTestAccess
{
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
        Details.MultiStats = MoveTemp(Stats);
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
            _Tab->Tick(
                FGeometry::MakeRoot(FVector2D{1000.0f, 700.0f}, FSlateLayoutTransform{}),
                FPlatformTime::Seconds(),
                0.0f);
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
            _Tab->Tick(
                FGeometry::MakeRoot(FVector2D{1000.0f, 700.0f}, FSlateLayoutTransform{}),
                FPlatformTime::Seconds(),
                0.0f);
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
    FCkInsightsAnalyzerTab_AppliesPreparedMultiFrameDetails,
    "Ck.InsightsDebugger.AnalyzerTab.AppliesPreparedMultiFrameDetails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsAnalyzerTab_AppliesPreparedMultiFrameDetails::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(
        ck_insights_analyzer_tab_spec::FCk_Latent_TabMultiFrameRequest(this));
    return true;
}

#endif
