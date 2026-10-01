#include "CkInsightsDebugger_Module.h"

#include "CkInsightsDebugger/Window/SCkInsightsAnalyzerTab.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "Trace/Trace.h"
#include "Widgets/SWindow.h"
#include "Widgets/SWidget.h"

// --------------------------------------------------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_insights_recording_presentation_tests
{
    constexpr auto RetainedResultsTag = TEXT("InsightsAnalyzer.RetainedResults");

    auto FindWidgetWithTag(const TSharedRef<SWidget>& InWidget, FName InTag) -> TSharedPtr<SWidget>
    {
        if (InWidget->GetTag() == InTag)
        { return InWidget; }

        auto* Children = InWidget->GetChildren();
        for (auto ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
        {
            const auto Found = FindWidgetWithTag(Children->GetChildAt(ChildIndex), InTag);
            if (Found.IsValid())
            { return Found; }
        }

        return {};
    }

    class FCk_Latent_RecordingPresentation final : public IAutomationLatentCommand
    {
    public:
        explicit FCk_Latent_RecordingPresentation(FAutomationTestBase* InTest)
            : _Test(InTest)
            , _Capture(&FCkInsightsDebuggerModule::Get().Get_CaptureController())
        {}

        virtual ~FCk_Latent_RecordingPresentation() override
        {
            CloseTabWindow();
            _RetainedChild.Reset();
            _RetainedResults.Reset();
            _Tab.Reset();

            if (_StartedTestCapture && _Capture && _Capture->Get_Snapshot().bIsOwnedByTool)
            {
                auto Error = FString{};
                _Capture->TrySet_Tracing(false, Error);
            }

            if (_StartedTestCapture && _Capture)
            {
                auto CompletedPath = FString{};
                auto CompletedGuid = FGuid{};
                if (_Capture->Get_CompletedCapture(CompletedPath, CompletedGuid))
                { _Capture->Acknowledge_CompletedCapture(CompletedGuid); }
            }

            if (_ParkedModuleTab)
            {
                if (NOT FSlateApplication::IsInitialized())
                { _Test->AddError(TEXT("Cannot restore the parked Insights Analyzer tab after Slate shutdown.")); }
                else
                {
                    auto& Module = FCkInsightsDebuggerModule::Get();
                    Module.OpenDebugger();
                    _Test->TestTrue(TEXT("Recording presentation restores the preexisting Insights Analyzer tab"),
                        Module.IsDebuggerOpen());
                }
            }
        }

        virtual auto Update() -> bool override
        {
#if UE_TRACE_ENABLED
            const auto Now = FPlatformTime::Seconds();
            switch (_Phase)
            {
                case EPhase::StartEarlyStop:
                {
                    if (UE::Trace::IsTracing())
                    {
                        _Test->AddError(TEXT("The recording presentation test requires no existing active trace."));
                        return true;
                    }
                    auto ExistingCompletionPath = FString{};
                    auto ExistingCompletionGuid = FGuid{};
                    if (_Capture->Get_CompletedCapture(ExistingCompletionPath, ExistingCompletionGuid))
                    {
                        _Test->AddError(TEXT("The recording presentation test requires no pending Insights capture completion."));
                        return true;
                    }
                    if (NOT FSlateApplication::IsInitialized())
                    {
                        _Test->AddError(TEXT("Recording presentation requires Slate before parking the module tab."));
                        return true;
                    }

                    auto& Module = FCkInsightsDebuggerModule::Get();
                    if (Module.IsDebuggerOpen())
                    {
                        Module.CloseDebugger();
                        if (NOT _Test->TestFalse(TEXT("Recording presentation parks the preexisting Insights Analyzer tab"),
                            Module.IsDebuggerOpen()))
                        { return true; }
                        _ParkedModuleTab = true;
                    }

                    if (NOT OpenTabAndFindRetainedResults())
                    { return true; }

                    _Test->TestTrue(
                        TEXT("Retained results are visible before capture"),
                        _RetainedResults->GetVisibility() == EVisibility::Visible);
                    if (NOT CaptureRetainedChild())
                    { return true; }

                    auto Error = FString{};
                    const auto Started = _Capture->TryStart_TimedCapture(5.0, 0, Error);
                    _Test->TestTrue(*FString::Printf(TEXT("Timed capture starts: %s"), *Error), Started);
                    if (NOT Started)
                    { return true; }
                    _StartedTestCapture = true;

                    _Deadline = Now + TimeoutSeconds;
                    _Phase = EPhase::AwaitEarlyStopRecording;
                    return false;
                }

                case EPhase::AwaitEarlyStopRecording:
                {
                    if (_Capture->Get_Snapshot().State != ECkInsightsCaptureState::Recording)
                    {
                        if (Now < _Deadline)
                        { return false; }

                        _Test->AddError(TEXT("Timed capture did not become ready for the early-stop presentation check."));
                        StopTraceIfNeeded();
                        return true;
                    }

                    RepassTab();
                    _Test->TestTrue(
                        TEXT("Recording collapses the tagged retained-results subtree"),
                        _RetainedResults->GetVisibility() == EVisibility::Collapsed);
                    _Test->TestTrue(
                        TEXT("Recording keeps the existing retained-results child"),
                        GetOnlyRetainedChild() == _RetainedChild);

                    auto Error = FString{};
                    const auto Stopped = _Capture->TryStop_TimedCapture(Error);
                    _Test->TestTrue(*FString::Printf(TEXT("Timed capture stops early: %s"), *Error), Stopped);
                    if (NOT Stopped)
                    {
                        StopTraceIfNeeded();
                        return true;
                    }

                    auto CompletedPath = FString{};
                    auto CompletedGuid = FGuid{};
                    const auto HasCompletion = _Capture->Get_CompletedCapture(CompletedPath, CompletedGuid);
                    _Test->TestTrue(TEXT("Early stop records a completion before the tab can auto-open it"), HasCompletion);
                    if (HasCompletion)
                    {
                        _Test->TestTrue(
                            TEXT("The test clears only its own early-stop completion record"),
                            _Capture->Acknowledge_CompletedCapture(CompletedGuid));
                    }

                    RepassTab();
                    _Test->TestTrue(
                        TEXT("Early stop restores the retained-results subtree"),
                        _RetainedResults->GetVisibility() == EVisibility::Visible);
                    _Test->TestTrue(
                        TEXT("Early stop preserves the retained-results child identity"),
                        GetOnlyRetainedChild() == _RetainedChild);

                    _RetainedChild.Reset();
                    _RetainedResults.Reset();
                    CloseTabWindow();
                    _Tab.Reset();
                    _Deadline = Now + TimeoutSeconds;
                    _Phase = EPhase::StartCloseDuringCapture;
                    return false;
                }

                case EPhase::StartCloseDuringCapture:
                {
                    if (UE::Trace::IsTracing())
                    {
                        // Stop queues writer shutdown; the worker may need more than one frame to drain.
                        if (Now < _Deadline)
                        { return false; }

                        _Test->AddError(TEXT("Early-stop trace writer did not finish before timeout."));
                        StopTraceIfNeeded();
                        return true;
                    }

                    if (NOT OpenTabAndFindRetainedResults())
                    { return true; }

                    auto Error = FString{};
                    const auto Started = _Capture->TryStart_TimedCapture(2.0, 0, Error);
                    _Test->TestTrue(*FString::Printf(TEXT("Timed capture starts before closing its tab: %s"), *Error), Started);
                    if (NOT Started)
                    { return true; }
                    _StartedTestCapture = true;

                    _Deadline = Now + TimeoutSeconds;
                    _Phase = EPhase::AwaitCloseDuringCaptureRecording;
                    return false;
                }

                case EPhase::AwaitCloseDuringCaptureRecording:
                {
                    if (_Capture->Get_Snapshot().State != ECkInsightsCaptureState::Recording)
                    {
                        if (Now < _Deadline)
                        { return false; }

                        _Test->AddError(TEXT("Timed capture did not become ready before tab-close recovery check."));
                        StopTraceIfNeeded();
                        return true;
                    }

                    RepassTab();
                    _Test->TestTrue(
                        TEXT("The first tab collapses retained results during recording"),
                        _RetainedResults->GetVisibility() == EVisibility::Collapsed);

                    _ClosedTab = _Tab;
                    _RetainedResults.Reset();
                    CloseTabWindow();
                    _Tab.Reset();
                    _Test->TestFalse(TEXT("Closing a tab releases its Slate instance"), _ClosedTab.IsValid());

                    if (NOT OpenTabAndFindRetainedResults())
                    {
                        StopTraceIfNeeded();
                        return true;
                    }

                    RepassTab();
                    _Test->TestTrue(
                        TEXT("A reopened tab observes the module-owned capture and remains collapsed"),
                        _RetainedResults->GetVisibility() == EVisibility::Collapsed);

                    _RetainedResults.Reset();
                    CloseTabWindow();
                    _Tab.Reset();
                    _Deadline = Now + TimeoutSeconds;
                    _Phase = EPhase::AwaitClosedCaptureCompletion;
                    return false;
                }

                case EPhase::AwaitClosedCaptureCompletion:
                {
                    if (UE::Trace::IsTracing())
                    {
                        if (Now < _Deadline)
                        { return false; }

                        _Test->AddError(TEXT("Timed capture did not finish after all tabs were closed."));
                        StopTraceIfNeeded();
                        return true;
                    }

                    auto CompletedPath = FString{};
                    auto CompletedGuid = FGuid{};
                    const auto HasCompletion = _Capture->Get_CompletedCapture(CompletedPath, CompletedGuid);
                    _Test->TestTrue(TEXT("A tab-close capture remains recoverable through the module controller"), HasCompletion);
                    if (HasCompletion)
                    {
                        if (OpenTabAndFindRetainedResults())
                        {
                            RepassTab();
                            _Test->TestTrue(
                                TEXT("A tab reopened after automatic completion restores retained results"),
                                _RetainedResults->GetVisibility() == EVisibility::Visible);
                            _RetainedResults.Reset();
                            CloseTabWindow();
                            _Tab.Reset();
                        }
                        _Test->TestTrue(
                            TEXT("The test clears only its own tab-close completion record"),
                            _Capture->Acknowledge_CompletedCapture(CompletedGuid));
                    }
                    return true;
                }
            }
#endif
            return true;
        }

    private:
        auto OpenTabAndFindRetainedResults() -> bool
        {
            if (NOT FSlateApplication::IsInitialized())
            {
                _Test->AddError(TEXT("Recording presentation requires Slate before mounting the Insights Analyzer tab."));
                return false;
            }
            _Tab = SNew(SCkInsightsAnalyzerTab);
            _HostWindow = SNew(SWindow)
                .AutoCenter(EAutoCenter::None)
                .ClientSize(FVector2D{1120.0f, 700.0f})
                .CreateTitleBar(false)
                .HasCloseButton(false)
                [
                    _Tab.ToSharedRef()
                ];
            FSlateApplication::Get().AddWindow(_HostWindow.ToSharedRef(), true);
            RepassTab();
            _RetainedResults = FindWidgetWithTag(_Tab.ToSharedRef(), FName{RetainedResultsTag});
            _Test->TestNotNull(TEXT("Insights Analyzer exposes its tagged retained-results subtree"), _RetainedResults.Get());
            return _RetainedResults.IsValid();
        }

        auto CaptureRetainedChild() -> bool
        {
            _RetainedChild = GetOnlyRetainedChild();
            return _RetainedChild.IsValid();
        }

        auto GetOnlyRetainedChild() const -> TSharedPtr<SWidget>
        {
            auto* Children = _RetainedResults->GetChildren();
            _Test->TestEqual(TEXT("Retained results have one content child"), Children->Num(), 1);
            if (Children->Num() != 1)
            { return {}; }

            return Children->GetChildAt(0);
        }

        auto RepassTab() const -> void
        {
            if (NOT FSlateApplication::IsInitialized())
            {
                _Test->AddError(TEXT("Recording presentation cannot tick the mounted tab after Slate shutdown."));
                return;
            }
            FSlateApplication& Slate = FSlateApplication::Get();
            Slate.PumpMessages();
            Slate.Tick();
        }

        auto CloseTabWindow() -> void
        {
            if (NOT _HostWindow.IsValid()) { return; }
            if (NOT FSlateApplication::IsInitialized())
            {
                _Test->AddError(TEXT("Recording presentation cannot destroy its mounted window after Slate shutdown."));
                _HostWindow.Reset();
                return;
            }
            FSlateApplication& Slate = FSlateApplication::Get();
            Slate.DestroyWindowImmediately(_HostWindow.ToSharedRef());
            _HostWindow.Reset();
            Slate.PumpMessages();
            Slate.Tick();
        }

        auto StopTraceIfNeeded() const -> void
        {
            if (_Capture->Get_Snapshot().bIsOwnedByTool)
            {
                auto Error = FString{};
                _Capture->TrySet_Tracing(false, Error);
            }
        }

        enum class EPhase : uint8
        {
            StartEarlyStop,
            AwaitEarlyStopRecording,
            StartCloseDuringCapture,
            AwaitCloseDuringCaptureRecording,
            AwaitClosedCaptureCompletion,
        };

        static constexpr double TimeoutSeconds = 10.0;

        FAutomationTestBase* _Test = nullptr;
        FCkInsightsCaptureController* _Capture = nullptr;
        TSharedPtr<SWindow> _HostWindow;
        TSharedPtr<SCkInsightsAnalyzerTab> _Tab;
        TWeakPtr<SCkInsightsAnalyzerTab> _ClosedTab;
        TSharedPtr<SWidget> _RetainedResults;
        TSharedPtr<SWidget> _RetainedChild;
        bool _StartedTestCapture = false;
        bool _ParkedModuleTab = false;
        EPhase _Phase = EPhase::StartEarlyStop;
        double _Deadline = 0.0;
    };
}

// --------------------------------------------------------------------------------------------------------------------

using ck_insights_recording_presentation_tests::FCk_Latent_RecordingPresentation;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkInsightsRecordingPresentation,
    "Ck.InsightsDebugger.Capture.RecordingPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

auto FCkInsightsRecordingPresentation::RunTest(const FString&) -> bool
{
    ADD_LATENT_AUTOMATION_COMMAND(FCk_Latent_RecordingPresentation(this));
    return true;
}

#endif
