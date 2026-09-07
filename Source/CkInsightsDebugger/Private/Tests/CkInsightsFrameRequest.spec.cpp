#include "CkInsightsDebugger/Analysis/CkInsightsFrameRequest.h"

#include "HAL/Event.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_insights_frame_request_spec
{
    auto MakeDetails(uint64 InWorkerFrameIndex) -> FCkInsightsFrameDetails
    {
        auto Details = FCkInsightsFrameDetails{};
        Details.Result.FrameIndex = InWorkerFrameIndex;
        Details.Report = FString::Printf(TEXT("frame %llu"), InWorkerFrameIndex);
        return Details;
    }

    auto MakeMultiDetails(uint64 InWorkerFrameIndex) -> FCkInsightsFrameDetails
    {
        auto Details = MakeDetails(InWorkerFrameIndex);
        Details.MultiStats.Emplace();
        Details.Report = FString::Printf(TEXT("multi frame %llu"), InWorkerFrameIndex);
        return Details;
    }

    auto MakeProgress(uint64 InWorkerFrameIndex, uint64 InProcessedFrames, uint64 InRequestedFrames)
        -> FCkInsightsFrameDetails
    {
        auto Details = MakeDetails(InWorkerFrameIndex);
        Details.ProcessedFrames = InProcessedFrames;
        Details.RequestedFrames = InRequestedFrames;
        Details.Report = FString::Printf(TEXT("progress %llu/%llu"), InProcessedFrames, InRequestedFrames);
        return Details;
    }

    class FCk_Latent_FrameRequestQueue final : public IAutomationLatentCommand
    {
    public:
        explicit FCk_Latent_FrameRequestQueue(FAutomationTestBase* InTest)
            : _Test(InTest)
            , _Gate(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
        {}

        virtual auto Update() -> bool override
        {
            const auto Now = FPlatformTime::Seconds();
            if (Now > _Deadline)
            {
                (*_Gate)->Trigger();
                _Test->AddError(TEXT("Frame request queue test timed out."));
                return true;
            }

            switch (_Phase)
            {
            case EPhase::Start:
                _Queue.Request(10, [Gate = _Gate](const FCancelToken& InCancelToken)
                {
                    (*Gate)->Wait();
                    return MakeDetails(InCancelToken->Load() ? 1000 : 10);
                });
                _Queue.Request(20, [](const FCancelToken&) { return MakeDetails(20); });
                _Queue.Request(30, [](const FCancelToken&) { return MakeDetails(30); });
                (*_Gate)->Trigger();
                _Phase = EPhase::AwaitCoalescedResult;
                return false;

            case EPhase::AwaitCoalescedResult:
                if (const auto Details = _Queue.Poll())
                {
                    _Test->TestEqual(TEXT("Late cancelled completion is rejected and newest request wins"),
                        Details->Result.FrameIndex, uint64{30});
                    _Test->TestEqual(TEXT("Only the latest pending request ran"), Details->Report, TEXT("frame 30"));
                    (*_Gate)->Reset();
                    _Queue.Request(40, [Gate = _Gate](const FCancelToken&)
                    {
                        (*Gate)->Wait();
                        return MakeDetails(40);
                    });
                    _Phase = EPhase::ResetActiveRequest;
                }
                return false;

            case EPhase::ResetActiveRequest:
                _Queue.Reset();
                (*_Gate)->Trigger();
                _Phase = EPhase::AwaitResetCompletion;
                return false;

            case EPhase::AwaitResetCompletion:
                if (NOT _Queue.IsBusy())
                {
                    _Test->TestFalse(TEXT("Reset completion never reaches the owner"), _Queue.Poll().IsSet());
                    _Queue.Request(50, [](const FCancelToken&) { return MakeDetails(5000); });
                    _Phase = EPhase::AwaitCurrentResult;
                }
                else
                {
                    _Test->TestFalse(TEXT("Reset discards a late completion"), _Queue.Poll().IsSet());
                }
                return false;

            case EPhase::AwaitCurrentResult:
                if (const auto Details = _Queue.Poll())
                {
                    _Test->TestEqual(TEXT("Current completion is delivered"), Details->Result.FrameIndex, uint64{50});
                    _Test->TestTrue(TEXT("Queue drains after the current completion"), NOT _Queue.IsBusy());
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
            AwaitCoalescedResult,
            ResetActiveRequest,
            AwaitResetCompletion,
            AwaitCurrentResult,
        };

        FAutomationTestBase* _Test = nullptr;
        FCkInsightsFrameRequestQueue _Queue;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _Gate;
        EPhase _Phase = EPhase::Start;
        double _Deadline = FPlatformTime::Seconds() + 5.0;
    };

    class FCk_Latent_MultiSelectionSupersession final : public IAutomationLatentCommand
    {
    public:
        explicit FCk_Latent_MultiSelectionSupersession(FAutomationTestBase* InTest)
            : _Test(InTest)
            , _Gate(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
        {}

        virtual auto Update() -> bool override
        {
            if (FPlatformTime::Seconds() > _Deadline)
            {
                (*_Gate)->Trigger();
                _Test->AddError(TEXT("Multi-selection frame request queue test timed out."));
                return true;
            }

            switch (_Phase)
            {
            case EPhase::Start:
                (*_Gate)->Reset();
                _Queue.Request(60, [Gate = _Gate](const FCancelToken&)
                {
                    (*Gate)->Wait();
                    return MakeDetails(60);
                });
                _Queue.Request(70, [](const FCancelToken&) { return MakeMultiDetails(70); });
                _Queue.Request(80, [](const FCancelToken&) { return MakeDetails(80); });
                (*_Gate)->Trigger();
                _Phase = EPhase::AwaitSingleAfterMulti;
                return false;

            case EPhase::AwaitSingleAfterMulti:
                if (const auto Details = _Queue.Poll())
                {
                    _Test->TestEqual(TEXT("A newer single frame supersedes a pending multi-frame request"),
                        Details->Result.FrameIndex, uint64{80});
                    _Test->TestFalse(TEXT("The delivered newest request carries no multi-frame payload"),
                        Details->MultiStats.IsSet());
                    (*_Gate)->Reset();
                    _Queue.Request(90, [Gate = _Gate](const FCancelToken&)
                    {
                        (*Gate)->Wait();
                        return MakeMultiDetails(90);
                    });
                    _Phase = EPhase::ClearMulti;
                }
                return false;

            case EPhase::ClearMulti:
                _Queue.Reset();
                (*_Gate)->Trigger();
                _Phase = EPhase::AwaitClearedMulti;
                return false;

            case EPhase::AwaitClearedMulti:
                if (_Queue.IsBusy())
                {
                    _Test->TestFalse(TEXT("Clearing rejects a late multi-frame result"), _Queue.Poll().IsSet());
                    return false;
                }

                _Test->TestFalse(TEXT("A cleared multi-frame result never reaches the owner"), _Queue.Poll().IsSet());
                return true;
            }

            return true;
        }

    private:
        enum class EPhase : uint8
        {
            Start,
            AwaitSingleAfterMulti,
            ClearMulti,
            AwaitClearedMulti,
        };

        FAutomationTestBase* _Test = nullptr;
        FCkInsightsFrameRequestQueue _Queue;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _Gate;
        EPhase _Phase = EPhase::Start;
        double _Deadline = FPlatformTime::Seconds() + 5.0;
    };

    class FCk_Latent_ProgressiveFrameRequestQueue final : public IAutomationLatentCommand
    {
    public:
        explicit FCk_Latent_ProgressiveFrameRequestQueue(FAutomationTestBase* InTest)
            : _Test(InTest)
            , _FirstPublished(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
            , _AllowSecond(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
            , _SecondPublished(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
            , _AllowFinal(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
            , _CancelPublished(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
            , _AllowCancelledFinal(MakeShared<FEventRef, ESPMode::ThreadSafe>(EEventMode::ManualReset))
        {}

        virtual auto Update() -> bool override
        {
            if (FPlatformTime::Seconds() > _Deadline)
            {
                (*_AllowSecond)->Trigger();
                (*_AllowFinal)->Trigger();
                (*_AllowCancelledFinal)->Trigger();
                _Test->AddError(TEXT("Progressive frame request queue test timed out."));
                return true;
            }

            switch (_Phase)
            {
            case EPhase::Start:
                _Queue.RequestProgressive(100,
                    [FirstPublished = _FirstPublished, AllowSecond = _AllowSecond,
                        SecondPublished = _SecondPublished, AllowFinal = _AllowFinal]
                    (const FCancelToken&, const FCkInsightsFrameRequestQueue::FPublish& Publish)
                    {
                        Publish(MakeProgress(100, 1, 3));
                        (*FirstPublished)->Trigger();
                        (*AllowSecond)->Wait();
                        Publish(MakeProgress(100, 2, 3));
                        (*SecondPublished)->Trigger();
                        (*AllowFinal)->Wait();
                        return MakeProgress(100, 3, 3);
                    });
                _Phase = EPhase::AwaitFirstPublication;
                return false;

            case EPhase::AwaitFirstPublication:
                if ((*_FirstPublished)->Wait(0))
                {
                    (*_AllowSecond)->Trigger();
                    _Phase = EPhase::AwaitSecondPublication;
                }
                return false;

            case EPhase::AwaitSecondPublication:
                if ((*_SecondPublished)->Wait(0))
                {
                    if (const auto Details = _Queue.Poll())
                    {
                        _Test->TestTrue(TEXT("Latest progress is marked intermediate"), Details->IsIntermediate);
                        _Test->TestEqual(TEXT("Unconsumed progress is bounded to the latest publication"),
                            Details->ProcessedFrames, uint64{2});
                        _Test->TestEqual(TEXT("Progress keeps the requested frame count"),
                            Details->RequestedFrames, uint64{3});
                        (*_AllowFinal)->Trigger();
                        _Phase = EPhase::AwaitFinal;
                    }
                }
                return false;

            case EPhase::AwaitFinal:
                if (const auto Details = _Queue.Poll())
                {
                    _Test->TestFalse(TEXT("A ready final result wins over any mailbox progress"), Details->IsIntermediate);
                    _Test->TestEqual(TEXT("Final result is delivered after progress"), Details->ProcessedFrames, uint64{3});
                    _Queue.RequestProgressive(200,
                        [CancelPublished = _CancelPublished, AllowFinal = _AllowCancelledFinal]
                        (const FCancelToken&, const FCkInsightsFrameRequestQueue::FPublish& Publish)
                        {
                            Publish(MakeProgress(200, 1, 2));
                            (*CancelPublished)->Trigger();
                            (*AllowFinal)->Wait();
                            return MakeProgress(200, 2, 2);
                        });
                    _Phase = EPhase::AwaitCancelledPublication;
                }
                return false;

            case EPhase::AwaitCancelledPublication:
                if ((*_CancelPublished)->Wait(0))
                {
                    _Queue.Reset();
                    (*_AllowCancelledFinal)->Trigger();
                    _Phase = EPhase::AwaitCancelledCompletion;
                }
                return false;

            case EPhase::AwaitCancelledCompletion:
                _Test->TestFalse(TEXT("Cancellation rejects stale progress and stale final data"), _Queue.Poll().IsSet());
                if (NOT _Queue.IsBusy())
                {
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
            AwaitFirstPublication,
            AwaitSecondPublication,
            AwaitFinal,
            AwaitCancelledPublication,
            AwaitCancelledCompletion,
        };

        FAutomationTestBase* _Test = nullptr;
        FCkInsightsFrameRequestQueue _Queue;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _FirstPublished;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _AllowSecond;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _SecondPublished;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _AllowFinal;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _CancelPublished;
        TSharedPtr<FEventRef, ESPMode::ThreadSafe> _AllowCancelledFinal;
        EPhase _Phase = EPhase::Start;
        double _Deadline = FPlatformTime::Seconds() + 5.0;
    };
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsFrameRequestQueue_CoordinatesLatestFrame,
    "Ck.InsightsDebugger.FrameRequest.CoordinatesLatestFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsFrameRequestQueue_CoordinatesLatestFrame::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(
        ck_insights_frame_request_spec::FCk_Latent_FrameRequestQueue(this));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsFrameRequestQueue_MultiSelectionSupersession,
    "Ck.InsightsDebugger.FrameRequest.MultiSelectionSupersession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsFrameRequestQueue_MultiSelectionSupersession::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(
        ck_insights_frame_request_spec::FCk_Latent_MultiSelectionSupersession(this));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsFrameRequestQueue_PublishesIntermediateDetails,
    "Ck.InsightsDebugger.FrameRequest.PublishesIntermediateDetails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInsightsFrameRequestQueue_PublishesIntermediateDetails::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(
        ck_insights_frame_request_spec::FCk_Latent_ProgressiveFrameRequestQueue(this));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
